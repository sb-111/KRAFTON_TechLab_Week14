// Electric Post Process Shader
// 화면 전체에 전기 감전 효과를 표현 (번쩍임 + 번개 줄기 + 크로매틱 수차)

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

cbuffer ElectricCB : register(b2)
{
    float4 ElectricColor;       // 전기 색상 (주황-노랑)
    float  Intensity;           // 효과 강도 (0~1)
    float  Time;                // 애니메이션 시간
    float  FlickerSpeed;        // 번쩍임 속도
    float  Weight;              // 모디파이어 가중치
    float  BoltCount;           // 번개 줄기 개수
    float  ChromaticStrength;   // 색수차 강도
    float  _Pad0[2];
}

cbuffer ViewportConstants : register(b10)
{
    float4 ViewportRect;
    float4 ScreenSize;
}

// 해시 함수들
float hash(float n)
{
    return frac(sin(n) * 43758.5453123);
}

float hash2(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

float2 hash22(float2 p)
{
    float n = sin(dot(p, float2(41.0, 289.0)));
    return frac(float2(262144.0, 32768.0) * n);
}

// Value 노이즈
float noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0 - 2.0 * f);

    float a = hash2(i);
    float b = hash2(i + float2(1.0, 0.0));
    float c = hash2(i + float2(0.0, 1.0));
    float d = hash2(i + float2(1.0, 1.0));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// FBM (Fractal Brownian Motion)
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

// 날카로운 노이즈 (번개의 꺾임을 위해)
float sharpNoise(float2 p, float t)
{
    float n = noise(p + t);
    // 더 날카롭게 꺾이도록
    return (n - 0.5) * 2.0;
}

// 불규칙한 번쩍임 생성 - 더 빠르고 급격하게
float getFlicker(float t, float seed)
{
    // 빠른 불규칙 펄스
    float f1 = sin(t * FlickerSpeed * 2.0 + seed * 100.0);
    float f2 = sin(t * FlickerSpeed * 5.7 + seed * 200.0);
    float f3 = sin(t * FlickerSpeed * 11.3 + seed * 300.0);
    float f4 = sin(t * FlickerSpeed * 17.9 + seed * 400.0);

    float combined = f1 * 0.3 + f2 * 0.3 + f3 * 0.25 + f4 * 0.15;

    // 더 급격한 온/오프
    float flicker = smoothstep(0.1, 0.3, combined);

    // 랜덤 스파이크 추가
    float spike = step(0.92, hash(floor(t * FlickerSpeed * 3.0) + seed));

    return saturate(flicker + spike * 0.5);
}

// 세그먼트 기반 번개 - 실제 번개처럼 꺾이는 효과
float lightningSegment(float2 uv, float2 start, float2 end, float thickness, float glowSize)
{
    float2 pa = uv - start;
    float2 ba = end - start;
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    float d = length(pa - ba * h);

    // 코어 (밝은 중심)
    float core = smoothstep(thickness, thickness * 0.1, d);
    // 글로우 (주변 빛번짐)
    float glow = smoothstep(glowSize, 0.0, d) * 0.5;

    return core + glow;
}

// 분기(가지)가 있는 번개 생성
float generateLightningWithBranches(float2 uv, int boltIndex, float t)
{
    float seed = float(boltIndex) * 2.718; // 자연로그 e로 분산
    float timeSeed = floor(t * 3.0 + hash(seed) * 2.0);

    // 시작점과 방향 결정
    float2 startPoint;
    float2 mainDir;
    float totalLength;

    // 더 다양한 방향 패턴 (8가지)
    int dirType = int(hash(seed * 1.1 + timeSeed * 0.1) * 8.0);

    if (dirType == 0)
    {
        // 왼쪽 가장자리에서
        startPoint = float2(-0.05, hash(seed * 1.2 + timeSeed) * 0.8 + 0.1);
        mainDir = normalize(float2(1.0, (hash(seed * 1.3 + timeSeed) - 0.5) * 0.6));
        totalLength = 0.3 + hash(seed * 1.4) * 0.25;
    }
    else if (dirType == 1)
    {
        // 오른쪽 가장자리에서
        startPoint = float2(1.05, hash(seed * 2.2 + timeSeed) * 0.8 + 0.1);
        mainDir = normalize(float2(-1.0, (hash(seed * 2.3 + timeSeed) - 0.5) * 0.6));
        totalLength = 0.3 + hash(seed * 2.4) * 0.25;
    }
    else if (dirType == 2)
    {
        // 위쪽 가장자리에서
        startPoint = float2(hash(seed * 3.2 + timeSeed) * 0.8 + 0.1, -0.05);
        mainDir = normalize(float2((hash(seed * 3.3 + timeSeed) - 0.5) * 0.6, 1.0));
        totalLength = 0.3 + hash(seed * 3.4) * 0.25;
    }
    else if (dirType == 3)
    {
        // 아래쪽 가장자리에서
        startPoint = float2(hash(seed * 4.2 + timeSeed) * 0.8 + 0.1, 1.05);
        mainDir = normalize(float2((hash(seed * 4.3 + timeSeed) - 0.5) * 0.6, -1.0));
        totalLength = 0.3 + hash(seed * 4.4) * 0.25;
    }
    else if (dirType == 4)
    {
        // 대각선 (좌상단에서)
        startPoint = float2(-0.05, -0.05 + hash(seed * 5.1 + timeSeed) * 0.3);
        mainDir = normalize(float2(1.0, 0.7 + hash(seed * 5.2) * 0.3));
        totalLength = 0.35 + hash(seed * 5.3) * 0.2;
    }
    else if (dirType == 5)
    {
        // 대각선 (우상단에서)
        startPoint = float2(1.05, -0.05 + hash(seed * 6.1 + timeSeed) * 0.3);
        mainDir = normalize(float2(-1.0, 0.7 + hash(seed * 6.2) * 0.3));
        totalLength = 0.35 + hash(seed * 6.3) * 0.2;
    }
    else if (dirType == 6)
    {
        // 화면 중앙 근처에서 방사형
        float angle = hash(seed * 7.1 + timeSeed) * 6.28318;
        float2 center = float2(0.5, 0.5);
        startPoint = center + float2(cos(angle), sin(angle)) * (0.05 + hash(seed * 7.2) * 0.1);
        mainDir = float2(cos(angle), sin(angle));
        totalLength = 0.2 + hash(seed * 7.3) * 0.15;
    }
    else
    {
        // 코너에서 반대 코너로
        int corner = int(hash(seed * 8.1) * 4.0);
        if (corner == 0) { startPoint = float2(-0.05, -0.05); mainDir = normalize(float2(1.0, 1.0)); }
        else if (corner == 1) { startPoint = float2(1.05, -0.05); mainDir = normalize(float2(-1.0, 1.0)); }
        else if (corner == 2) { startPoint = float2(-0.05, 1.05); mainDir = normalize(float2(1.0, -1.0)); }
        else { startPoint = float2(1.05, 1.05); mainDir = normalize(float2(-1.0, -1.0)); }
        totalLength = 0.4 + hash(seed * 8.2) * 0.2;
    }

    // 번개 세그먼트 생성 (극적인 지그재그 효과)
    float lightning = 0.0;
    float2 currentPos = startPoint;
    int numSegments = 8 + int(hash(seed * 9.1) * 6.0); // 8~14개 세그먼트 (더 많이)
    float segmentLength = totalLength / float(numSegments);

    // 번쩍임 체크
    float boltFlicker = step(0.3, hash(seed + floor(t * FlickerSpeed * 0.8 + hash(seed) * 5.0)));
    if (boltFlicker < 0.5) return 0.0;

    // 지그재그 방향 (번갈아가며 꺾임)
    float zigzagDir = 1.0;

    for (int seg = 0; seg < numSegments; seg++)
    {
        float segSeed = seed + float(seg) * 3.7;
        float2 perpDir = float2(-mainDir.y, mainDir.x);

        // 극적인 지그재그 꺾임 - 매 세그먼트마다 방향 전환
        // bendAmount를 0.6~1.2 사이로 크게 (약 30~60도 꺾임)
        float baseBend = 0.6 + hash(segSeed * 1.5) * 0.6;

        // 지그재그: 번갈아가며 좌우로 꺾임
        float bendAmount = baseBend * zigzagDir;

        // 추가 랜덤성 (완전히 규칙적이지 않게)
        bendAmount += sharpNoise(float2(segSeed, t * 3.0), t) * 0.3;

        // 가끔 같은 방향으로 두 번 꺾이기도 함 (더 자연스럽게)
        if (hash(segSeed * 1.7) > 0.8)
            zigzagDir = zigzagDir; // 방향 유지
        else
            zigzagDir = -zigzagDir; // 방향 전환

        float2 segDir = normalize(mainDir + perpDir * bendAmount);
        float2 nextPos = currentPos + segDir * segmentLength;

        // 세그먼트별 두께 변화 (끝으로 갈수록 가늘게)
        float progress = float(seg) / float(numSegments);
        float thickness = 0.007 * (1.0 - progress * 0.6);
        float glowSize = 0.03 * (1.0 - progress * 0.4);

        lightning += lightningSegment(uv, currentPos, nextPos, thickness, glowSize);

        // 가지(branch) 생성 - 더 자주, 더 극적으로
        if (hash(segSeed * 2.1) > 0.5 && seg > 0 && seg < numSegments - 1)
        {
            // 가지 각도도 더 크게 (60~90도)
            float branchAngle = (hash(segSeed * 2.2) - 0.5) * 2.5;
            float2 branchDir = float2(
                segDir.x * cos(branchAngle) - segDir.y * sin(branchAngle),
                segDir.x * sin(branchAngle) + segDir.y * cos(branchAngle)
            );
            float branchLength = segmentLength * (0.4 + hash(segSeed * 2.3) * 0.5);
            float2 branchEnd = currentPos + branchDir * branchLength;

            float branchThickness = thickness * 0.6;
            float branchGlow = glowSize * 0.5;
            lightning += lightningSegment(uv, currentPos, branchEnd, branchThickness, branchGlow) * 0.7;

            // 2차 가지 - 더 자주 생성
            if (hash(segSeed * 2.4) > 0.5)
            {
                float subBranchAngle = (hash(segSeed * 2.5) - 0.5) * 2.0;
                float2 subBranchDir = float2(
                    branchDir.x * cos(subBranchAngle) - branchDir.y * sin(subBranchAngle),
                    branchDir.x * sin(subBranchAngle) + branchDir.y * cos(subBranchAngle)
                );
                float2 subBranchEnd = branchEnd + subBranchDir * branchLength * 0.6;
                lightning += lightningSegment(uv, branchEnd, subBranchEnd, branchThickness * 0.5, branchGlow * 0.4) * 0.5;

                // 3차 가지 (아주 작은 가지)
                if (hash(segSeed * 2.6) > 0.6)
                {
                    float tinyAngle = (hash(segSeed * 2.7) - 0.5) * 1.8;
                    float2 tinyDir = float2(
                        subBranchDir.x * cos(tinyAngle) - subBranchDir.y * sin(tinyAngle),
                        subBranchDir.x * sin(tinyAngle) + subBranchDir.y * cos(tinyAngle)
                    );
                    float2 tinyEnd = subBranchEnd + tinyDir * branchLength * 0.3;
                    lightning += lightningSegment(uv, subBranchEnd, tinyEnd, branchThickness * 0.25, branchGlow * 0.2) * 0.3;
                }
            }
        }

        currentPos = nextPos;

        // 메인 방향도 조금씩 틀어지게 (전체적으로 휘어지는 효과)
        float mainBendAmount = sharpNoise(float2(segSeed * 0.5, t), t) * 0.1;
        mainDir = normalize(mainDir + perpDir * mainBendAmount);
    }

    return lightning;
}

// 전기 아크 노이즈 (화면 전체에 지직거리는 효과)
float electricArcNoise(float2 uv, float t)
{
    float2 p = uv * 15.0;
    p.x += t * 8.0;
    p.y += sin(t * 3.0 + uv.x * 5.0) * 2.0;

    float n = noise(p);
    n = pow(n, 3.0); // 밝은 점들만 남김

    // 빠른 깜빡임
    float flicker = step(0.7, sin(t * 30.0 + uv.x * 20.0 + uv.y * 15.0));

    return n * flicker;
}

// 화면 가장자리 전기 글로우
float getEdgeElectricGlow(float2 uv, float t)
{
    float2 fromCenter = abs(uv - 0.5) * 2.0;
    float edgeDist = max(fromCenter.x, fromCenter.y);

    // 기본 가장자리 글로우
    float baseGlow = smoothstep(0.5, 1.0, edgeDist);

    // 움직이는 전기 패턴
    float electricPattern = noise(float2(uv.x * 10.0 + t * 3.0, uv.y * 10.0 - t * 2.0));
    electricPattern += noise(float2(uv.x * 20.0 - t * 5.0, uv.y * 20.0 + t * 4.0)) * 0.5;
    electricPattern = pow(electricPattern, 2.0);

    return baseGlow * (0.5 + electricPattern * 0.5);
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    // 뷰포트 정규화 UV 계산
    float2 ViewportStartUV = ViewportRect.xy * ScreenSize.zw;
    float2 ViewportUVSpan = ViewportRect.zw * ScreenSize.zw;
    float2 localUV = (input.texCoord - ViewportStartUV) / ViewportUVSpan;

    // === 메인 번쩍임 효과 ===
    float mainFlicker = getFlicker(Time, 0.0);
    float secondaryFlicker = getFlicker(Time * 1.3, 1.5);

    // === 크로매틱 수차 (더 강하게) ===
    float2 fromCenter = localUV - 0.5;
    float distFromCenter = length(fromCenter);
    float2 chromaDir = normalize(fromCenter + 0.0001);

    float chromaOffset = ChromaticStrength * (mainFlicker * 0.8 + 0.2) * distFromCenter * 1.5;

    // RGB 채널 분리
    float r = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord + chromaDir * chromaOffset).r;
    float g = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord).g;
    float b = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord - chromaDir * chromaOffset * 0.7).b;

    float3 sceneColor = float3(r, g, b);
    float sceneAlpha = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord).a;

    // === 번개 줄기 생성 (가지 포함) ===
    float totalLightning = 0.0;
    int numBolts = (int)BoltCount;
    for (int i = 0; i < numBolts; i++)
    {
        totalLightning += generateLightningWithBranches(localUV, i, Time);
    }
    totalLightning = saturate(totalLightning);

    // === 전기 아크 노이즈 ===
    float arcNoise = electricArcNoise(localUV, Time);

    // === 가장자리 전기 글로우 ===
    float edgeGlow = getEdgeElectricGlow(localUV, Time);

    // === 색상 ===
    float3 mainColor = ElectricColor.rgb;
    float3 coreColor = float3(1.0, 0.98, 0.9); // 거의 흰색 (번개 중심)
    float3 glowColor = float3(1.0, 0.6, 0.2);  // 주황 글로우

    // === 최종 효과 계산 ===
    float effectStrength = Intensity * Weight;
    float3 finalColor = sceneColor;

    // 1. 번개 줄기 (코어는 밝게, 글로우는 주황)
    float3 lightningColor = lerp(glowColor, coreColor, saturate(totalLightning * 2.0));
    finalColor += lightningColor * totalLightning * effectStrength * 2.5;

    // 2. 전기 아크 노이즈
    finalColor += mainColor * arcNoise * mainFlicker * effectStrength * 0.4;

    // 3. 가장자리 전기 글로우
    float3 edgeEffect = glowColor * edgeGlow * (mainFlicker * 0.7 + secondaryFlicker * 0.3) * effectStrength * 0.6;
    finalColor += edgeEffect;

    // 4. 전체 화면 플래시 (급격한 번쩍임)
    float flashIntensity = mainFlicker * effectStrength * 0.25;
    flashIntensity += step(0.95, mainFlicker) * effectStrength * 0.3; // 강한 플래시
    finalColor += mainColor * flashIntensity;

    // 5. 화면 전체 색조 (감전 느낌)
    float globalTint = effectStrength * 0.15;
    finalColor = lerp(finalColor, finalColor * float3(1.15, 0.95, 0.8), globalTint);

    // 6. 약간의 노출 과다 효과 (번쩍일 때)
    finalColor *= 1.0 + mainFlicker * effectStrength * 0.2;

    return float4(finalColor, sceneAlpha);
}
