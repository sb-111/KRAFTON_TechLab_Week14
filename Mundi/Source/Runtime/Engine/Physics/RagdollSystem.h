#pragma once

#include "pch.h"
#include "RagdollBone.h"
#include <PxPhysicsAPI.h>

using namespace physx;

class UPhysicsAsset;
class UBodySetup;
class URenderer;

// ===== Ragdoll Instance =====
// 하나의 Ragdoll 인스턴스 (본들과 조인트들의 집합)
class FRagdollInstance
{
public:
    TArray<FRagdollBone> Bones;
    bool bIsActive = false;
    UPhysicsAsset* PhysicsAsset = nullptr;  // 원본 에셋 참조
    void* UserData = nullptr;                // SkeletalMeshComponent 참조용

    void Activate();
    void Deactivate();
    void SyncToMesh();  // 물리 -> 렌더링 Transform 동기화
};

// ===== Ragdoll System =====
// Ragdoll 생성, 시뮬레이션, 관리를 담당하는 싱글톤
class FRagdollSystem
{
public:
    static FRagdollSystem& GetInstance();

    void Initialize();
    void Shutdown();

    // === UPhysicsAsset에서 Ragdoll 생성 (핵심!) ===
    FRagdollInstance* CreateRagdollFromPhysicsAsset(
        UPhysicsAsset* PhysicsAsset,
        const TArray<FTransform>& BoneWorldTransforms,  // 현재 본 위치들
        void* InUserData = nullptr
    );

    void DestroyRagdoll(FRagdollInstance* Instance);
    void Update(float DeltaTime);
    void RenderDebugAll(URenderer* Renderer) const;

    // Ragdoll 개수 조회
    int32 GetActiveRagdollCount() const { return ActiveRagdolls.Num(); }

private:
    FRagdollSystem() = default;
    ~FRagdollSystem() = default;
    FRagdollSystem(const FRagdollSystem&) = delete;
    FRagdollSystem& operator=(const FRagdollSystem&) = delete;

    // UBodySetup에서 Shape 생성
    void CreateShapesFromBodySetup(UBodySetup* BodySetup, PxRigidDynamic* Body);

    // PxD6Joint 생성
    void CreateJoint(FRagdollBone& Bone, FRagdollBone& ParentBone);

    // 부모-자식 관계 설정 (TODO: SkeletalMesh 본 계층에서 가져와야 함)
    void SetupBoneHierarchy(FRagdollInstance* Instance);

    TArray<FRagdollInstance*> ActiveRagdolls;
    static FRagdollSystem* Instance;
};
