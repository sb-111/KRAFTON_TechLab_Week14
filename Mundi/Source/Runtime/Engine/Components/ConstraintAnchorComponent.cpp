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

void UConstraintAnchorComponent::BeginManipulation()
{
    if (!TargetConstraint) return;

    // 드래그 시작 시 현재 Rotation 저장 (모드별 분리 조작용)
    StartRotation1 = TargetConstraint->Rotation1;
    StartRotation2 = TargetConstraint->Rotation2;
}

void UConstraintAnchorComponent::UpdateAnchorFromConstraint()
{
    if (!TargetConstraint || !MeshComp || ParentBoneIndex < 0)
        return;

    // 기즈모 조작 중에는 Constraint에서 Anchor로의 업데이트 무시
    if (bIsBeingManipulated)
        return;

    // OnTransformUpdated()에서 Constraint 수정 방지
    bUpdatingFromConstraint = true;

    CachedParentBoneWorldTransform = MeshComp->GetBoneWorldTransform(ParentBoneIndex);
    if (ChildBoneIndex >= 0)
        CachedChildBoneWorldTransform = MeshComp->GetBoneWorldTransform(ChildBoneIndex);

    FVector JointWorldPos;
    FQuat JointWorldRot;

    // ManipulationMode에 따라 기즈모 위치/회전 결정
    switch (ManipulationMode)
    {
    case EConstraintManipulationMode::ChildOnly:
        {
            // Child frame 기준 (Position1, Rotation1)
            FQuat JointRot1 = FQuat::MakeFromEulerZYX(TargetConstraint->Rotation1);
            JointWorldPos = CachedChildBoneWorldTransform.Translation +
                            CachedChildBoneWorldTransform.Rotation.RotateVector(TargetConstraint->Position1);
            JointWorldRot = CachedChildBoneWorldTransform.Rotation * JointRot1;
        }
        break;

    case EConstraintManipulationMode::Both:
    case EConstraintManipulationMode::ParentOnly:
    default:
        {
            // Parent frame 기준 (Position2, Rotation2)
            FQuat JointRot2 = FQuat::MakeFromEulerZYX(TargetConstraint->Rotation2);
            JointWorldPos = CachedParentBoneWorldTransform.Translation +
                            CachedParentBoneWorldTransform.Rotation.RotateVector(TargetConstraint->Position2);
            JointWorldRot = CachedParentBoneWorldTransform.Rotation * JointRot2;
        }
        break;
    }

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

    if (!TargetConstraint || !MeshComp || ParentBoneIndex < 0)
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

    // ===== 본 월드 트랜스폼 가져오기 =====
    FTransform ParentBoneWorld = MeshComp->GetBoneWorldTransform(ParentBoneIndex);
    FTransform ChildBoneWorld = (ChildBoneIndex >= 0) ?
        MeshComp->GetBoneWorldTransform(ChildBoneIndex) : ParentBoneWorld;

    FQuat ParentBoneWorldRotInv = ParentBoneWorld.Rotation.Inverse();
    FQuat ChildBoneWorldRotInv = ChildBoneWorld.Rotation.Inverse();

    // ===== 위치 업데이트 (ManipulationMode에 따라 분기) =====
    if (bLocationChanged)
    {
        FVector LocalPosition1 = ChildBoneWorldRotInv.RotateVector(AnchorWorldPos - ChildBoneWorld.Translation);
        FVector LocalPosition2 = ParentBoneWorldRotInv.RotateVector(AnchorWorldPos - ParentBoneWorld.Translation);

        switch (ManipulationMode)
        {
        case EConstraintManipulationMode::Both:
            TargetConstraint->Position1 = LocalPosition1;
            TargetConstraint->Position2 = LocalPosition2;
            break;
        case EConstraintManipulationMode::ParentOnly:
            TargetConstraint->Position2 = LocalPosition2;
            break;
        case EConstraintManipulationMode::ChildOnly:
            TargetConstraint->Position1 = LocalPosition1;
            break;
        }
    }

    // ===== 회전 업데이트 (ManipulationMode에 따라 분기) =====
    if (bRotationChanged)
    {
        // Child frame (Rotation1) 계산
        auto CalculateRotation1 = [&]() -> FVector
        {
            FQuat LocalRotation1 = ChildBoneWorldRotInv * AnchorWorldRot;
            FVector NewEuler1 = LocalRotation1.ToEulerZYXDeg();
            FVector OldEuler1 = TargetConstraint->Rotation1;
            for (int i = 0; i < 3; ++i)
            {
                float Diff = NewEuler1[i] - OldEuler1[i];
                if (Diff > 180.0f) NewEuler1[i] -= 360.0f;
                else if (Diff < -180.0f) NewEuler1[i] += 360.0f;
            }
            return NewEuler1;
        };

        // Parent frame (Rotation2) 계산
        auto CalculateRotation2 = [&]() -> FVector
        {
            FQuat LocalRotation2 = ParentBoneWorldRotInv * AnchorWorldRot;
            FVector NewEuler2 = LocalRotation2.ToEulerZYXDeg();
            FVector OldEuler2 = TargetConstraint->Rotation2;
            for (int i = 0; i < 3; ++i)
            {
                float Diff = NewEuler2[i] - OldEuler2[i];
                if (Diff > 180.0f) NewEuler2[i] -= 360.0f;
                else if (Diff < -180.0f) NewEuler2[i] += 360.0f;
            }
            return NewEuler2;
        };

        switch (ManipulationMode)
        {
        case EConstraintManipulationMode::Both:
            {
                // 일반 회전: 월드 스페이스에서 같은 delta를 Rotation1, Rotation2에 적용
                // 두 본이 다른 방향을 가지므로, 월드 스페이스 delta를 각각의 로컬 스페이스로 변환해야 함

                // 1. Parent frame 시작/현재 월드 회전
                FQuat StartRot2Quat = FQuat::MakeFromEulerZYX(StartRotation2);
                FQuat StartWorld2 = ParentBoneWorld.Rotation * StartRot2Quat;
                FQuat CurrentWorld2 = AnchorWorldRot;  // 현재 앵커의 월드 회전

                // 2. 월드 스페이스 delta 계산 (왼쪽 곱셈 형태 = 월드 축 기준 회전)
                FQuat WorldDelta = CurrentWorld2 * StartWorld2.Inverse();

                // 3. Rotation2 업데이트 (Parent bone 기준)
                FQuat LocalRotation2 = ParentBoneWorldRotInv * AnchorWorldRot;
                FVector NewEuler2 = LocalRotation2.ToEulerZYXDeg();
                FVector OldEuler2 = TargetConstraint->Rotation2;
                for (int i = 0; i < 3; ++i)
                {
                    float Diff = NewEuler2[i] - OldEuler2[i];
                    if (Diff > 180.0f) NewEuler2[i] -= 360.0f;
                    else if (Diff < -180.0f) NewEuler2[i] += 360.0f;
                }
                TargetConstraint->Rotation2 = NewEuler2;

                // 4. Rotation1 업데이트 (Child bone 기준으로 같은 월드 delta 적용)
                FQuat StartRot1Quat = FQuat::MakeFromEulerZYX(StartRotation1);
                FQuat StartWorld1 = ChildBoneWorld.Rotation * StartRot1Quat;
                FQuat NewWorld1 = WorldDelta * StartWorld1;  // 월드 delta 적용
                FQuat NewLocalRot1 = ChildBoneWorldRotInv * NewWorld1;  // Child bone 로컬로 변환
                FVector NewEuler1 = NewLocalRot1.ToEulerZYXDeg();
                FVector OldEuler1 = TargetConstraint->Rotation1;
                for (int i = 0; i < 3; ++i)
                {
                    float Diff = NewEuler1[i] - OldEuler1[i];
                    if (Diff > 180.0f) NewEuler1[i] -= 360.0f;
                    else if (Diff < -180.0f) NewEuler1[i] += 360.0f;
                }
                TargetConstraint->Rotation1 = NewEuler1;
            }
            break;

        case EConstraintManipulationMode::ParentOnly:
            // ALT: Rotation2만 업데이트 (부채꼴 방향 변경)
            TargetConstraint->Rotation2 = CalculateRotation2();
            break;

        case EConstraintManipulationMode::ChildOnly:
            // SHIFT+ALT: Rotation1만 업데이트 (화살표 위치 조정)
            TargetConstraint->Rotation1 = CalculateRotation1();
            break;
        }
    }

    bConstraintModified = true;
}
