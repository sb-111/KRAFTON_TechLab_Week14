// Depth of Field - CoC Visualization Pass
// Visualizes Circle of Confusion for debugging

Texture2D g_SceneTexture : register(t0);      // Full res 원본 (미사용이지만 슬롯 맞춤)
Texture2D g_BlurredTexture : register(t1);    // Half res 흐림 (미사용이지만 슬롯 맞춤)
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

    float2 BlurDirection;
    float2 TexelSize;
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

// CoC 계산
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
    float depth = g_DepthTexture.Sample(g_PointSampler, input.texCoord).r;
    float linearDepth = LinearDepth(depth);
    float coc = CalculateCoC(linearDepth);

    // CoC 시각화 with color coding
    // Green tint: 포그라운드 흐림 (Near)
    // Red tint: 백그라운드 흐림 (Far)
    // Blue: 선명 (Focused)

    float nearStart = FocalDistance - NearTransitionRange;
    float nearBlur = saturate((nearStart - linearDepth) / max(NearTransitionRange, 0.01f));

    float farBlur = saturate((linearDepth - FocalDistance) / max(FarTransitionRange, 0.01f));

    float3 color;
    if (nearBlur > 0.01f)
    {
        // 포그라운드 흐림: 초록색
        color = float3(0.0f, nearBlur, 0.0f);
    }
    else if (farBlur > 0.01f)
    {
        // 백그라운드 흐림: 빨간색
        color = float3(farBlur, 0.0f, 0.0f);
    }
    else
    {
        // 선명한 영역: 파란색
        color = float3(0.0f, 0.0f, 1.0f);
    }

    return float4(color, 1.0f);
}
