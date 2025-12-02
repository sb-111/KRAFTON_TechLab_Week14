#pragma once

class UWorld; class FViewport; class FViewportClient; class ASkeletalMeshActor; class USkeletalMesh; class UAnimSequence;
class UParticleSystem; class UParticleSystemComponent; class AActor; class UParticleModule;
class UShapeAnchorComponent;
class UConstraintAnchorComponent;

struct FAnimNotifyEvent
{
    float TriggerTime;
    float Duration;
    FName NotifyName;
    FString SoundPath;
    FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
};

struct FNotifyTrack
{
    FString Name;
    TArray<FAnimNotifyEvent> Notifies;

    FNotifyTrack() = default;
    FNotifyTrack(const FString& InName) : Name(InName) {}
};

struct FSelectedNotify
{
    int32 TrackIndex = -1;
    int32 NotifyIndex = -1;

    bool IsValid() const { return TrackIndex != -1 && NotifyIndex != -1; }
    void Invalidate() { TrackIndex = -1; NotifyIndex = -1; }
};

class ViewerState
{
public:
    // 가상 소멸자 - 파생 클래스 포인터를 기본 클래스 포인터로 delete할 때 필요
    virtual ~ViewerState() = default;

    FName Name;
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;
    
    // Have a pointer to the currently selected mesh to render in the viewer
    ASkeletalMeshActor* PreviewActor = nullptr;
    USkeletalMesh* CurrentMesh = nullptr;
    FString LoadedMeshPath;  // Track loaded mesh path for unloading
    int32 SelectedBoneIndex = -1;
    bool bShowMesh = true;
    bool bShowBones = true;
    // Bone line rebuild control
    bool bBoneLinesDirty = true;      // true면 본 라인 재구성
    int32 LastSelectedBoneIndex = -1; // 색상 갱신을 위한 이전 선택 인덱스
    // UI path buffer per-tab
    char MeshPathBuffer[260] = {0};
    std::set<int32> ExpandedBoneIndices;

    // 본 트랜스폼 편집 관련
    FVector EditBoneLocation;
    FVector EditBoneRotation;  // Euler angles in degrees
    FVector EditBoneScale;
    
    bool bBoneTransformChanged = false;
    bool bBoneRotationEditing = false;
    bool bRequestScrollToBone = false;

    // Animation State
    UAnimSequence* CurrentAnimation = nullptr;
    bool bIsPlaying = false;
    bool bIsLooping = true;
    bool bReversePlay = false;
    float PlaybackSpeed = 1.0f;
    float CurrentTime = 0.0f;
    float TotalTime = 0.0f;

    bool bIsScrubbing = false;

    float PreviousTime = 0.0f;  // The animation time from the previous frame to detect notify triggers

    TArray<UAnimSequence*> CompatibleAnimations;
    bool bShowOnlyCompatible = false;
    
    TArray<FNotifyTrack> NotifyTracks;

    bool bFoldNotifies = false;
    bool bFoldCurves = false;
    bool bFoldAttributes = false;

    FSelectedNotify SelectedNotify;

    bool bIsDirty = false;

    // Additive bone transforms applied on top of animation
    TMap<int32, FTransform> BoneAdditiveTransforms;

    // 기즈모 드래그 첫 프레임 감지용 (부동소수점 오차로 인한 불필요한 업데이트 방지)
    bool bWasGizmoDragging = false;
};

// Shape 타입 열거형
enum class EShapeType : uint8
{
    None,
    Sphere,
    Box,
    Capsule
};

// Physics Asset 에디터 상태
struct PhysicsAssetEditorState : public ViewerState
{
    // ==== 편집 대상 ====
    class UPhysicsAsset* EditingAsset = nullptr; // 현재 편집 중인 Physics Asset

    // ==== 선택 상태 ====
    int32 SelectedBodyIndex = -1;               // 선택된 Body 인덱스
    int32 SelectedConstraintIndex = -1;         // 선택된 Constraint 인덱스
    int32 SelectedShapeIndex = -1;              // 선택된 Shape 인덱스
    EShapeType SelectedShapeType = EShapeType::None;  // 선택된 Shape 타입

    // ==== 선택 소스 (색상 구분용) ====
    enum class ESelectionSource { TreeOrViewport, Graph }; // 트리/뷰포트 : 파란색, 그래프 : 보라색
    ESelectionSource SelectionSource = ESelectionSource::TreeOrViewport;

    // ==== 표시 옵션 ====
    bool bShowBodies = true;
    bool bShowConstraints = true;
    bool bShowMesh = true;
    bool bShowBoneNames = false;

    // ==== Shape 시각화 ====
    class ULineComponent* ShapeLineComponent = nullptr;  // Shape 와이어프레임용
    bool bShapesDirty = true;  // Shape 라인 재구성 필요 플래그

    // ==== Shape 기즈모용 앵커 ====
    UShapeAnchorComponent* ShapeGizmoAnchor = nullptr;   // 선택된 Shape의 기즈모 앵커

    // ==== Constraint 기즈모용 앵커 ====
    UConstraintAnchorComponent* ConstraintGizmoAnchor = nullptr;  // 선택된 Constraint의 기즈모 앵커

    // ==== Constraint 시각화 ====
    class ULineComponent* ConstraintLineComponent = nullptr;  // Constraint 와이어프레임용
    bool bConstraintsDirty = true;  // Constraint 라인 재구성 필요 플래그

    // ==== Constraint 생성 중간 상태 ====
    int32 ConstraintStartBodyIndex = -1;  // -1이면 Constraint 생성 대기 안함

    // ==== 바닥 (시뮬레이션용) ====
    class AStaticMeshActor* FloorMeshActor = nullptr;    // 시각적 바닥 (StaticMesh)
    class AActor* FloorCollisionActor = nullptr;         // 물리 바닥 (BoxComponent)

    // ==== Graph View 상태
    FVector2D GraphOffset = FVector2D::Zero();  // 그래프 패닝 오프셋
    float GraphZoom = 1.0f;                     // 그래프 줌 레벨

    // 파일 상태
    FString CurrentFilePath;
    // bIsDirty는 부모 클래스에 있음
};

// 파티클 에디터 상태
struct ParticleEditorState : public ViewerState
{
    // 파티클 시스템
    UParticleSystem* EditingTemplate = nullptr;
    UParticleSystemComponent* PreviewComponent = nullptr;
    AActor* PreviewActor = nullptr;

    // 원점축 라인 컴포넌트 (탭별로 소유)
    class ULineComponent* OriginAxisLineComponent = nullptr;

    // Bounds 와이어프레임 라인 컴포넌트
    class ULineComponent* BoundsLineComponent = nullptr;

    // Bounds 캐시 (최대 크기 추적용 - 언리얼 방식)
    FVector CachedBoundsMin = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector CachedBoundsMax = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    bool bBoundsNeedRebuild = true;  // 라인 재생성 필요 플래그

    // Bounds 리셋 (파티클 시스템 재시작 시 호출)
    void ResetBounds()
    {
        CachedBoundsMin = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
        CachedBoundsMax = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        bBoundsNeedRebuild = true;
    }

    // 선택 상태
    int32 SelectedEmitterIndex = -1;
    int32 SelectedModuleIndex = -1;
    UParticleModule* SelectedModule = nullptr;

    // 파일 경로
    FString CurrentFilePath;
    bool bIsDirty = false;

    // 시뮬레이션 제어
    bool bIsSimulating = true;
    float SimulationSpeed = 1.0f;

    // 뷰포트 표시 옵션
    bool bShowBounds = false;
    bool bShowOriginAxis = false;
    FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // LOD 제어
    int32 CurrentLODLevel = 0;

    // LOD 레벨 변경 (모듈 선택 초기화 포함)
    void SetLODLevel(int32 NewLevel)
    {
        if (CurrentLODLevel != NewLevel)
        {
            CurrentLODLevel = NewLevel;
            // LOD 변경 시 모듈 선택 초기화 (다른 LOD의 모듈을 편집하는 버그 방지)
            SelectedModule = nullptr;
            SelectedModuleIndex = -1;
        }
    }
};
