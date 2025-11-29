#pragma once

#include "pch.h"
#include <PxPhysicsAPI.h>

using namespace physx;

class UBodySetup;

// ===== Ragdoll Joint Limit =====
// Joint 각도 제한 설정
struct FRagdollJointLimit
{
    float TwistLimitLow = -PxPi / 4;   // -45도
    float TwistLimitHigh = PxPi / 4;   // +45도
    float Swing1Limit = PxPi / 6;      // 30도
    float Swing2Limit = PxPi / 6;      // 30도
};

// ===== Ragdoll Bone =====
// Ragdoll 본 정의 - UBodySetup 활용
struct FRagdollBone
{
    // === UBodySetup에서 가져오는 정보 ===
    // BodySetup->BoneName             : 본 이름
    // BodySetup->AggGeom              : Shape 정보 (FKSphylElem 등)
    // BodySetup->MassInKg             : 질량
    // BodySetup->Friction/Restitution : 물리 재질
    UBodySetup* BodySetup = nullptr;

    // === Ragdoll 전용 정보 ===
    int32 ParentIndex = -1;             // 부모 본 인덱스 (-1이면 루트)
    FRagdollJointLimit JointLimit;      // Joint 각도 제한

    // === PhysX 런타임 데이터 ===
    PxRigidDynamic* Body = nullptr;
    PxD6Joint* Joint = nullptr;

    // 헬퍼 함수
    FName GetBoneName() const;
    float GetMass() const;
};
