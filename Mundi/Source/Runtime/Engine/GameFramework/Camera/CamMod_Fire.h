#pragma once
#include "CameraModifierBase.h"
#include "PostProcessing/PostProcessing.h"

class UCamMod_Fire : public UCameraModifierBase
{

public:
    DECLARE_CLASS(UCamMod_Fire, UCameraModifierBase)

    UCamMod_Fire() = default;
    virtual ~UCamMod_Fire() = default;

    FLinearColor Color = FLinearColor(1.0f, 0.3f, 0.0f, 1.0f);  // 기본 주황색
    float Intensity = 0.8f;     // 불꽃 강도
    float EdgeStart = 0.15f;    // 화면 가장자리에서 시작 위치
    float NoiseScale = 5.0f;    // 노이즈 스케일
    float ElapsedTime = 0.0f;   // 경과 시간 (애니메이션용)
    float FadeOutRatio = 0.4f;  // Duration의 몇 %부터 페이드 아웃 시작 (0.4 = 후반 60%)

    virtual void ApplyToView(float DeltaTime, FMinimalViewInfo* ViewInfo) override
    {
        // 시간 업데이트 (애니메이션용)
        ElapsedTime += DeltaTime;
    }

    virtual void CollectPostProcess(TArray<FPostProcessModifier>& Out) override
    {
        if (!bEnabled) return;

        // 페이드 아웃 계산: Duration의 FadeOutRatio 지점부터 서서히 사라짐
        float fadeWeight = 1.0f;
        if (Duration > 0.0f)
        {
            float fadeStartTime = Duration * FadeOutRatio;
            if (Elapsed > fadeStartTime)
            {
                // fadeStartTime ~ Duration 구간에서 1.0 -> 0.0으로 페이드
                float fadeProgress = (Elapsed - fadeStartTime) / (Duration - fadeStartTime);
                fadeWeight = 1.0f - FMath::Clamp(fadeProgress, 0.0f, 1.0f);
            }
        }

        FPostProcessModifier M;
        M.Type = EPostProcessEffectType::Fire;
        M.Priority = Priority;
        M.bEnabled = true;
        M.Weight = Weight * fadeWeight;  // 페이드 아웃 적용
        M.SourceObject = this;

        M.Payload.Color = Color;
        // Params0: X=Intensity, Y=EdgeStart, Z=NoiseScale, W=Time
        M.Payload.Params0 = FVector4(Intensity, EdgeStart, NoiseScale, ElapsedTime);

        Out.Add(M);
    }
};
