#include "pch.h"
#include "RagdollSystem.h"
#include "RagdollDebugRenderer.h"
#include "PhysicsSystem.h"
#include "PhysicsAsset.h"
#include "BodySetup.h"
#include "AggregateGeometry.h"
#include "PhysxConverter.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMesh.h"
#include <extensions/PxD6Joint.h>

using namespace physx;

// ===== Static Instance =====
FRagdollSystem* FRagdollSystem::Instance = nullptr;

// ===== FRagdollBone Helper Functions =====
FName FRagdollBone::GetBoneName() const
{
    return BodySetup ? BodySetup->BoneName : FName();
}

float FRagdollBone::GetMass() const
{
    return BodySetup ? BodySetup->MassInKg : 1.0f;
}

// ===== FRagdollInstance =====
void FRagdollInstance::Activate()
{
    if (bIsActive) return;

    for (FRagdollBone& Bone : Bones)
    {
        if (Bone.Body)
        {
            Bone.Body->wakeUp();
        }
    }
    bIsActive = true;
}

void FRagdollInstance::Deactivate()
{
    if (!bIsActive) return;

    for (FRagdollBone& Bone : Bones)
    {
        if (Bone.Body)
        {
            Bone.Body->putToSleep();
        }
    }
    bIsActive = false;
}

void FRagdollInstance::SyncToMesh()
{
    // UserData에서 SkeletalMeshComponent 가져오기
    USkeletalMeshComponent* SkelMeshComp = static_cast<USkeletalMeshComponent*>(UserData);
    if (!SkelMeshComp) return;

    USkeletalMesh* SkelMesh = SkelMeshComp->GetSkeletalMesh();
    if (!SkelMesh) return;

    const FSkeleton* Skeleton = SkelMesh->GetSkeleton();
    if (!Skeleton) return;

    // 각 Ragdoll 본의 물리 Transform을 메시에 적용
    for (const FRagdollBone& Bone : Bones)
    {
        if (!Bone.Body || !Bone.BodySetup) continue;

        FName BoneName = Bone.GetBoneName();

        // 스켈레톤에서 이 본의 인덱스 찾기
        const auto* BoneIndexPtr = Skeleton->BoneNameToIndex.Find(BoneName.ToString());
        if (!BoneIndexPtr) continue;

        int32 BoneIndex = *BoneIndexPtr;

        // PhysX Transform을 엔진 Transform으로 변환
        PxTransform PhysicsTransform = Bone.Body->getGlobalPose();
        FTransform WorldTransform = PhysxConverter::ToFTransform(PhysicsTransform);

        // SkeletalMeshComponent에 본 Transform 설정
        SkelMeshComp->SetBoneWorldTransform(BoneIndex, WorldTransform);
    }
}

// ===== FRagdollSystem =====
FRagdollSystem& FRagdollSystem::GetInstance()
{
    if (!Instance)
    {
        Instance = new FRagdollSystem();
    }
    return *Instance;
}

void FRagdollSystem::Initialize()
{
    // 필요시 초기화 로직 추가
}

void FRagdollSystem::Shutdown()
{
    // 모든 활성 Ragdoll 제거
    // 주의: DestroyRagdoll 내부에서 ActiveRagdolls.Remove()를 호출하므로
    // 복사본으로 순회해야 함
    TArray<FRagdollInstance*> RagdollsCopy = ActiveRagdolls;
    for (FRagdollInstance* RagdollInstance : RagdollsCopy)
    {
        DestroyRagdoll(RagdollInstance);
    }
    ActiveRagdolls.Empty();

    if (Instance)
    {
        delete Instance;
        Instance = nullptr;
    }
}

FRagdollInstance* FRagdollSystem::CreateRagdollFromPhysicsAsset(
    UPhysicsAsset* PhysicsAsset,
    const TArray<FTransform>& BoneWorldTransforms,
    void* InUserData)
{
    if (!PhysicsAsset) return nullptr;

    FPhysicsSystem& PhysicsSystem = FPhysicsSystem::GetInstance();
    if (!PhysicsSystem.GetPhysics() || !PhysicsSystem.GetScene())
    {
        return nullptr;
    }

    FRagdollInstance* NewInstance = new FRagdollInstance();
    NewInstance->PhysicsAsset = PhysicsAsset;
    NewInstance->UserData = InUserData;

    // 1단계: 각 BodySetup에서 RigidDynamic 생성
    int32 NumBodySetups = PhysicsAsset->Bodies.Num();
    for (int32 i = 0; i < NumBodySetups; ++i)
    {
        UBodySetup* BodySetup = PhysicsAsset->Bodies[i];
        if (!BodySetup) continue;

        FRagdollBone Bone;
        Bone.BodySetup = BodySetup;

        // 본 Transform 가져오기
        FTransform BoneTransform;
        if (i < BoneWorldTransforms.Num())
        {
            BoneTransform = BoneWorldTransforms[i];
        }
        else
        {
            // 기본 Transform (default constructor creates identity)
            BoneTransform = FTransform();
        }

        PxTransform InitTransform = PhysxConverter::ToPxTransform(BoneTransform);

        // RigidDynamic 생성
        PxRigidDynamic* Body = PhysicsSystem.GetPhysics()->createRigidDynamic(InitTransform);
        if (!Body) continue;

        // UBodySetup의 AggGeom에서 Shape들 생성
        CreateShapesFromBodySetup(BodySetup, Body);

        // 질량 설정 (UBodySetup에서 가져옴)
        PxRigidBodyExt::setMassAndUpdateInertia(*Body, BodySetup->MassInKg);

        // Damping 설정
        Body->setLinearDamping(BodySetup->LinearDamping);
        Body->setAngularDamping(BodySetup->AngularDamping);

        // Scene에 추가
        PhysicsSystem.GetScene()->addActor(*Body);
        Body->wakeUp();

        Bone.Body = Body;
        NewInstance->Bones.Add(Bone);
    }

    // 2단계: 부모-자식 관계 설정
    SetupBoneHierarchy(NewInstance);

    // 3단계: Joint 생성
    for (FRagdollBone& Bone : NewInstance->Bones)
    {
        if (Bone.ParentIndex >= 0 && Bone.ParentIndex < NewInstance->Bones.Num())
        {
            CreateJoint(Bone, NewInstance->Bones[Bone.ParentIndex]);
        }
    }

    ActiveRagdolls.Add(NewInstance);
    NewInstance->bIsActive = true;

    return NewInstance;
}

void FRagdollSystem::CreateShapesFromBodySetup(UBodySetup* BodySetup, PxRigidDynamic* Body)
{
    if (!BodySetup || !Body) return;

    FPhysicsSystem& PhysicsSystem = FPhysicsSystem::GetInstance();
    if (!PhysicsSystem.GetPhysics()) return;

    // UBodySetup의 물리 재질로 PxMaterial 생성
    PxMaterial* Material = PhysicsSystem.GetPhysics()->createMaterial(
        BodySetup->Friction,
        BodySetup->Friction,
        BodySetup->Restitution
    );
    if (!Material) return;

    const FKAggregateGeom& AggGeom = BodySetup->AggGeom;

    // === Sphere Shape 생성 ===
    for (int32 i = 0; i < AggGeom.SphereElems.Num(); ++i)
    {
        const FKSphereElem& Sphere = AggGeom.SphereElems[i];
        PxSphereGeometry Geom(Sphere.Radius);
        PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Body, Geom, *Material);
        if (Shape)
        {
            PxTransform LocalPose(PhysxConverter::ToPxVec3(Sphere.Center));
            Shape->setLocalPose(LocalPose);
        }
    }

    // === Box Shape 생성 ===
    for (int32 i = 0; i < AggGeom.BoxElems.Num(); ++i)
    {
        const FKBoxElem& Box = AggGeom.BoxElems[i];
        // 좌표계 변환: 엔진(X,Y,Z) → PhysX(Y,Z,-X) 후 절댓값 (BoxComponent와 동일 방식)
        PxVec3 HalfExtent = PhysxConverter::ToPxVec3(FVector(Box.X, Box.Y, Box.Z));
        HalfExtent.x = std::abs(HalfExtent.x);
        HalfExtent.y = std::abs(HalfExtent.y);
        HalfExtent.z = std::abs(HalfExtent.z);
        PxBoxGeometry Geom(HalfExtent.x, HalfExtent.y, HalfExtent.z);
        PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Body, Geom, *Material);
        if (Shape)
        {
            // 회전 적용 (Euler angles in degrees)
            FQuat Rotation = FQuat::MakeFromEulerZYX(Box.Rotation);
            PxTransform LocalPose(
                PhysxConverter::ToPxVec3(Box.Center),
                PhysxConverter::ToPxQuat(Rotation)
            );
            Shape->setLocalPose(LocalPose);
        }
    }

    // === Capsule (Sphyl) Shape 생성 ===
    for (int32 i = 0; i < AggGeom.SphylElems.Num(); ++i)
    {
        const FKSphylElem& Capsule = AggGeom.SphylElems[i];
        // PhysX Capsule: halfHeight는 cylindrical 부분의 절반 길이
        PxCapsuleGeometry Geom(Capsule.Radius, Capsule.Length / 2.0f);
        PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Body, Geom, *Material);
        if (Shape)
        {
            // 기본 축 회전: 엔진 캡슐(Z축) → PhysX 캡슐(X축)
            // 엔진에서 Z축을 Y축으로 회전 (X축 기준 -90도)하면 PhysX 변환 후 X축이 됨
            FQuat BaseRotation = FQuat::MakeFromEulerZYX(FVector(-90.0f, 0.0f, 0.0f));
            FQuat UserRotation = FQuat::MakeFromEulerZYX(Capsule.Rotation);
            FQuat FinalRotation = UserRotation * BaseRotation;

            PxTransform LocalPose(
                PhysxConverter::ToPxVec3(Capsule.Center),
                PhysxConverter::ToPxQuat(FinalRotation)
            );
            Shape->setLocalPose(LocalPose);
        }
    }

    // Material release (Shape들이 참조하고 있으므로 refcount만 감소)
    Material->release();
}

void FRagdollSystem::CreateJoint(FRagdollBone& Bone, FRagdollBone& ParentBone)
{
    if (!Bone.Body || !ParentBone.Body) return;

    FPhysicsSystem& PhysicsSystem = FPhysicsSystem::GetInstance();
    if (!PhysicsSystem.GetPhysics()) return;

    // 자식 본의 위치를 조인트 위치로 사용
    PxVec3 JointPos = Bone.Body->getGlobalPose().p;

    // 부모 기준 로컬 프레임
    PxTransform ParentGlobalPose = ParentBone.Body->getGlobalPose();
    PxTransform LocalFrameParent = ParentGlobalPose.getInverse() * PxTransform(JointPos);

    // 자식 기준 로컬 프레임 (원점)
    PxTransform LocalFrameChild = PxTransform(PxVec3(0));

    // D6 Joint 생성
    PxD6Joint* Joint = PxD6JointCreate(
        *PhysicsSystem.GetPhysics(),
        ParentBone.Body, LocalFrameParent,
        Bone.Body, LocalFrameChild
    );

    if (!Joint) return;

    // Linear DOF: 모두 잠금 (본이 분리되지 않게)
    Joint->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED);
    Joint->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED);
    Joint->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);

    // Angular DOF: FREE로 설정 (일단 크래시 방지용, 나중에 LIMIT으로 변경)
    Joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eFREE);
    Joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eFREE);
    Joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eFREE);

    // TODO: 각도 제한 설정 (현재 FREE로 비활성화)
    // const FRagdollJointLimit& Limit = Bone.JointLimit;
    // PxJointAngularLimitPair TwistLimit(Limit.TwistLimitLow, Limit.TwistLimitHigh, 0.05f);
    // Joint->setTwistLimit(TwistLimit);
    // PxJointLimitCone SwingLimit(Limit.Swing1Limit, Limit.Swing2Limit, 0.05f);
    // Joint->setSwingLimit(SwingLimit);

    Bone.Joint = Joint;
}

void FRagdollSystem::SetupBoneHierarchy(FRagdollInstance* Instance)
{
    if (!Instance) return;

    // UserData에서 SkeletalMeshComponent 가져오기
    USkeletalMeshComponent* SkelMeshComp = static_cast<USkeletalMeshComponent*>(Instance->UserData);
    if (!SkelMeshComp)
    {
        // UserData가 없으면 선형 계층으로 폴백
        for (int32 i = 1; i < Instance->Bones.Num(); ++i)
        {
            Instance->Bones[i].ParentIndex = i - 1;
        }
        return;
    }

    USkeletalMesh* SkelMesh = SkelMeshComp->GetSkeletalMesh();
    if (!SkelMesh)
    {
        for (int32 i = 1; i < Instance->Bones.Num(); ++i)
        {
            Instance->Bones[i].ParentIndex = i - 1;
        }
        return;
    }

    const FSkeleton* Skeleton = SkelMesh->GetSkeleton();
    if (!Skeleton)
    {
        for (int32 i = 1; i < Instance->Bones.Num(); ++i)
        {
            Instance->Bones[i].ParentIndex = i - 1;
        }
        return;
    }

    // BodySetup의 BoneName을 기반으로 Ragdoll 본 인덱스 매핑 생성
    TMap<FName, int32> BoneNameToRagdollIndex;
    for (int32 i = 0; i < Instance->Bones.Num(); ++i)
    {
        FName BoneName = Instance->Bones[i].GetBoneName();
        BoneNameToRagdollIndex.Add(BoneName, i);
    }

    // 각 Ragdoll 본에 대해 부모 설정
    for (int32 i = 0; i < Instance->Bones.Num(); ++i)
    {
        FRagdollBone& Bone = Instance->Bones[i];
        FName BoneName = Bone.GetBoneName();

        // 스켈레톤에서 이 본의 인덱스 찾기
        const auto* SkeletonBoneIndexPtr = Skeleton->BoneNameToIndex.Find(BoneName.ToString());
        if (!SkeletonBoneIndexPtr)
        {
            Bone.ParentIndex = -1;  // 찾지 못하면 루트로 설정
            continue;
        }

        int32 SkeletonBoneIndex = *SkeletonBoneIndexPtr;
        int32 SkeletonParentIndex = Skeleton->Bones[SkeletonBoneIndex].ParentIndex;

        if (SkeletonParentIndex < 0)
        {
            Bone.ParentIndex = -1;  // 스켈레톤에서도 루트인 경우
            continue;
        }

        // 부모 본 이름 가져오기
        const FString& ParentBoneName = Skeleton->Bones[SkeletonParentIndex].Name;

        // Ragdoll에서 부모 본 찾기 (체인을 따라 올라가며 Ragdoll에 있는 본 찾기)
        int32 CurrentParentIndex = SkeletonParentIndex;
        while (CurrentParentIndex >= 0)
        {
            const FString& CurrentParentName = Skeleton->Bones[CurrentParentIndex].Name;
            const auto* RagdollParentIndexPtr = BoneNameToRagdollIndex.Find(FName(CurrentParentName));

            if (RagdollParentIndexPtr)
            {
                Bone.ParentIndex = *RagdollParentIndexPtr;
                break;
            }

            // Ragdoll에 없으면 더 위의 부모로
            CurrentParentIndex = Skeleton->Bones[CurrentParentIndex].ParentIndex;
        }

        // 끝까지 못 찾으면 루트
        if (CurrentParentIndex < 0)
        {
            Bone.ParentIndex = -1;
        }
    }
}

void FRagdollSystem::DestroyRagdoll(FRagdollInstance* Instance)
{
    if (!Instance) return;

    // Joint 먼저 해제
    for (FRagdollBone& Bone : Instance->Bones)
    {
        if (Bone.Joint)
        {
            Bone.Joint->release();
            Bone.Joint = nullptr;
        }
    }

    // Body 해제
    for (FRagdollBone& Bone : Instance->Bones)
    {
        if (Bone.Body)
        {
            // Body가 속한 Scene에서 직접 제거 (싱글톤 Scene과 다를 수 있음)
            PxScene* BodyScene = Bone.Body->getScene();
            if (BodyScene)
            {
                BodyScene->removeActor(*Bone.Body);
            }
            Bone.Body->release();
            Bone.Body = nullptr;
        }
    }

    // 목록에서 제거
    ActiveRagdolls.Remove(Instance);

    delete Instance;
}

void FRagdollSystem::Update(float DeltaTime)
{
    // 각 Ragdoll의 Transform을 메시에 동기화
    for (FRagdollInstance* Instance : ActiveRagdolls)
    {
        if (Instance && Instance->bIsActive)
        {
            Instance->SyncToMesh();
        }
    }

    // TODO: DeltaTime 활용 가능한 기능들
    // - Transform 보간 (부드러운 물리 적용)
    // - 애니메이션 ↔ 물리 블렌딩
    // - 타임아웃 기반 Ragdoll 비활성화
}

void FRagdollSystem::RenderDebugAll(URenderer* Renderer) const
{
    if (!Renderer) return;

    UE_LOG("[Ragdoll Debug] RenderDebugAll called, ActiveRagdolls count: %d", ActiveRagdolls.Num());

    for (const FRagdollInstance* Instance : ActiveRagdolls)
    {
        if (Instance && Instance->bIsActive)
        {
            UE_LOG("[Ragdoll Debug] Rendering instance with %d bones", Instance->Bones.Num());
            FRagdollDebugRenderer::RenderRagdoll(Renderer, Instance);
        }
    }
}
