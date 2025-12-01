// Poisson Disk 기반 Bokeh 컴퓨트 셰이더 (HLSL)
// - 변수명 명확화: accumulatedColor, accumulatedWeight, sampleCount 등
// - CoC에 따라 샘플 수 동적으로 증가
// - 각 샘플 Weight는 radial Gaussian으로 계산 (수식 및 주석 포함)
// - 중심 샘플 포함, wsum fallback, poisson 방향 정규화 등 안정성 포함

Texture2D<float4> g_InputTexture : register(t0);     // RGBA: RGB=color, A=CoC (0..1 정규화)
RWTexture2D<float4> g_OutputTexture : register(u0);

SamplerState g_LinearSampler : register(s0);

cbuffer DOFParametersCB : register(b2)
{
    float FocalDistance;
    float NearTransitionRange;
    float FarTransitionRange;
    float MaxCoCRadius;      // CoC=1일 때 최대 블러 반경(픽셀 단위)

    float2 ProjectionAB;
    int IsOrthographic;
    float Padding;

    float2 BlurDirection;    // (disk gather에서는 사용 안함, 호환성 유지)
    float2 TexelSize;        // (1/width, 1/height)
};

// Poisson-ish 방향 배열 (방향 + 약간의 반경 jitter 포함)
// 길이가 1이 아님 → dirUnit과 dirLenNorm을 사용
static const int POISSON_COUNT = 16;
static const float2 poissonDisk[POISSON_COUNT] = {
    float2(-0.94201624, -0.39906216),
    float2(0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870),
    float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845),
    float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554),
    float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023),
    float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507),
    float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367),
    float2(0.14383161, -0.14100790)
};

// poissonDisk 항목의 최대 길이 (정규화용)
static const float POISSON_MAX_LEN = 1.23393213;

// -----------------------
// 반경 가우시안 Weight 함수
// -----------------------
// r: 0(center)~1(outer radius) 범위의 정규화된 반경
// sigma: 표준편차, CoC 기반으로 동적 결정
// w(r) = exp( - (r^2) / (2*sigma^2) )
// 정규화 후 합=1로 밝기 유지
float RadialGaussianWeight(float r, float sigma)
{
    float s = max(sigma, 1e-6f); // sigma >0 방어
    return exp(- (r * r) / (2.0f * s * s));
}

// -----------------------
// 컴퓨트 셰이더 진입점
// -----------------------
[numthreads(8,8,1)]
void mainCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelPos = dispatchThreadID.xy;

    // 텍스처 범위 확인
    uint width, height;
    g_InputTexture.GetDimensions(width, height);
    if (pixelPos.x >= width || pixelPos.y >= height)
        return;

    // 정수 픽셀 위치 -> UV 좌표 (픽셀 중심)
    float2 uv = (pixelPos + 0.5f) / float2(width, height);

    // 중심 샘플 읽기 (RGBA, A=CoC 0..1)
    float4 centerSample = g_InputTexture.SampleLevel(g_LinearSampler, uv, 0);
    float centerCoC = centerSample.a;

    // CoC가 거의 없으면 블러 스킵
    if (centerCoC < 1e-3f)
    {
        g_OutputTexture[pixelPos] = centerSample;
        return;
    }

    // -----------------------
    // 동적 샘플 수 계산
    // -----------------------
    // centerCoC가 클수록 샘플 수 증가 (품질 유지)
    // sampleCount = round( lerp(minSamples, maxSamples, pow(centerCoC, sampleBiasPower)) )
    const int minSamples = 8;      // 최소 샘플
    const int maxSamples = 64;     // 최대 샘플

    float t = centerCoC;
    int sampleCount = (int)round( lerp((float)minSamples, (float)maxSamples, t) );
    sampleCount = clamp(sampleCount, 1, maxSamples);

    // -----------------------
    // 픽셀 반경 및 가우시안 sigma 결정
    // -----------------------
    float radiusPixels = centerCoC * MaxCoCRadius; // 픽셀 단위 블러 반경
    float sigma = 0.5f * saturate(centerCoC);     // sigma: CoC 비례, 0~0.5 조절 가능

    // -----------------------
    // 누적 변수 초기화
    // -----------------------
    float3 accumulatedColor = float3(0.0f,0.0f,0.0f);
    float accumulatedWeight = 0.0f;

    // 중심 샘플 포함 (밝기 유지)
    float centerWeight = RadialGaussianWeight(0.0f, sigma);
    accumulatedColor += centerSample.rgb * centerWeight;
    accumulatedWeight += centerWeight;

    // -----------------------
    // 샘플 루프: 디스크 채우기
    // -----------------------
    for (int si=0; si<sampleCount; ++si)
    {
        // Poisson 기반 방향 선택, 단위벡터로 변환
        float2 baseDir = poissonDisk[si % POISSON_COUNT];
        float baseDirLen = max(length(baseDir), 1e-6f);
        float2 dirUnit = baseDir / baseDirLen;

        // radialFactor: 0..1 범위로 디스크 내 샘플 위치 결정
        float radialFactor = ((float)si + 0.5f) / (float)sampleCount;

        // UV 오프셋 계산
        float2 offsetUV = dirUnit * (radialFactor * radiusPixels) * TexelSize;
        float2 sampleUV = uv + offsetUV;

        // 텍스처 샘플링
        float3 sampleColor = g_InputTexture.SampleLevel(g_LinearSampler, sampleUV, 0).rgb;

        // 정규화된 반경 r=0..1
        float rNorm = saturate(radialFactor);

        // 가중치 계산
        float sampleWeight = RadialGaussianWeight(rNorm, sigma);

        // 누적
        accumulatedColor += sampleColor * sampleWeight;
        accumulatedWeight += sampleWeight;
    }

    // 안전장치: weight 합이 거의 0이면 중심 샘플로 폴백
    if (accumulatedWeight <= 1e-6f)
    {
        g_OutputTexture[pixelPos] = centerSample;
        return;
    }

    // 누적 컬러 정규화
    float3 blurredColor = accumulatedColor / accumulatedWeight;

    // 출력 (RGB=블러 컬러, A=CoC 유지)
    g_OutputTexture[pixelPos] = float4(blurredColor, centerCoC);
}
