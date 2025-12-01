#include "pch.h"
#include "ConstraintAnchorComponent.h"
#include "SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Physics/ConstraintInstance.h"

IMPLEMENT_CLASS(UConstraintAnchorComponent)

void UConstraintAnchorComponent::SetTarget(FConstraintInstance* InConstraint,
                                            USkeletalMeshComponent* InMeshComp, int32 InBone1Index)
{
    TargetConstraint = InConstraint;
    MeshComp = InMeshComp;
    Bone1Index = InBone1Index;
    bConstraintModified = false;
    bIsBeingManipulated = false;
    bUpdatingFromConstraint = false;

    // Last 트랜스폼 초기화
    LastAnchorLocation = FVector::Zero();
    LastAnchorRotation = FQuat::Identity();

    if (MeshComp && Bone1Index >= 0)
    {
        CachedBoneWorldTransform = MeshComp->GetBoneWorldTransform(Bone1Index);
    }
}

void UConstraintAnchorComponent::ClearTarget()
{
    TargetConstraint = nullptr;
    MeshComp = nullptr;
    Bone1Index = -1;
    bConstraintModified = false;
    bIsBeingManipulated = false;
}

void UConstraintAnchorComponent::UpdateAnchorFromConstraint()
{
    if (!TargetConstraint || !MeshComp || Bone1Index < 0)
        return;

    // 기즈모 조작 중에는 Constraint에서 Anchor로의 업데이트 무시
    if (bIsBeingManipulated)
        return;

    // OnTransformUpdated()에서 Constraint 수정 방지
    bUpdatingFromConstraint = true;

    CachedBoneWorldTransform = MeshComp->GetBoneWorldTransform(Bone1Index);

    // Constraint의 Position1, Rotation1을 월드 좌표로 변환
    FQuat JointRot = FQuat::MakeFromEulerZYX(TargetConstraint->Rotation1);
    FVector JointWorldPos = CachedBoneWorldTransform.Translation +
                            CachedBoneWorldTransform.Rotation.RotateVector(TargetConstraint->Position1);
    FQuat JointWorldRot = CachedBoneWorldTransform.Rotation * JointRot;

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

    if (!TargetConstraint || !MeshComp || Bone1Index < 0)
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

    // 현재 본 월드 트랜스폼
    FTransform BoneWorld = MeshComp->GetBoneWorldTransform(Bone1Index);
    FQuat BoneWorldRotInv = BoneWorld.Rotation.Inverse();

    // 본 로컬 좌표로 변환
    FVector LocalPosition = BoneWorldRotInv.RotateVector(AnchorWorldPos - BoneWorld.Translation);
    FQuat LocalRotation = BoneWorldRotInv * AnchorWorldRot;
    FVector LocalEuler = LocalRotation.ToEulerZYXDeg();

    // Constraint에 적용
    TargetConstraint->Position1 = LocalPosition;
    TargetConstraint->Rotation1 = LocalEuler;

    bConstraintModified = true;
}
