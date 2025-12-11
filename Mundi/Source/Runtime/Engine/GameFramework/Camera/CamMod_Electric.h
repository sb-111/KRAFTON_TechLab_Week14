#pragma once
#include "CameraModifierBase.h"
#include "PostProcessing/PostProcessing.h"

class UCamMod_Electric : public UCameraModifierBase
{

public:
    DECLARE_CLASS(UCamMod_Electric, UCameraModifierBase)

    UCamMod_Electric() = default;
    virtual ~UCamMod_Electric() = default;

    // 주황-노랑 색상 (CoreBeam 기반)
    FLinearColor Color = FLinearColor(1.0f, 0.7f, 0.3f, 1.0f);
    float Intensity = 0.8f;           // 효과 강도
    float FlickerSpeed = 20.0f;       // 번쩍임 속도 (더 빠르게)
    float BoltCount = 12.0f;          // 번개 줄기 개수 (가지 포함 시 더 많아짐)
    float ChromaticStrength = 0.015f; // 색수차 강도
    float ElapsedTime = 0.0f;         // 경과 시간 (애니메이션용)
    float FadeOutRatio = 0.6f;        // Duration의 몇 %부터 페이드 아웃 시작

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
        M.Type = EPostProcessEffectType::Electric;
        M.Priority = Priority;
        M.bEnabled = true;
        M.Weight = Weight * fadeWeight;  // 페이드 아웃 적용
        M.SourceObject = this;

        M.Payload.Color = Color;
        // Params0: X=Intensity, Y=Time, Z=FlickerSpeed, W=BoltCount
        M.Payload.Params0 = FVector4(Intensity, ElapsedTime, FlickerSpeed, BoltCount);
        // Params1: X=ChromaticStrength
        M.Payload.Params1 = FVector4(ChromaticStrength, 0.0f, 0.0f, 0.0f);

        Out.Add(M);
    }
};
