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

// ViewportConstants (b10) - FullScreenTriangle_VS와 동일
cbuffer ViewportConstants : register(b10)
{
    float4 ViewportRect;  // x: MinX, y: MinY, z: Width, w: Height
    float4 ScreenSize;    // x: Width, y: Height, z: 1/Width, w: 1/Height
}

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

// 선형 깊이 변환 (SceneDepth_PS.hlsl과 동일한 방식)
// ProjectionAB.x = Near, ProjectionAB.y = Far
float LinearDepth(float zBufferDepth)
{
    if (IsOrthographic == 1)
    {
        return ProjectionAB.x + zBufferDepth * (ProjectionAB.y - ProjectionAB.x);
    }
    else
    {
        // SceneDepth_PS.hlsl과 동일한 공식
        return ProjectionAB.x * ProjectionAB.y / (ProjectionAB.y - zBufferDepth * (ProjectionAB.y - ProjectionAB.x));
    }
}

// CoC 계산 (DownsampleCoC와 동일한 로직)
float CalculateCoC(float depth)
{
    // Near focus range: [FocalDistance - NearTransitionRange, FocalDistance]
    // 포그라운드 흐림: 포커스 거리보다 가까운 곳
    float nearBlur = saturate((FocalDistance - depth) / max(NearTransitionRange, 0.01f));

    // Far focus range: [FocalDistance, FocalDistance + FarTransitionRange]
    // 백그라운드 흐림: 포커스 거리보다 먼 곳
    float farBlur = saturate((depth - FocalDistance) / max(FarTransitionRange, 0.01f));

    // 합성 (0.0 = 선명, 1.0 = 최대 흐림)
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

    // Half resolution 텍스처를 위한 UV 변환
    // input.texCoord는 ViewRect 영역의 UV ([ViewStartUV, ViewStartUV + ViewUVSpan] 범위)
    // Half resolution 텍스처는 [0,1]이 ViewRect 전체에 대응
    // 따라서 ViewRect UV → [0,1]로 변환 필요
    float2 ViewportStartUV = ViewportRect.xy * ScreenSize.zw;
    float2 ViewportUVSpan = ViewportRect.zw * ScreenSize.zw;
    float2 halfResUV = (uv - ViewportStartUV) / ViewportUVSpan;

    // 흐린 이미지 샘플링 (Half resolution → Bilinear upsampling)
    float3 blurredColor = g_BlurredTexture.Sample(g_LinearSampler, halfResUV).rgb;

    // CoC에 따라 선형 보간
    // CoC = 0: 완전히 선명 (sharpColor)
    // CoC = 1: 완전히 흐림 (blurredColor)
    float3 finalColor = lerp(sharpColor, blurredColor, coc);

    return float4(finalColor, 1.0f);
}
