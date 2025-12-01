#include "pch.h"
#include "ShapeAnchorComponent.h"
#include "SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Physics/BodySetup.h"
#include "Source/Runtime/Engine/Physics/AggregateGeometry.h"

IMPLEMENT_CLASS(UShapeAnchorComponent)

void UShapeAnchorComponent::SetTarget(UBodySetup* InBody, EShapeType InShapeType, int32 InShapeIndex,
                                       USkeletalMeshComponent* InMeshComp, int32 InBoneIndex)
{
    TargetBody = InBody;
    ShapeType = InShapeType;
    ShapeIndex = InShapeIndex;
    MeshComp = InMeshComp;
    BoneIndex = InBoneIndex;
    bShapeModified = false;
    bIsBeingManipulated = false;  // 새 타겟 설정 시 조작 플래그 리셋
    bUpdatingFromShape = false;

    // Last 트랜스폼 초기화 (첫 UpdateAnchorFromShape에서 설정됨)
    LastAnchorLocation = FVector::Zero();
    LastAnchorRotation = FQuat::Identity();
    LastAnchorScale = FVector::Zero();  // Zero로 설정해서 첫 업데이트 시 무조건 갱신되게

    if (MeshComp && BoneIndex >= 0)
    {
        CachedBoneWorldTransform = MeshComp->GetBoneWorldTransform(BoneIndex);
    }

    // 초기 Shape 크기 캐시
    if (TargetBody && ShapeIndex >= 0)
    {
        switch (ShapeType)
        {
        case EShapeType::Sphere:
            if (ShapeIndex < TargetBody->AggGeom.SphereElems.Num())
                InitialShapeSize = FVector(TargetBody->AggGeom.SphereElems[ShapeIndex].Radius, 0, 0);
            break;
        case EShapeType::Box:
            if (ShapeIndex < TargetBody->AggGeom.BoxElems.Num())
            {
                const FKBoxElem& Box = TargetBody->AggGeom.BoxElems[ShapeIndex];
                InitialShapeSize = FVector(Box.X, Box.Y, Box.Z);
            }
            break;
        case EShapeType::Capsule:
            if (ShapeIndex < TargetBody->AggGeom.SphylElems.Num())
            {
                const FKSphylElem& Capsule = TargetBody->AggGeom.SphylElems[ShapeIndex];
                InitialShapeSize = FVector(Capsule.Radius, Capsule.Length, 0);
            }
            break;
        default:
            InitialShapeSize = FVector::One();
            break;
        }
    }
}

void UShapeAnchorComponent::ClearTarget()
{
    TargetBody = nullptr;
    ShapeType = EShapeType::None;
    ShapeIndex = -1;
    MeshComp = nullptr;
    BoneIndex = -1;
    bShapeModified = false;
    bIsBeingManipulated = false;
    InitialShapeSize = FVector::One();
}

void UShapeAnchorComponent::UpdateAnchorFromShape()
{
    if (!TargetBody || !MeshComp || BoneIndex < 0 || ShapeIndex < 0)
        return;

    // 기즈모 조작 중에는 Shape에서 Anchor로의 업데이트 무시
    if (bIsBeingManipulated)
        return;

    // OnTransformUpdated()에서 Shape 수정 방지
    bUpdatingFromShape = true;

    CachedBoneWorldTransform = MeshComp->GetBoneWorldTransform(BoneIndex);

    FVector ShapeCenter;
    FQuat ShapeRotation;

    switch (ShapeType)
    {
    case EShapeType::Sphere:
    {
        if (ShapeIndex >= TargetBody->AggGeom.SphereElems.Num()) return;
        const FKSphereElem& Sphere = TargetBody->AggGeom.SphereElems[ShapeIndex];
        ShapeCenter = CachedBoneWorldTransform.Translation +
                      CachedBoneWorldTransform.Rotation.RotateVector(Sphere.Center);
        ShapeRotation = CachedBoneWorldTransform.Rotation;
        // 스케일을 Shape 크기로 설정 (균일 스케일)
        SetWorldScale(FVector(Sphere.Radius, Sphere.Radius, Sphere.Radius));
        InitialShapeSize = FVector(Sphere.Radius, 0, 0);
        break;
    }
    case EShapeType::Box:
    {
        if (ShapeIndex >= TargetBody->AggGeom.BoxElems.Num()) return;
        const FKBoxElem& Box = TargetBody->AggGeom.BoxElems[ShapeIndex];
        ShapeCenter = CachedBoneWorldTransform.Translation +
                      CachedBoneWorldTransform.Rotation.RotateVector(Box.Center);
        FQuat BoxRotation = FQuat::MakeFromEulerZYX(Box.Rotation);
        ShapeRotation = CachedBoneWorldTransform.Rotation * BoxRotation;
        // 스케일을 HalfExtent로 설정
        SetWorldScale(FVector(Box.X, Box.Y, Box.Z));
        InitialShapeSize = FVector(Box.X, Box.Y, Box.Z);
        break;
    }
    case EShapeType::Capsule:
    {
        if (ShapeIndex >= TargetBody->AggGeom.SphylElems.Num()) return;
        const FKSphylElem& Capsule = TargetBody->AggGeom.SphylElems[ShapeIndex];
        ShapeCenter = CachedBoneWorldTransform.Translation +
                      CachedBoneWorldTransform.Rotation.RotateVector(Capsule.Center);
        FQuat CapsuleRotation = FQuat::MakeFromEulerZYX(Capsule.Rotation);
        ShapeRotation = CachedBoneWorldTransform.Rotation * CapsuleRotation;
        // 캡슐: 균일 스케일 1로 설정 (스케일 대신 직접 Radius/Length 조작)
        SetWorldScale(FVector::One());
        InitialShapeSize = FVector(Capsule.Radius, Capsule.Length, 0);
        break;
    }
    default:
        return;
    }

    SetWorldLocation(ShapeCenter);
    SetWorldRotation(ShapeRotation);

    // 현재 트랜스폼 저장 (변화 감지용)
    LastAnchorLocation = ShapeCenter;
    LastAnchorRotation = ShapeRotation;
    LastAnchorScale = GetWorldScale();

    bUpdatingFromShape = false;
}

void UShapeAnchorComponent::OnTransformUpdated()
{
    USceneComponent::OnTransformUpdated();

    if (!TargetBody || !MeshComp || BoneIndex < 0 || ShapeIndex < 0)
        return;

    // UpdateAnchorFromShape() 실행 중에는 Shape 수정하지 않음
    if (bUpdatingFromShape)
        return;

    // 기즈모가 실제로 드래그 중일 때만 Shape 수정
    if (!bIsBeingManipulated)
        return;

    // 앵커의 월드 위치/회전/스케일
    FVector AnchorWorldPos = GetWorldLocation();
    FQuat AnchorWorldRot = GetWorldRotation();
    FVector AnchorScale = GetWorldScale();

    // 실제로 의미있는 변화가 있는지 체크 (미세한 부동소수점 오차 무시)
    const float PosTolerance = 0.0001f;
    const float RotTolerance = 0.0001f;
    const float ScaleTolerance = 0.0001f;

    bool bLocationChanged = (AnchorWorldPos - LastAnchorLocation).SizeSquared() > PosTolerance * PosTolerance;
    bool bRotationChanged = FMath::Abs(AnchorWorldRot.X - LastAnchorRotation.X) > RotTolerance ||
                            FMath::Abs(AnchorWorldRot.Y - LastAnchorRotation.Y) > RotTolerance ||
                            FMath::Abs(AnchorWorldRot.Z - LastAnchorRotation.Z) > RotTolerance ||
                            FMath::Abs(AnchorWorldRot.W - LastAnchorRotation.W) > RotTolerance;
    bool bScaleChanged = (AnchorScale - LastAnchorScale).SizeSquared() > ScaleTolerance * ScaleTolerance;

    // 변화가 없으면 Shape 수정 안 함
    if (!bLocationChanged && !bRotationChanged && !bScaleChanged)
        return;

    // 현재 트랜스폼 저장
    LastAnchorLocation = AnchorWorldPos;
    LastAnchorRotation = AnchorWorldRot;
    LastAnchorScale = AnchorScale;

    // 현재 본 월드 트랜스폼
    FTransform BoneWorld = MeshComp->GetBoneWorldTransform(BoneIndex);
    FQuat BoneWorldRotInv = BoneWorld.Rotation.Inverse();

    // 본 로컬 좌표로 변환
    FVector LocalCenter = BoneWorldRotInv.RotateVector(AnchorWorldPos - BoneWorld.Translation);
    FQuat LocalRotation = BoneWorldRotInv * AnchorWorldRot;
    FVector LocalEuler = LocalRotation.ToEulerZYXDeg();

    switch (ShapeType)
    {
    case EShapeType::Sphere:
    {
        if (ShapeIndex >= TargetBody->AggGeom.SphereElems.Num()) return;
        FKSphereElem& Sphere = TargetBody->AggGeom.SphereElems[ShapeIndex];
        Sphere.Center = LocalCenter;
        // Sphere: 균일 스케일 - 평균값 사용
        Sphere.Radius = (AnchorScale.X + AnchorScale.Y + AnchorScale.Z) / 3.0f;
        if (Sphere.Radius < 0.01f) Sphere.Radius = 0.01f;
        break;
    }
    case EShapeType::Box:
    {
        if (ShapeIndex >= TargetBody->AggGeom.BoxElems.Num()) return;
        FKBoxElem& Box = TargetBody->AggGeom.BoxElems[ShapeIndex];
        Box.Center = LocalCenter;
        Box.Rotation = LocalEuler;
        // Box: 각 축별 스케일 적용
        Box.X = FMath::Max(0.01f, AnchorScale.X);
        Box.Y = FMath::Max(0.01f, AnchorScale.Y);
        Box.Z = FMath::Max(0.01f, AnchorScale.Z);
        break;
    }
    case EShapeType::Capsule:
    {
        if (ShapeIndex >= TargetBody->AggGeom.SphylElems.Num()) return;
        FKSphylElem& Capsule = TargetBody->AggGeom.SphylElems[ShapeIndex];
        Capsule.Center = LocalCenter;
        Capsule.Rotation = LocalEuler;
        // Capsule: 스케일 무시 (위치/회전만 적용, Radius/Length는 디테일 패널에서 조작)
        break;
    }
    default:
        return;
    }

    bShapeModified = true;
}
