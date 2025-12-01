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

static const float2 poissonDisk[16] = {
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

// Precomputed maximum length of values in poissonDisk (measured from provided array).
// This value was computed from the original array: ~1.23393213
static const float POISSON_MAX_LEN = 1.23393213;
static const float POISSON_COUNT = 16;

float ComputeKernelWeight(float r)
{
    // simple triangular falloff
    return saturate(1.0 - r);
}

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

    // === 포이송 샘플링 ==

    uint KernelRadius = (uint)ceil(centerCoC * MaxCoCRadius);
    if (KernelRadius % 2 == 0)
        KernelRadius++;

    // 최소/최대 제한
    KernelRadius = clamp(KernelRadius, 1u, 31u);

    float radiusPixels = centerCoC * MaxCoCRadius;
    float3 acc = float3(0.0f, 0.0f, 0.0f);
    float wsum = 0;

    // for (uint i = 0; i < KernelRadius; i++)
    // {
    //     float2 dir = poissonDisk[i % 16];
    //     float2 off = dir * radiusPixels * TexelSize;
    //     float2 sampleUV = uv + off;
    //
    //     float3 samp = g_InputTexture.SampleLevel(g_LinearSampler, sampleUV, 0).rgb;
    //     float rNorm = length(dir) / POISSON_MAX_LEN;
    //     float w = ComputeKernelWeight(rNorm);
    //     acc += samp * w;
    //     wsum += w;
    // }
    //
    // float3 result = acc / max(wsum, 1e-6);
    // float blend = saturate(centerCoC);
    // float3 outCol = lerp(centerSample.rgb, result, blend);
    //
    // // RGB: Blurred Color, A: CoC (유지)
    // g_OutputTexture[pixelPos] = float4(outCol, centerCoC);

    for (uint i = 0; i < KernelRadius; ++i)
    {
        // choose a direction from poisson disk (wrap if KernelRadius > POISSON_COUNT)
        float2 dir = poissonDisk[i % POISSON_COUNT];

        // compute normalized radial coordinate in [0..1] for this dir
        // len(dir) / POISSON_MAX_LEN -> ~0..1
        float dirLenNorm = length(dir) / POISSON_MAX_LEN;

        // **Important**: Scale dir to actual pixel radius, but dir should be treated as direction*unitRadius
        // We want UV offset = (normalizedDir) * radiusPixels * TexelSize
        // Get unit direction:
        float2 dirUnit = dir / max(length(dir), 1e-6f); // guard divide-by-zero

        // optionally multiply by dirLenNorm if you want the poissonDisk to encode radius jitter
        // here we will use dirLenNorm as an additional radial factor for variety:
        float radialFactor = dirLenNorm; // 0..1 roughly

        // final offset in UV space
        float2 off = dirUnit * (radialFactor * radiusPixels) * TexelSize;
        float2 sampleUV = uv + off;

        // sample scene color (uv bounds may go outside 0..1; sampler should be clamp)
        float3 samp = g_InputTexture.SampleLevel(g_LinearSampler, sampleUV, 0).rgb;

        // compute weight based on radial normalized distance (radialFactor)
        // kernel expects 0..1 where 1 is outermost radius.
        float w = ComputeKernelWeight(radialFactor);

        // accumulate
        acc += samp * w;
        wsum += w;
    }

    // if weights sum is (near) zero, fallback to center sample
    if (wsum <= 1e-6f)
    {
        g_OutputTexture[pixelPos] = centerSample;
        return;
    }

    float3 result = acc / wsum;

    // mix original and blurred based on CoC strength
    float blend = saturate(centerCoC);
    float3 outCol = lerp(centerSample.rgb, result, blend);

    // write result (preserve CoC in alpha)
    g_OutputTexture[pixelPos] = float4(outCol, centerCoC);
}