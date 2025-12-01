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

// Gaussian weights (정규화된 값)
static const float Weights13[13] = {
    0.0010, 0.0044, 0.0162, 0.0540, 0.1216, 0.1945, 0.2270,
    0.1945, 0.1216, 0.0540, 0.0162, 0.0044, 0.0010
};
static const float Weights9[9] = {
    0.0162, 0.0540, 0.1216, 0.1945, 0.2270,
    0.1945, 0.1216, 0.0540, 0.0162
};
static const float Weights5[5] = {
    0.0625, 0.25, 0.375, 0.25, 0.0625
};
static const float Weights3[3] = {
    0.25, 0.5, 0.25
};

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

    // CoC 크기에 따라 샘플 수 및 블러 결정
    float3 colorSum = 0.0f;
    float weightSum = 0.0f;

    if (centerCoC < 0.05f)
    {
        // kernelRadius = 1 (3-tap)
        for (int i = -1; i <= 1; ++i)
        {
            float2 offset = BlurDirection * TexelSize * float(i) * centerCoC * MaxCoCRadius;
            float2 sampleUV = uv + offset;

            float4 tapSample = g_InputTexture.SampleLevel(g_LinearSampler, sampleUV, 0);
            float tapCoC = tapSample.a;

            float cocWeight = saturate(tapCoC - centerCoC + 1.0f);
            float weight = Weights3[i + 1] * cocWeight;

            colorSum += tapSample.rgb * weight;
            weightSum += weight;
        }
    }
    else if (centerCoC < 0.3f)
    {
        // kernelRadius = 2 (5-tap)
        for (int i = -2; i <= 2; ++i)
        {
            float2 offset = BlurDirection * TexelSize * float(i) * centerCoC * MaxCoCRadius;
            float2 sampleUV = uv + offset;

            float4 tapSample = g_InputTexture.SampleLevel(g_LinearSampler, sampleUV, 0);
            float tapCoC = tapSample.a;

            float cocWeight = saturate(tapCoC - centerCoC + 1.0f);
            float weight = Weights5[i + 2] * cocWeight;

            colorSum += tapSample.rgb * weight;
            weightSum += weight;
        }
    }
    else if (centerCoC < 0.7f)
    {
        // kernelRadius = 4 (9-tap)
        for (int i = -4; i <= 4; ++i)
        {
            float2 offset = BlurDirection * TexelSize * float(i) * centerCoC * MaxCoCRadius;
            float2 sampleUV = uv + offset;

            float4 tapSample = g_InputTexture.SampleLevel(g_LinearSampler, sampleUV, 0);
            float tapCoC = tapSample.a;

            float cocWeight = saturate(tapCoC - centerCoC + 1.0f);
            float weight = Weights9[i + 4] * cocWeight;

            colorSum += tapSample.rgb * weight;
            weightSum += weight;
        }
    }
    else
    {
        // kernelRadius = 6 (13-tap)
        for (int i = -6; i <= 6; ++i)
        {
            float2 offset = BlurDirection * TexelSize * float(i) * centerCoC * MaxCoCRadius;
            float2 sampleUV = uv + offset;

            float4 tapSample = g_InputTexture.SampleLevel(g_LinearSampler, sampleUV, 0);
            float tapCoC = tapSample.a;

            float cocWeight = saturate(tapCoC - centerCoC + 1.0f);
            float weight = Weights13[i + 6] * cocWeight;

            colorSum += tapSample.rgb * weight;
            weightSum += weight;
        }
    }

    float3 blurredColor = colorSum / max(weightSum, 0.001f);
    g_OutputTexture[pixelPos] = float4(blurredColor, centerCoC);
}