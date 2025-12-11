// Fire Post Process Shader
// 화면 가장자리에서 불타는 효과를 표현

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

cbuffer FireCB : register(b2)
{
    float4 FireColor;        // 불꽃 기본 색상 (주황/빨강)
    float  Intensity;        // 불꽃 강도 (0~1)
    float  EdgeStart;        // 화면 가장자리에서 시작 위치 (0~1)
    float  NoiseScale;       // 노이즈 스케일
    float  Time;             // 애니메이션 시간
    float  Weight;           // 모디파이어 가중치
    float  _Pad0[3];
}

cbuffer ViewportConstants : register(b10)
{
    float4 ViewportRect;
    float4 ScreenSize;
}

// Simplex 노이즈를 위한 해시 함수
float hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

// Value 노이즈
float noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);

    // 4개의 코너 값
    float a = hash(i);
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));

    // 부드러운 보간
    float2 u = f * f * (3.0 - 2.0 * f);

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// FBM (Fractal Brownian Motion) - 여러 옥타브의 노이즈 합성
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

float4 mainPS(PS_INPUT input) : SV_Target
{
    float4 SceneColor = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord);

    // 뷰포트 정규화 UV 계산
    float2 ViewportStartUV = ViewportRect.xy * ScreenSize.zw;
    float2 ViewportUVSpan = ViewportRect.zw * ScreenSize.zw;
    float2 localUV = (input.texCoord - ViewportStartUV) / ViewportUVSpan;

    // 화면 아래쪽에서의 거리 계산 (0 = 아래 가장자리, 1 = 위)
    // localUV.y: 0 = 상단, 1 = 하단
    float edgeDist = 1.0 - localUV.y;

    // 불꽃 높이 (0 = 하단, 1 = 상단으로 갈수록)
    float fireHeight = localUV.y;

    // === 메인 불꽃 노이즈 (위로 흐르는 효과) ===
    float2 noiseCoord = localUV * NoiseScale * 1.5;
    noiseCoord.y -= Time * 2.5;
    float fireNoise = fbm(noiseCoord, 5);

    // === 세로로 긴 불꽃 패턴 (자연스러운 불꽃 혀) ===
    // X축은 촘촘하게, Y축은 길게 늘어뜨려서 세로로 긴 패턴
    float2 verticalCoord = float2(localUV.x * NoiseScale * 4.0, localUV.y * NoiseScale * 0.8);
    verticalCoord.y -= Time * 3.5;
    float verticalNoise = fbm(verticalCoord, 4);

    // === 빠르게 흔들리는 작은 불꽃 ===
    float2 flickerCoord = float2(localUV.x * NoiseScale * 2.5, localUV.y * NoiseScale * 1.2);
    flickerCoord.y -= Time * 5.0;
    flickerCoord.x += sin(Time * 3.0 + localUV.y * 10.0) * 0.1;  // 좌우 흔들림
    float flickerNoise = fbm(flickerCoord, 3);

    // === 불꽃 끝 강조 (위로 갈수록 더 불규칙) ===
    float tipNoise = noise(float2(localUV.x * NoiseScale * 5.0, Time * 2.0));
    tipNoise = pow(tipNoise, 0.7);  // 밝은 부분 강조

    // 노이즈 합성 (위로 갈수록 세로 패턴 강조)
    float heightBlend = saturate(fireHeight * 1.5);
    float combinedNoise = lerp(
        fireNoise * 0.5 + flickerNoise * 0.5,                    // 하단: 부드러운 노이즈
        verticalNoise * 0.6 + tipNoise * 0.4,                    // 상단: 세로로 긴 패턴
        heightBlend
    );

    // 가장자리에서 더 강한 노이즈 효과
    float noiseInfluence = 0.3 * (1.0 - edgeDist * 2.0);
    noiseInfluence = max(0.0, noiseInfluence);

    // 불꽃 마스크 계산
    float adjustedEdge = edgeDist - combinedNoise * noiseInfluence;

    // 위로 갈수록 조금 더 날카로운 경계 (과하지 않게)
    float edgePower = lerp(1.0, 1.6, saturate(fireHeight * 1.5));
    float fireMask = 1.0 - saturate(adjustedEdge / max(EdgeStart * 1.5, 0.01));
    fireMask = pow(fireMask, edgePower);
    fireMask *= Intensity;

    // 불꽃 색상 그라데이션 (더 밝고 생생한 색상)
    float3 coreColor = float3(1.0, 1.0, 0.6);    // 밝은 노랑/흰색 (코어)
    float3 innerColor = float3(1.0, 0.8, 0.2);   // 노랑
    float3 midColor = float3(1.0, 0.4, 0.0);     // 주황
    float3 outerColor = float3(0.9, 0.1, 0.0);   // 진한 빨강

    float gradientT = saturate(fireMask * 2.5);
    float3 fireGradient;
    if (gradientT < 0.3)
    {
        fireGradient = lerp(coreColor, innerColor, gradientT / 0.3);
    }
    else if (gradientT < 0.6)
    {
        fireGradient = lerp(innerColor, midColor, (gradientT - 0.3) / 0.3);
    }
    else
    {
        fireGradient = lerp(midColor, outerColor, (gradientT - 0.6) / 0.4);
    }

    // 사용자 지정 색상과 블렌딩
    fireGradient = lerp(fireGradient, FireColor.rgb, 0.2);

    // 강한 일렁임/이글거림 효과
    float flicker1 = 0.85 + 0.15 * sin(Time * 15.0 + localUV.x * 8.0);
    float flicker2 = 0.9 + 0.1 * sin(Time * 23.0 + localUV.y * 12.0);
    float flicker3 = 0.95 + 0.05 * sin(Time * 37.0 + (localUV.x + localUV.y) * 6.0);
    float flicker = flicker1 * flicker2 * flicker3;
    fireGradient *= flicker;

    // 불꽃 밝기 부스트
    fireGradient *= 1.5;

    // Additive 블렌딩으로 불꽃 효과 적용
    float blendFactor = fireMask * Weight;
    float3 finalColor = SceneColor.rgb + fireGradient * blendFactor;

    // 불꽃 영역 주변 열기 효과
    float heatTint = fireMask * Weight * 0.15;
    finalColor = lerp(finalColor, finalColor * float3(1.1, 0.95, 0.85), heatTint);

    // 화면 전체 붉은 틴트 (Additive로 강하게 적용)
    float globalRedTint = Weight * 0.15;
    finalColor += float3(globalRedTint, globalRedTint * 0.2, 0.0);  // 빨간색 추가

    return float4(finalColor, SceneColor.a);
}
