#pragma once
#include "CameraModifierBase.h"
#include "PostProcessing/PostProcessing.h"

class UCamMod_Slime : public UCameraModifierBase
{

public:
    DECLARE_CLASS(UCamMod_Slime, UCameraModifierBase)

    UCamMod_Slime() = default;
    virtual ~UCamMod_Slime() = default;

    FLinearColor Color = FLinearColor(0.2f, 0.8f, 0.1f, 1.0f);  // 기본 녹색
    float Intensity = 0.7f;           // 효과 강도
    float DrippingSpeed = 0.3f;       // 흘러내리는 속도
    float Coverage = 0.6f;            // 화면 덮는 정도
    float DistortionStrength = 0.02f; // 왜곡 강도
    float ElapsedTime = 0.0f;         // 경과 시간 (애니메이션용)
    float FadeOutRatio = 0.5f;        // Duration의 몇 %부터 페이드 아웃 시작

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
        M.Type = EPostProcessEffectType::Slime;
        M.Priority = Priority;
        M.bEnabled = true;
        M.Weight = Weight * fadeWeight;  // 페이드 아웃 적용
        M.SourceObject = this;

        M.Payload.Color = Color;
        // Params0: X=Intensity, Y=DrippingSpeed, Z=Time, W=Coverage
        M.Payload.Params0 = FVector4(Intensity, DrippingSpeed, ElapsedTime, Coverage);
        // Params1: X=DistortionStrength
        M.Payload.Params1 = FVector4(DistortionStrength, 0.0f, 0.0f, 0.0f);

        Out.Add(M);
    }
};
