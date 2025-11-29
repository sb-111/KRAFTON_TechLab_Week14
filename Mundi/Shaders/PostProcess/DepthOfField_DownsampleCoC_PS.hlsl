// Depth of Field - Downsample + CoC Calculation Pass
// Downsamples scene color to 1/2 resolution and calculates Circle of Confusion

Texture2D g_SceneTexture : register(t0);
Texture2D g_DepthTexture : register(t1);

SamplerState g_LinearSampler : register(s0);
SamplerState g_PointSampler : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer DOFParametersCB : register(b2)
{
    float FocalDistance;           // 포커스 중심 거리 (월드 단위)
    float NearTransitionRange;     // 포그라운드 전환 범위
    float FarTransitionRange;      // 백그라운드 전환 범위
    float MaxCoCRadius;            // 최대 CoC 반경 (픽셀 단위)

    float2 ProjectionAB;           // 깊이 복원용 (Near/Far)
    int IsOrthographic;            // 0 = Perspective, 1 = Orthographic
    float Padding;

    float2 BlurDirection;          // Blur pass용 (이 pass에서는 미사용)
    float2 TexelSize;              // Blur pass용 (이 pass에서는 미사용)
};

// 선형 깊이 변환
float LinearDepth(float zBufferDepth)
{
    if (IsOrthographic == 1)
    {
        // 정사영: 선형 변환
        return ProjectionAB.x + zBufferDepth * (ProjectionAB.y - ProjectionAB.x);
    }
    else
    {
        // 원근: 역 프로젝션
        // ProjectionAB.x = Far / (Far - Near)
        // ProjectionAB.y = (-Far * Near) / (Far - Near)
        return ProjectionAB.y / (zBufferDepth - ProjectionAB.x);
    }
}

// CoC 계산 (Piecewise Linear)
float CalculateCoC(float depth)
{
    // Near focus range: [FocalDistance - NearTransitionRange, FocalDistance]
    // 포그라운드 흐림: 포커스 거리보다 가까운 곳
    float nearStart = FocalDistance - NearTransitionRange;
    float nearBlur = saturate((nearStart - depth) / max(NearTransitionRange, 0.01f));

    // Far focus range: [FocalDistance, FocalDistance + FarTransitionRange]
    // 백그라운드 흐림: 포커스 거리보다 먼 곳
    float farEnd = FocalDistance + FarTransitionRange;
    float farBlur = saturate((depth - FocalDistance) / max(FarTransitionRange, 0.01f));

    // 합성 (0.0 = 선명, 1.0 = 최대 흐림)
    float blur = saturate(nearBlur + farBlur);

    return blur;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    float2 uv = input.texCoord;

    // 4개 픽셀 샘플링 (Box filter downsampling)
    // Full resolution에서 1/2 resolution으로 다운샘플
    // 현재 픽셀은 Half Res이므로, Full Res에서 2x2 영역을 샘플링

    // Full Res 텍스처 크기 계산을 위한 변수
    uint fullWidth, fullHeight;
    g_SceneTexture.GetDimensions(fullWidth, fullHeight);
    float2 fullTexelSize = float2(1.0f / fullWidth, 1.0f / fullHeight);

    float3 color = 0.0f;
    float cocSum = 0.0f;

    // 2x2 Box filter: 4개 픽셀을 평균내어 다운샘플
    // UV는 Half Res 기준이므로, Full Res에서는 4개 픽셀이 대응됨
    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 2; ++x)
        {
            float2 offset = float2(x, y) * fullTexelSize;
            float2 sampleUV = uv + offset;

            // 색상 샘플링 (Linear filter)
            color += g_SceneTexture.Sample(g_LinearSampler, sampleUV).rgb;

            // 깊이 샘플링 (Point filter - 보간 불필요)
            float depth = g_DepthTexture.Sample(g_PointSampler, sampleUV).r;
            float linearDepth = LinearDepth(depth);

            // CoC 계산
            cocSum += CalculateCoC(linearDepth);
        }
    }

    // 평균 계산
    color /= 4.0f;
    float coc = cocSum / 4.0f;

    // RGB: Downsampled Color, A: Circle of Confusion (0~1)
    return float4(color, coc);
}
