// Depth of Field - CoC Visualization Pass
// Visualizes Circle of Confusion for debugging

Texture2D g_DepthTexture : register(t0);
SamplerState g_PointSampler : register(s1);

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

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// 선형 깊이 변환 (DownsampleCoC_PS.hlsl과 동일)
float LinearDepth(float zBufferDepth)
{
    if (IsOrthographic == 1)
    {
        return ProjectionAB.x + zBufferDepth * (ProjectionAB.y - ProjectionAB.x);
    }
    else
    {
        return ProjectionAB.x * ProjectionAB.y / (ProjectionAB.y - zBufferDepth * (ProjectionAB.y - ProjectionAB.x));
    }
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    // 깊이 샘플링
    float depth = g_DepthTexture.Sample(g_PointSampler, input.texCoord).r;
    float linearDepth = LinearDepth(depth);

    // CoC 계산 (전경/배경 블러 분리)
    float nearStart = FocalDistance - NearTransitionRange;
    float nearBlur = saturate((nearStart - linearDepth) / max(NearTransitionRange, 0.01f));

    float farBlur = saturate((linearDepth - FocalDistance) / max(FarTransitionRange, 0.01f));

    float totalBlur = saturate(nearBlur + farBlur);

    // 초점 영역 계산 (블러가 없을수록 높은 값)
    float inFocus = 1.0f - totalBlur;

    // 컬러 시각화:
    // R (빨강) = 배경 블러 (포커스보다 먼 곳)
    // G (초록) = 전경 블러 (포커스보다 가까운 곳)
    // B (파랑) = 초점 영역 (선명한 곳)
    return float4(farBlur, nearBlur, inFocus, 1.0f);
}
