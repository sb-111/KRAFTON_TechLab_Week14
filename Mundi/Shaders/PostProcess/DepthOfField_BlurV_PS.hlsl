// Depth of Field - Vertical Blur Pass
// Applies separable Gaussian blur in vertical direction with CoC-based weighting

Texture2D g_InputTexture : register(t0);

SamplerState g_LinearSampler : register(s0);

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
    float MaxCoCRadius;            // 최대 CoC 반경 (픽셀 단위)

    float2 ProjectionAB;           // (이 pass에서는 미사용)
    int IsOrthographic;
    float Padding;

    float2 BlurDirection;          // (0, 1) for vertical
    float2 TexelSize;              // 1 / HalfResSize
};

// 9-tap Gaussian weights (sigma ≈ 2.0)
static const int KernelRadius = 4;
static const float Weights[9] = {
    0.0162, 0.0540, 0.1216, 0.1945, 0.2270,
    0.1945, 0.1216, 0.0540, 0.0162
};

float4 mainPS(PS_INPUT input) : SV_Target
{
    float2 uv = input.texCoord;
    float4 centerSample = g_InputTexture.Sample(g_LinearSampler, uv);
    float centerCoC = centerSample.a;

    // CoC가 거의 없으면 조기 종료 (성능 최적화)
    if (centerCoC < 0.01f)
    {
        return centerSample;
    }

    float3 colorSum = 0.0f;
    float weightSum = 0.0f;

    // 9-tap 샘플링 (-4 ~ +4)
    for (int i = -KernelRadius; i <= KernelRadius; ++i)
    {
        // CoC에 비례하는 오프셋 계산
        // centerCoC가 클수록 더 넓은 범위를 샘플링
        float2 offset = BlurDirection * TexelSize * float(i) * centerCoC * MaxCoCRadius;
        float2 sampleUV = uv + offset;

        // 텍스처 샘플링
        float4 tapSample = g_InputTexture.Sample(g_LinearSampler, sampleUV);
        float tapCoC = tapSample.a;

        // CoC 기반 가중치
        // 현재 픽셀의 CoC보다 큰 CoC를 가진 샘플이 더 많이 기여
        // 이를 통해 포그라운드 객체가 배경으로 번지는 현상 방지
        float cocWeight = saturate(tapCoC - centerCoC + 1.0f);

        // 최종 가중치 = Gaussian weight × CoC weight
        float weight = Weights[i + KernelRadius] * cocWeight;

        colorSum += tapSample.rgb * weight;
        weightSum += weight;
    }

    // 정규화
    float3 blurredColor = colorSum / max(weightSum, 0.001f);

    // RGB: Blurred Color, A: CoC (유지)
    return float4(blurredColor, centerCoC);
}
