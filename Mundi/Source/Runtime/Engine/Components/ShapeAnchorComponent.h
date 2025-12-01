#pragma once

#include "SceneComponent.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"

class UBodySetup;
class USkeletalMeshComponent;

// Shape 기즈모용 앵커 컴포넌트
// 기즈모 조작 시 Shape 데이터(Center, Rotation, Scale)를 업데이트
class UShapeAnchorComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UShapeAnchorComponent, USceneComponent)

    // Shape 타겟 설정
    void SetTarget(UBodySetup* InBody, EShapeType InShapeType, int32 InShapeIndex,
                   USkeletalMeshComponent* InMeshComp, int32 InBoneIndex);

    // 타겟 정보 클리어
    void ClearTarget();

    // Shape 월드 트랜스폼으로부터 앵커 위치 업데이트
    void UpdateAnchorFromShape();

    // 기즈모 조작 시 호출 - Shape 데이터에 변경사항 적용
    void OnTransformUpdated() override;

    // Getter
    UBodySetup* GetTargetBody() const { return TargetBody; }
    EShapeType GetShapeType() const { return ShapeType; }
    int32 GetShapeIndex() const { return ShapeIndex; }

    // Dirty 플래그 (외부에서 Shape 변경 감지용)
    bool bShapeModified = false;

    // 기즈모 조작 중 플래그 (true일 때 UpdateAnchorFromShape() 무시)
    bool bIsBeingManipulated = false;

private:
    // UpdateAnchorFromShape() 실행 중 플래그 (OnTransformUpdated에서 Shape 수정 방지)
    bool bUpdatingFromShape = false;

    // 마지막으로 설정한 앵커 트랜스폼 (변화 감지용)
    FVector LastAnchorLocation;
    FQuat LastAnchorRotation;
    FVector LastAnchorScale;

    UBodySetup* TargetBody = nullptr;
    EShapeType ShapeType = EShapeType::None;
    int32 ShapeIndex = -1;
    USkeletalMeshComponent* MeshComp = nullptr;
    int32 BoneIndex = -1;

    // 본 월드 트랜스폼 캐시 (로컬 변환 계산용)
    FTransform CachedBoneWorldTransform;

    // 초기 Shape 크기 캐시 (스케일 적용용)
    FVector InitialShapeSize;  // Sphere: (Radius,0,0), Box: (X,Y,Z), Capsule: (Radius,Length,0)
};
