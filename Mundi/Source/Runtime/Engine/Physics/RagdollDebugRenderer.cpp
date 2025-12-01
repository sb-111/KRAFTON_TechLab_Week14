#include "pch.h"
#include "RagdollDebugRenderer.h"
#include "SkeletalMeshComponent.h"
#include "BodyInstance.h"
#include "BodySetup.h"
#include "AggregateGeometry.h"
#include "PhysxConverter.h"
#include "Renderer.h"

void FRagdollDebugRenderer::RenderSkeletalMeshRagdoll(
    URenderer* Renderer,
    const USkeletalMeshComponent* SkelMeshComp,
    const FVector4& BoneColor,
    const FVector4& JointColor)
{
    if (!Renderer || !SkelMeshComp) return;
    // IsSimulatingPhysics() 조건 제거: 에디터 미리보기에서도 렌더링되도록
    // Bodies가 있으면 렌더링 (호출자가 이미 Bodies 유무를 체크함)

    const TArray<FBodyInstance*>& Bodies = SkelMeshComp->GetBodies();
    const TArray<int32>& ParentIndices = SkelMeshComp->GetBodyParentIndices();

    if (Bodies.Num() == 0) return;

    TArray<FVector> StartPoints;
    TArray<FVector> EndPoints;
    TArray<FVector4> Colors;

    for (int32 i = 0; i < Bodies.Num(); ++i)
    {
        const FBodyInstance* Body = Bodies[i];
        if (!Body || !Body->IsValidBodyInstance() || !Body->BodySetup) continue;

        PxRigidDynamic* RigidBody = Body->GetPxRigidDynamic();
        if (!RigidBody) continue;

        PxTransform WorldTransform = RigidBody->getGlobalPose();

        // UBodySetup의 AggGeom에서 Shape들 렌더링
        RenderAggGeom(Renderer, Body->BodySetup->AggGeom, WorldTransform, BoneColor,
                      StartPoints, EndPoints, Colors);

        // 부모와 연결선 (Joint 시각화)
        int32 ParentIdx = (i < ParentIndices.Num()) ? ParentIndices[i] : -1;
        if (ParentIdx >= 0 && ParentIdx < Bodies.Num())
        {
            const FBodyInstance* ParentBody = Bodies[ParentIdx];
            if (ParentBody && ParentBody->IsValidBodyInstance())
            {
                PxRigidDynamic* ParentRigidBody = ParentBody->GetPxRigidDynamic();
                if (ParentRigidBody)
                {
                    PxVec3 Start = WorldTransform.p;
                    PxVec3 End = ParentRigidBody->getGlobalPose().p;

                    StartPoints.Add(PhysxConverter::ToFVector(Start));
                    EndPoints.Add(PhysxConverter::ToFVector(End));
                    Colors.Add(JointColor);
                }
            }
        }
    }

    // 라인 렌더링
    if (StartPoints.Num() > 0)
    {
        Renderer->AddLines(StartPoints, EndPoints, Colors);
    }
}

void FRagdollDebugRenderer::RenderAggGeom(
    URenderer* Renderer,
    const FKAggregateGeom& AggGeom,
    const PxTransform& WorldTransform,
    const FVector4& Color,
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors)
{
    // Sphere Shape들
    for (int32 i = 0; i < AggGeom.SphereElems.Num(); ++i)
    {
        RenderSphere(AggGeom.SphereElems[i], WorldTransform, Color,
                     OutStartPoints, OutEndPoints, OutColors);
    }

    // Box Shape들
    for (int32 i = 0; i < AggGeom.BoxElems.Num(); ++i)
    {
        RenderBox(AggGeom.BoxElems[i], WorldTransform, Color,
                  OutStartPoints, OutEndPoints, OutColors);
    }

    // Capsule Shape들
    for (int32 i = 0; i < AggGeom.SphylElems.Num(); ++i)
    {
        RenderCapsule(AggGeom.SphylElems[i], WorldTransform, Color,
                      OutStartPoints, OutEndPoints, OutColors);
    }
}

void FRagdollDebugRenderer::RenderSphere(
    const FKSphereElem& Sphere,
    const PxTransform& WorldTransform,
    const FVector4& Color,
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors)
{
    // 로컬 위치를 월드로 변환
    PxVec3 LocalCenter = PhysxConverter::ToPxVec3(Sphere.Center);
    PxVec3 WorldCenter = WorldTransform.transform(LocalCenter);
    FVector Center = PhysxConverter::ToFVector(WorldCenter);

    // 3개 축의 원으로 구 표현
    const int32 NumSegments = 16;

    // XY 평면 원
    AddCircle(Center, FVector(0, 0, 1), Sphere.Radius, NumSegments, Color,
              OutStartPoints, OutEndPoints, OutColors);
    // XZ 평면 원
    AddCircle(Center, FVector(0, 1, 0), Sphere.Radius, NumSegments, Color,
              OutStartPoints, OutEndPoints, OutColors);
    // YZ 평면 원
    AddCircle(Center, FVector(1, 0, 0), Sphere.Radius, NumSegments, Color,
              OutStartPoints, OutEndPoints, OutColors);
}

void FRagdollDebugRenderer::RenderBox(
    const FKBoxElem& Box,
    const PxTransform& WorldTransform,
    const FVector4& Color,
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors)
{
    // 로컬 회전 (Euler angles in degrees)
    FQuat LocalRotation = FQuat::MakeFromEulerZYX(Box.Rotation);
    PxQuat LocalPxRot = PhysxConverter::ToPxQuat(LocalRotation);
    PxTransform LocalTransform(PhysxConverter::ToPxVec3(Box.Center), LocalPxRot);

    // 최종 월드 변환
    PxTransform FinalTransform = WorldTransform * LocalTransform;

    FVector Center = PhysxConverter::ToFVector(FinalTransform.p);
    FQuat Rotation = PhysxConverter::ToFQuat(FinalTransform.q);

    FVector HalfExtent(Box.X, Box.Y, Box.Z);

    // 8개 코너 계산
    FVector Corners[8];
    Corners[0] = Center + Rotation.RotateVector(FVector(-HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z));
    Corners[1] = Center + Rotation.RotateVector(FVector(+HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z));
    Corners[2] = Center + Rotation.RotateVector(FVector(+HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z));
    Corners[3] = Center + Rotation.RotateVector(FVector(-HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z));
    Corners[4] = Center + Rotation.RotateVector(FVector(-HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z));
    Corners[5] = Center + Rotation.RotateVector(FVector(+HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z));
    Corners[6] = Center + Rotation.RotateVector(FVector(+HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z));
    Corners[7] = Center + Rotation.RotateVector(FVector(-HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z));

    // 12개 엣지
    // 아래 면
    OutStartPoints.Add(Corners[0]); OutEndPoints.Add(Corners[1]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[1]); OutEndPoints.Add(Corners[2]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[2]); OutEndPoints.Add(Corners[3]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[3]); OutEndPoints.Add(Corners[0]); OutColors.Add(Color);

    // 위 면
    OutStartPoints.Add(Corners[4]); OutEndPoints.Add(Corners[5]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[5]); OutEndPoints.Add(Corners[6]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[6]); OutEndPoints.Add(Corners[7]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[7]); OutEndPoints.Add(Corners[4]); OutColors.Add(Color);

    // 수직 연결
    OutStartPoints.Add(Corners[0]); OutEndPoints.Add(Corners[4]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[1]); OutEndPoints.Add(Corners[5]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[2]); OutEndPoints.Add(Corners[6]); OutColors.Add(Color);
    OutStartPoints.Add(Corners[3]); OutEndPoints.Add(Corners[7]); OutColors.Add(Color);
}

void FRagdollDebugRenderer::RenderCapsule(
    const FKSphylElem& Capsule,
    const PxTransform& WorldTransform,
    const FVector4& Color,
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors)
{
    // 기본 축 회전: 엔진 캡슐(Z축) → PhysX 캡슐(X축)
    // RagdollSystem과 동일한 회전 적용
    FQuat BaseRotation = FQuat::MakeFromEulerZYX(FVector(-90.0f, 0.0f, 0.0f));
    FQuat UserRotation = FQuat::MakeFromEulerZYX(Capsule.Rotation);
    FQuat FinalLocalRotation = UserRotation * BaseRotation;

    PxQuat LocalPxRot = PhysxConverter::ToPxQuat(FinalLocalRotation);
    PxTransform LocalTransform(PhysxConverter::ToPxVec3(Capsule.Center), LocalPxRot);

    // 최종 월드 변환
    PxTransform FinalTransform = WorldTransform * LocalTransform;

    FVector Center = PhysxConverter::ToFVector(FinalTransform.p);
    FQuat Rotation = PhysxConverter::ToFQuat(FinalTransform.q);

    float Radius = Capsule.Radius;
    float HalfLength = Capsule.Length / 2.0f;

    // PhysX 캡슐은 PhysX-X축 방향이 기본
    // 좌표계 변환 후: PhysX X → Engine Y
    // ToFVector: (x,y,z) → (-z, x, y) 이므로 PhysX(1,0,0) → Engine(0,1,0)
    FVector CapsuleAxis = Rotation.RotateVector(FVector(0, 1, 0));  // Engine Y = PhysX X
    FVector AxisY = Rotation.RotateVector(FVector(0, 0, 1));  // 원 그리기용
    FVector AxisZ = Rotation.RotateVector(FVector(1, 0, 0));  // 원 그리기용

    FVector TopCenter = Center + CapsuleAxis * HalfLength;
    FVector BottomCenter = Center - CapsuleAxis * HalfLength;

    const int32 NumSegments = 16;

    // 상단 원 (캡슐 축에 수직인 원)
    AddCircle(TopCenter, CapsuleAxis, Radius, NumSegments, Color,
              OutStartPoints, OutEndPoints, OutColors);

    // 하단 원
    AddCircle(BottomCenter, CapsuleAxis, Radius, NumSegments, Color,
              OutStartPoints, OutEndPoints, OutColors);

    // 세로 라인 (4개)
    for (int32 i = 0; i < 4; ++i)
    {
        float Angle = i * PxPi / 2.0f;
        FVector Offset = (AxisY * std::cos(Angle) + AxisZ * std::sin(Angle)) * Radius;

        FVector TopPoint = TopCenter + Offset;
        FVector BottomPoint = BottomCenter + Offset;

        OutStartPoints.Add(TopPoint);
        OutEndPoints.Add(BottomPoint);
        OutColors.Add(Color);
    }

    // 반구 표현 (간단화: 추가 원 2개)
    AddCircle(TopCenter, AxisY, Radius, NumSegments / 2, Color,
              OutStartPoints, OutEndPoints, OutColors);
    AddCircle(BottomCenter, AxisY, Radius, NumSegments / 2, Color,
              OutStartPoints, OutEndPoints, OutColors);
}

void FRagdollDebugRenderer::AddCircle(
    const FVector& Center,
    const FVector& Normal,
    float Radius,
    int32 NumSegments,
    const FVector4& Color,
    TArray<FVector>& OutStartPoints,
    TArray<FVector>& OutEndPoints,
    TArray<FVector4>& OutColors)
{
    // Normal에 수직인 두 벡터 계산
    FVector Tangent, Bitangent;

    if (FMath::Abs(Normal.Z) < 0.9f)
    {
        Tangent = FVector::Cross(Normal, FVector(0, 0, 1)).GetNormalized();
    }
    else
    {
        Tangent = FVector::Cross(Normal, FVector(1, 0, 0)).GetNormalized();
    }
    Bitangent = FVector::Cross(Normal, Tangent).GetNormalized();

    FVector PrevPoint = Center + Tangent * Radius;

    for (int32 i = 1; i <= NumSegments; ++i)
    {
        float Angle = (2.0f * PxPi * i) / NumSegments;
        FVector CurrentPoint = Center + (Tangent * std::cos(Angle) + Bitangent * std::sin(Angle)) * Radius;

        OutStartPoints.Add(PrevPoint);
        OutEndPoints.Add(CurrentPoint);
        OutColors.Add(Color);

        PrevPoint = CurrentPoint;
    }
}
