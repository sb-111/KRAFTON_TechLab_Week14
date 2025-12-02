Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

SamplerState g_LinearSampler : register(s0);

cbuffer DOFParametersCB : register(b2)
{
    float FocalDistance;
    float NearTransitionRange;
    float FarTransitionRange;
    float MaxCoCRadius;

    float2 ProjectionAB;
    int IsOrthographic;
    float Padding;

    float2 BlurDirection;
    float2 TexelSize;
};

// 가우시안 가중치 계산 함수
float ComputeGaussianTabWeight(int offset, float sigma)
{
    return exp(-(offset * offset) / (2.0f * sigma * sigma));
}

// [기존 고정 Weights - 주석 처리]
// static const float Weights13[13] = {
//     0.0010, 0.0044, 0.0162, 0.0540, 0.1216, 0.1945, 0.2270,
//     0.1945, 0.1216, 0.0540, 0.0162, 0.0044, 0.0010
// };
// static const float Weights9[9] = {
//     0.0162, 0.0540, 0.1216, 0.1945, 0.2270,
//     0.1945, 0.1216, 0.0540, 0.0162
// };
// static const float Weights5[5] = {
//     0.0625, 0.25, 0.375, 0.25, 0.0625
// };
// static const float Weights3[3] = {
//     0.25, 0.5, 0.25
// };

[numthreads(8, 8, 1)]
void mainCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelPos = dispatchThreadID.xy;

    // 텍스처 크기 확인 (범위 체크)
    uint width, height;
    g_InputTexture.GetDimensions(width, height);
    if (pixelPos.x >= width || pixelPos.y >= height)
        return;

    float2 uv = (pixelPos + 0.5f) / float2(width, height);

    float4 centerSample = g_InputTexture.SampleLevel(g_LinearSampler, uv, 0);
    float centerCoC = centerSample.a;

    // 조기 종료 (CoC가 거의 없으면 블러 스킵)
    if (centerCoC < 0.01f)
    {
        g_OutputTexture[pixelPos] = centerSample;
        return;
    }

    // === 동적 가우시안 샘플링 ===

    float3 colorSum = 0.0f;
    float weightSum = 0.0f;

    // CoC에 따른 동적 샘플 수 (홀수로 보정)
    uint KernelRadius = uint(centerCoC * MaxCoCRadius);
    if (KernelRadius % 2 == 0)
        KernelRadius++;

    // 최소/최대 제한
    KernelRadius = clamp(KernelRadius, 1u, 31u);

    // Sigma 동적 계산 (CoC에 비례)
    float Sigma = centerCoC * MaxCoCRadius * 0.5f;
    Sigma = max(Sigma, 0.5f); // 최소값 보장

    // 가우시안 가중치 정규화를 위해 커널의 전체 가중치 총합 계산
    float WeightTotal = 0.0f;
    for (int i = -int(KernelRadius); i <= int(KernelRadius); i++)
    {
        WeightTotal += ComputeGaussianTabWeight(i, Sigma);
    }

    // 블러 샘플링
    for (int i = -int(KernelRadius); i <= int(KernelRadius); ++i)
    {
        // CoC에 비례하는 오프셋 계산
        float2 offset = BlurDirection * TexelSize * float(i) * centerCoC * MaxCoCRadius;
        float2 sampleUV = uv + offset;

        // 텍스처 샘플링
        float4 tapSample = g_InputTexture.SampleLevel(g_LinearSampler, sampleUV, 0);
        float tapCoC = tapSample.a;

        // CoC 기반 가중치 (포그라운드 객체가 배경으로 번지는 현상 방지)
        float cocWeight = saturate(tapCoC - centerCoC + 1.0f);

        // 최종 가중치 = Normalized Gaussian weight × CoC weight
        float NormalizedGaussianWeight = ComputeGaussianTabWeight(i, Sigma) / WeightTotal;
        float weight = NormalizedGaussianWeight * cocWeight;

        colorSum += tapSample.rgb * weight;
        weightSum += weight;
    }

    // 정규화
    float3 blurredColor = colorSum / max(weightSum, 0.001f);

    // RGB: Blurred Color, A: CoC (유지)
    g_OutputTexture[pixelPos] = float4(blurredColor, centerCoC);

    // === [기존 고정 샘플링 로직 - 주석 처리] ===
    // if (centerCoC < 0.05f)
    // {
    //     // kernelRadius = 1 (3-tap)
    //     for (int i = -1; i <= 1; ++i)
    //     {
    //         float2 offset = BlurDirection * TexelSize * float(i) * centerCoC * MaxCoCRadius;
    //         float2 sampleUV = uv + offset;
    //
    //         float4 tapSample = g_InputTexture.SampleLevel(g_LinearSampler, sampleUV, 0);
    //         float tapCoC = tapSample.a;
    //
    //         float cocWeight = saturate(tapCoC - centerCoC + 1.0f);
    //         float weight = Weights3[i + 1] * cocWeight;
    //
    //         colorSum += tapSample.rgb * weight;
    //         weightSum += weight;
    //     }
    // }
    // [... 나머지 else if 블록들도 주석 처리됨 ...]
}