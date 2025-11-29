#pragma once
#include "pch.h"

// 스키닝 모드 (언리얼 엔진 방식)
enum class ESkinningMode
{
    ForceGPU,   // 모든 메시 GPU 스키닝
    ForceCPU    // 모든 메시 CPU 스키닝
};

// Per-world render settings (view mode + show flags)
class URenderSettings {
public:
    URenderSettings() = default;
    ~URenderSettings() = default;

    // View mode
    void SetViewMode(EViewMode In) { ViewMode = In; }
    EViewMode GetViewMode() const { return ViewMode; }

    // Show flags
    EEngineShowFlags GetShowFlags() const { return ShowFlags; }
    void SetShowFlags(EEngineShowFlags In) { ShowFlags = In; }
    void EnableShowFlag(EEngineShowFlags Flag) { ShowFlags |= Flag; }
    void DisableShowFlag(EEngineShowFlags Flag) { ShowFlags &= ~Flag; }
    void ToggleShowFlag(EEngineShowFlags Flag) { ShowFlags = HasShowFlag(ShowFlags, Flag) ? (ShowFlags & ~Flag) : (ShowFlags | Flag); }
    bool IsShowFlagEnabled(EEngineShowFlags Flag) const { return HasShowFlag(ShowFlags, Flag); }

    // FXAA parameters
    void SetFXAAEdgeThresholdMin(float Value) { FXAAEdgeThresholdMin = Value; }
    float GetFXAAEdgeThresholdMin() const { return FXAAEdgeThresholdMin; }

    void SetFXAAEdgeThresholdMax(float Value) { FXAAEdgeThresholdMax = Value; }
    float GetFXAAEdgeThresholdMax() const { return FXAAEdgeThresholdMax; }

    void SetFXAAQualitySubPix(float Value) { FXAAQualitySubPix = Value; }
    float GetFXAAQualitySubPix() const { return FXAAQualitySubPix; }

    void SetFXAAQualityIterations(int32 Value) { FXAAQualityIterations = Value; }
    int32 GetFXAAQualityIterations() const { return FXAAQualityIterations; }

    // Depth of Field parameters
    void SetDOFFocalDistance(float Value) { DOFFocalDistance = Value; }
    float GetDOFFocalDistance() const { return DOFFocalDistance; }

    void SetDOFNearTransitionRange(float Value) { DOFNearTransitionRange = Value; }
    float GetDOFNearTransitionRange() const { return DOFNearTransitionRange; }

    void SetDOFFarTransitionRange(float Value) { DOFFarTransitionRange = Value; }
    float GetDOFFarTransitionRange() const { return DOFFarTransitionRange; }

    void SetDOFMaxCoCRadius(float Value) { DOFMaxCoCRadius = Value; }
    float GetDOFMaxCoCRadius() const { return DOFMaxCoCRadius; }

    // Tile-based light culling
    void SetTileSize(uint32 Value) { TileSize = Value; }
    uint32 GetTileSize() const { return TileSize; }

    // 그림자 안티 에일리어싱
    void SetShadowAATechnique(EShadowAATechnique In) { ShadowAATechnique = In; }
    EShadowAATechnique GetShadowAATechnique() const { return ShadowAATechnique; }

    // 전역 스키닝 모드 (모든 World가 공유, static 함수)
    // 어떤 World의 RenderSettings를 통해서든 동일한 전역 값에 접근
    static void SetGlobalSkinningMode(ESkinningMode Mode) { GlobalSkinningMode = Mode; }
    static ESkinningMode GetGlobalSkinningMode() { return GlobalSkinningMode; }

    // 편의 함수: 인스턴스 메서드로도 호출 가능 (기존 코드 호환성)
    void SetGlobalSkinningModeInstance(ESkinningMode Mode) { SetGlobalSkinningMode(Mode); }
    ESkinningMode GetGlobalSkinningModeInstance() const { return GetGlobalSkinningMode(); }

private:
    EEngineShowFlags ShowFlags = EEngineShowFlags::SF_DefaultEnabled;
    EViewMode ViewMode = EViewMode::VMI_Lit_Phong;

    // FXAA parameters
    float FXAAEdgeThresholdMin = 0.0833f;   // 엣지 감지 최소 휘도 차이 (권장: 0.0833)
    float FXAAEdgeThresholdMax = 0.166f;    // 엣지 감지 최대 휘도 차이 (권장: 0.166)
    float FXAAQualitySubPix = 0.75f;        // 서브픽셀 품질 (낮을수록 부드러움, 권장: 0.75)
    int32 FXAAQualityIterations = 12;       // 엣지 탐색 반복 횟수 (권장: 12)

    // Depth of Field parameters
    float DOFFocalDistance = 10.0f;          // 포커스 거리 (미터, 기본값: 10.0)
    float DOFNearTransitionRange = 5.0f;     // 포그라운드 전환 범위 (미터, 기본값: 5.0)
    float DOFFarTransitionRange = 10.0f;     // 백그라운드 전환 범위 (미터, 기본값: 10.0)
    float DOFMaxCoCRadius = 8.0f;            // 최대 CoC 반경 (픽셀, 기본값: 8.0)

    // Tile-based light culling
    uint32 TileSize = 16;                   // 타일 크기 (픽셀, 기본값: 16)

    // 그림자 안티 에일리어싱
    EShadowAATechnique ShadowAATechnique = EShadowAATechnique::PCF; // 기본값 PCF

    // 전역 스키닝 모드 (모든 World가 공유, static)
    static ESkinningMode GlobalSkinningMode;
};
