#pragma once

#include "SceneComponent.h"

struct FConstraintInstance;
class USkeletalMeshComponent;

// Constraint 기즈모용 앵커 컴포넌트
// 기즈모 조작 시 Constraint 데이터(Position1, Rotation1)를 업데이트
// 언리얼과 동일하게 스케일 조작은 지원하지 않음
class UConstraintAnchorComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UConstraintAnchorComponent, USceneComponent)

    // Constraint 타겟 설정
    void SetTarget(FConstraintInstance* InConstraint,
                   USkeletalMeshComponent* InMeshComp, int32 InBone1Index);

    // 타겟 정보 클리어
    void ClearTarget();

    // Constraint 월드 트랜스폼으로부터 앵커 위치 업데이트
    void UpdateAnchorFromConstraint();

    // 기즈모 조작 시 호출 - Constraint 데이터에 변경사항 적용
    void OnTransformUpdated() override;

    // Getter
    FConstraintInstance* GetTargetConstraint() const { return TargetConstraint; }

    // Dirty 플래그 (외부에서 Constraint 변경 감지용)
    bool bConstraintModified = false;

    // 기즈모 조작 중 플래그 (true일 때 UpdateAnchorFromConstraint() 무시)
    bool bIsBeingManipulated = false;

private:
    // UpdateAnchorFromConstraint() 실행 중 플래그 (OnTransformUpdated에서 Constraint 수정 방지)
    bool bUpdatingFromConstraint = false;

    // 마지막으로 설정한 앵커 트랜스폼 (변화 감지용)
    FVector LastAnchorLocation;
    FQuat LastAnchorRotation;

    FConstraintInstance* TargetConstraint = nullptr;
    USkeletalMeshComponent* MeshComp = nullptr;
    int32 Bone1Index = -1;

    // 본 월드 트랜스폼 캐시 (로컬 변환 계산용)
    FTransform CachedBoneWorldTransform;
};
