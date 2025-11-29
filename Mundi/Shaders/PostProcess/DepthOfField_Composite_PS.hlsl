// Depth of Field - Composite Pass
// Combines sharp and blurred images based on CoC

Texture2D g_SceneTexture : register(t0);      // Full res 원본 (선명)
Texture2D g_BlurredTexture : register(t1);    // Half res 흐림
Texture2D g_DepthTexture : register(t2);      // Full res 깊이

SamplerState g_LinearSampler : register(s0);
SamplerState g_PointSampler : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer DOFParametersCB : register(b2)
{
    float FocalDistance;
    float NearTransitionRange;
    float FarTransitionRange;
    float MaxCoCRadius;

    float2 ProjectionAB;           // 깊이 복원용
    int IsOrthographic;
    float Padding;

    float2 BlurDirection;          // (이 pass에서는 미사용)
    float2 TexelSize;              // (이 pass에서는 미사용)
};

// 선형 깊이 변환
float LinearDepth(float zBufferDepth)
{
    if (IsOrthographic == 1)
    {
        return ProjectionAB.x + zBufferDepth * (ProjectionAB.y - ProjectionAB.x);
    }
    else
    {
        return ProjectionAB.y / (zBufferDepth - ProjectionAB.x);
    }
}

// CoC 계산 (DownsampleCoC와 동일한 로직)
float CalculateCoC(float depth)
{
    float nearStart = FocalDistance - NearTransitionRange;
    float nearBlur = saturate((nearStart - depth) / max(NearTransitionRange, 0.01f));

    float farEnd = FocalDistance + FarTransitionRange;
    float farBlur = saturate((depth - FocalDistance) / max(FarTransitionRange, 0.01f));

    float blur = saturate(nearBlur + farBlur);

    return blur;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    float2 uv = input.texCoord;

    // 원본 색상 및 깊이 샘플링 (Full resolution)
    float3 sharpColor = g_SceneTexture.Sample(g_LinearSampler, uv).rgb;
    float depth = g_DepthTexture.Sample(g_PointSampler, uv).r;
    float linearDepth = LinearDepth(depth);

    // CoC 계산
    float coc = CalculateCoC(linearDepth);

    // 흐린 이미지 샘플링 (Half resolution → Bilinear upsampling)
    // Linear Sampler를 사용하여 자동으로 부드러운 업샘플링 수행
    float3 blurredColor = g_BlurredTexture.Sample(g_LinearSampler, uv).rgb;

    // CoC에 따라 선형 보간
    // CoC = 0: 완전히 선명 (sharpColor)
    // CoC = 1: 완전히 흐림 (blurredColor)
    float3 finalColor = lerp(sharpColor, blurredColor, coc);

    return float4(finalColor, 1.0f);
}
