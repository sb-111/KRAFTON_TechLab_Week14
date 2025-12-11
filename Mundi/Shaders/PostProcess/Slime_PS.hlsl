// Slime Post Process Shader
// Boomer 폭발 시 화면에 녹색 끈적한 액체가 묻어서 흘러내리는 효과

Texture2D    g_SceneColorTex   : register(t0);

SamplerState g_LinearClampSample  : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer PostProcessCB : register(b0)
{
    float Near;
    float Far;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
}

cbuffer SlimeCB : register(b2)
{
    float4 SlimeColor;           // 슬라임 색상 (녹색)
    float  Intensity;            // 효과 강도 (0~1)
    float  DrippingSpeed;        // 흘러내리는 속도
    float  Time;                 // 애니메이션 시간
    float  Weight;               // 모디파이어 가중치
    float  Coverage;             // 화면 덮는 정도 (0~1)
    float  DistortionStrength;   // 왜곡 강도
    float  _Pad[2];
}

cbuffer ViewportConstants : register(b10)
{
    float4 ViewportRect;
    float4 ScreenSize;
}

// 해시 함수
float hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

float hash2(float2 p)
{
    return frac(sin(dot(p, float2(269.5, 183.3))) * 43758.5453123);
}

// Value 노이즈
float noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);

    float a = hash(i);
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));

    float2 u = f * f * (3.0 - 2.0 * f);

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// FBM 노이즈
float fbm(float2 p, int octaves)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < octaves; i++)
    {
        value += amplitude * noise(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }

    return value;
}

// Voronoi 노이즈 (방울 생성용)
float voronoi(float2 p, out float2 cellCenter)
{
    float2 i = floor(p);
    float2 f = frac(p);

    float minDist = 1.0;
    cellCenter = float2(0, 0);

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float2 neighbor = float2(x, y);
            float2 cell = i + neighbor;
            float2 pt = neighbor + float2(hash(cell), hash2(cell)) - f;
            float dist = length(pt);

            if (dist < minDist)
            {
                minDist = dist;
                cellCenter = cell;
            }
        }
    }

    return minDist;
}

// 방울 모양 생성 (부드러운 가장자리)
float dropletShape(float2 uv, float2 center, float radius, float seed)
{
    float2 offset = uv - center;

    // 방울 왜곡 (불규칙한 모양)
    float distortion = fbm(uv * 8.0 + seed, 3) * 0.3;
    float dist = length(offset) + distortion * radius;

    // 부드러운 가장자리
    float shape = smoothstep(radius, radius * 0.3, dist);

    return shape;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    // 뷰포트 정규화 UV 계산
    float2 ViewportStartUV = ViewportRect.xy * ScreenSize.zw;
    float2 ViewportUVSpan = ViewportRect.zw * ScreenSize.zw;
    float2 localUV = (input.texCoord - ViewportStartUV) / ViewportUVSpan;

    // === 방울 생성 ===
    float slimeMask = 0.0;
    float2 totalDistortion = float2(0, 0);

    // 여러 크기의 방울 레이어
    float numDroplets = 12.0;

    for (float i = 0; i < numDroplets; i++)
    {
        // 각 방울의 랜덤 위치와 크기
        float seed = i * 1.618;  // 황금비로 분포
        float2 dropletPos = float2(
            hash(float2(seed, 0.0)),
            hash(float2(0.0, seed)) * 0.7 + 0.15  // 위쪽에 더 많이 분포
        );

        float dropletSize = 0.08 + hash(float2(seed, seed)) * 0.12;
        dropletSize *= Coverage;

        // 흘러내림 효과 (시간에 따라 아래로 이동)
        float drip = Time * DrippingSpeed * (0.5 + hash(float2(seed, 1.0)) * 0.5);
        dropletPos.y += drip;

        // 방울이 화면 아래로 나가면 위에서 다시 시작
        dropletPos.y = frac(dropletPos.y);

        // 아래로 갈수록 방울이 늘어남 (중력 효과)
        float stretch = 1.0 + drip * 0.5;
        float2 stretchedUV = float2(localUV.x, localUV.y / stretch);
        float2 stretchedPos = float2(dropletPos.x, dropletPos.y / stretch);

        // 방울 모양
        float droplet = dropletShape(localUV, dropletPos, dropletSize, seed);

        // 흘러내리는 꼬리 추가
        float tailLength = 0.15 * drip;
        float2 tailUV = float2(localUV.x, localUV.y - tailLength);
        float tail = dropletShape(tailUV, dropletPos, dropletSize * 0.6, seed + 1.0);
        tail *= smoothstep(dropletPos.y, dropletPos.y + tailLength, localUV.y);

        droplet = max(droplet, tail * 0.7);

        slimeMask += droplet;

        // 굴절 방향 계산 (방울 중심에서 바깥으로)
        float2 toCenter = dropletPos - localUV;
        totalDistortion += toCenter * droplet * 0.5;
    }

    // 큰 얼룩 추가 (화면 가장자리)
    float2 noiseCoord = localUV * 3.0;
    float edgeSlime = fbm(noiseCoord + Time * 0.1, 4);
    float edgeMask = 1.0 - smoothstep(0.0, 0.3, min(min(localUV.x, 1.0 - localUV.x), min(localUV.y, 1.0 - localUV.y)));
    edgeSlime *= edgeMask * Coverage;

    slimeMask += edgeSlime * 0.5;
    slimeMask = saturate(slimeMask);

    // === 굴절/왜곡 효과 ===
    float2 distortedUV = input.texCoord + totalDistortion * DistortionStrength;
    distortedUV = clamp(distortedUV, 0.001, 0.999);  // UV 범위 제한

    float4 SceneColor = g_SceneColorTex.Sample(g_LinearClampSample, distortedUV);
    float4 OriginalColor = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord);

    // 왜곡된 영역과 원본 블렌딩
    float4 distortedScene = lerp(OriginalColor, SceneColor, slimeMask * Intensity);

    // === 슬라임 색상 적용 ===
    // 프레넬 효과 (가장자리가 더 밝게)
    float fresnel = pow(1.0 - slimeMask, 2.0) * slimeMask;

    // 슬라임 색상 (반투명 녹색)
    float3 slimeCol = SlimeColor.rgb;

    // 두께에 따른 색상 변화 (두꺼운 곳은 더 어둡고 진하게)
    float thickness = slimeMask;
    slimeCol = lerp(slimeCol * 1.3, slimeCol * 0.6, thickness);

    // 하이라이트 (빛 반사)
    float highlight = pow(saturate(1.0 - slimeMask * 2.0), 4.0) * slimeMask;
    slimeCol += float3(0.3, 0.4, 0.2) * highlight;

    // 끈적한 느낌의 광택
    float gloss = fbm(localUV * 20.0 + Time * 0.5, 3);
    gloss = pow(gloss, 3.0) * slimeMask * 0.3;
    slimeCol += float3(0.2, 0.3, 0.1) * gloss;

    // === 최종 블렌딩 ===
    float blendFactor = slimeMask * Intensity * Weight;
    float3 finalColor = lerp(distortedScene.rgb, slimeCol, blendFactor * 0.7);

    // 전체 화면에 녹색 틴트 추가
    float globalTint = Weight * 0.1;
    finalColor = lerp(finalColor, finalColor * float3(0.9, 1.1, 0.85), globalTint);

    // 슬라임 영역 어둡게 (시야 방해)
    finalColor *= lerp(1.0, 0.7, blendFactor * 0.5);

    return float4(finalColor, OriginalColor.a);
}
