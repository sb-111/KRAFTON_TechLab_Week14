#include "pch.h"
#include "ConstraintAnchorComponent.h"
#include "SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Physics/ConstraintInstance.h"

IMPLEMENT_CLASS(UConstraintAnchorComponent)

void UConstraintAnchorComponent::SetTarget(FConstraintInstance* InConstraint,
                                            USkeletalMeshComponent* InMeshComp, int32 InChildBoneIndex, int32 InParentBoneIndex)
{
    TargetConstraint = InConstraint;
    MeshComp = InMeshComp;
    ChildBoneIndex = InChildBoneIndex;    // Bone1 = Child
    ParentBoneIndex = InParentBoneIndex;  // Bone2 = Parent
    bConstraintModified = false;
    bIsBeingManipulated = false;
    bUpdatingFromConstraint = false;

    // Last 트랜스폼 초기화
    LastAnchorLocation = FVector::Zero();
    LastAnchorRotation = FQuat::Identity();

    if (MeshComp)
    {
        if (ParentBoneIndex >= 0)
            CachedParentBoneWorldTransform = MeshComp->GetBoneWorldTransform(ParentBoneIndex);
        if (ChildBoneIndex >= 0)
            CachedChildBoneWorldTransform = MeshComp->GetBoneWorldTransform(ChildBoneIndex);
    }
}

void UConstraintAnchorComponent::ClearTarget()
{
    TargetConstraint = nullptr;
    MeshComp = nullptr;
    ChildBoneIndex = -1;   // Bone1 = Child
    ParentBoneIndex = -1;  // Bone2 = Parent
    bConstraintModified = false;
    bIsBeingManipulated = false;
}

void UConstraintAnchorComponent::UpdateAnchorFromConstraint()
{
    if (!TargetConstraint || !MeshComp || ChildBoneIndex < 0)
        return;

    // 기즈모 조작 중에는 Constraint에서 Anchor로의 업데이트 무시
    if (bIsBeingManipulated)
        return;

    // OnTransformUpdated()에서 Constraint 수정 방지
    bUpdatingFromConstraint = true;

    CachedChildBoneWorldTransform = MeshComp->GetBoneWorldTransform(ChildBoneIndex);
    if (ParentBoneIndex >= 0)
        CachedParentBoneWorldTransform = MeshComp->GetBoneWorldTransform(ParentBoneIndex);

    // Constraint의 Position1, Rotation1을 월드 좌표로 변환 (언리얼 규칙: Bone1 = Child)
    FQuat JointRot = FQuat::MakeFromEulerZYX(TargetConstraint->Rotation1);
    FVector JointWorldPos = CachedChildBoneWorldTransform.Translation +
                            CachedChildBoneWorldTransform.Rotation.RotateVector(TargetConstraint->Position1);
    FQuat JointWorldRot = CachedChildBoneWorldTransform.Rotation * JointRot;

    SetWorldLocation(JointWorldPos);
    SetWorldRotation(JointWorldRot);
    // 스케일은 무시 (Constraint에는 스케일 개념 없음)
    SetWorldScale(FVector::One());

    // 현재 트랜스폼 저장 (변화 감지용)
    LastAnchorLocation = JointWorldPos;
    LastAnchorRotation = JointWorldRot;

    bUpdatingFromConstraint = false;
}

void UConstraintAnchorComponent::OnTransformUpdated()
{
    USceneComponent::OnTransformUpdated();

    if (!TargetConstraint || !MeshComp || ChildBoneIndex < 0)
        return;

    // UpdateAnchorFromConstraint() 실행 중에는 Constraint 수정하지 않음
    if (bUpdatingFromConstraint)
        return;

    // 기즈모가 실제로 드래그 중일 때만 Constraint 수정
    if (!bIsBeingManipulated)
        return;

    // 앵커의 월드 위치/회전
    FVector AnchorWorldPos = GetWorldLocation();
    FQuat AnchorWorldRot = GetWorldRotation();

    // 실제로 의미있는 변화가 있는지 체크
    const float PosTolerance = 0.0001f;
    const float RotTolerance = 0.0001f;

    bool bLocationChanged = (AnchorWorldPos - LastAnchorLocation).SizeSquared() > PosTolerance * PosTolerance;
    bool bRotationChanged = FMath::Abs(AnchorWorldRot.X - LastAnchorRotation.X) > RotTolerance ||
                            FMath::Abs(AnchorWorldRot.Y - LastAnchorRotation.Y) > RotTolerance ||
                            FMath::Abs(AnchorWorldRot.Z - LastAnchorRotation.Z) > RotTolerance ||
                            FMath::Abs(AnchorWorldRot.W - LastAnchorRotation.W) > RotTolerance;

    // 변화가 없으면 Constraint 수정 안 함
    if (!bLocationChanged && !bRotationChanged)
        return;

    // 현재 트랜스폼 저장
    LastAnchorLocation = AnchorWorldPos;
    LastAnchorRotation = AnchorWorldRot;

    // ===== Child 본 (Bone1) 기준으로 Position1, Rotation1 계산 (언리얼 규칙) =====
    FTransform ChildBoneWorld = MeshComp->GetBoneWorldTransform(ChildBoneIndex);
    FQuat ChildBoneWorldRotInv = ChildBoneWorld.Rotation.Inverse();

    FVector LocalPosition1 = ChildBoneWorldRotInv.RotateVector(AnchorWorldPos - ChildBoneWorld.Translation);
    FQuat LocalRotation1 = ChildBoneWorldRotInv * AnchorWorldRot;

    TargetConstraint->Position1 = LocalPosition1;
    TargetConstraint->Rotation1 = LocalRotation1.ToEulerZYXDeg();

    // ===== Parent 본 (Bone2) 기준으로 Position2, Rotation2 계산 (언리얼 규칙) =====
    if (ParentBoneIndex >= 0)
    {
        FTransform ParentBoneWorld = MeshComp->GetBoneWorldTransform(ParentBoneIndex);
        FQuat ParentBoneWorldRotInv = ParentBoneWorld.Rotation.Inverse();

        FVector LocalPosition2 = ParentBoneWorldRotInv.RotateVector(AnchorWorldPos - ParentBoneWorld.Translation);
        FQuat LocalRotation2 = ParentBoneWorldRotInv * AnchorWorldRot;

        TargetConstraint->Position2 = LocalPosition2;
        TargetConstraint->Rotation2 = LocalRotation2.ToEulerZYXDeg();
    }

    bConstraintModified = true;
}
