#include "pch.h"
#include "ConstraintInstance.h"
#include "BodyInstance.h"
#include "PhysicsSystem.h"
#include "PhysxConverter.h"
#include <cmath>

using namespace physx;

// 헬퍼: 방향 벡터에서 회전 쿼터니언 계산 (Twist 축 = 방향)
static PxQuat ComputeJointFrameRotation(const PxVec3& Direction)
{
    PxVec3 DefaultAxis(1, 0, 0);
    PxVec3 Dir = Direction.getNormalized();

    float Dot = DefaultAxis.dot(Dir);
    if (Dot > 0.9999f)
    {
        return PxQuat(PxIdentity);
    }
    if (Dot < -0.9999f)
    {
        return PxQuat(PxPi, PxVec3(0, 1, 0));
    }

    PxVec3 Axis = DefaultAxis.cross(Dir).getNormalized();
    float Angle = std::acos(Dot);
    return PxQuat(Angle, Axis);
}

// DegreesToRadians는 Vector.h에 이미 정의되어 있으므로 사용

FConstraintInstance::~FConstraintInstance()
{
    TermConstraint();
}

void FConstraintInstance::InitConstraint(FBodyInstance* Body1, FBodyInstance* Body2, UPrimitiveComponent* InOwnerComponent)
{
    // 기존 Joint가 있으면 해제
    TermConstraint();

    if (!Body1 || !Body2) return;

    PxRigidDynamic* Parent = Body1->GetPxRigidDynamic();
    PxRigidDynamic* Child = Body2->GetPxRigidDynamic();
    if (!Parent || !Child) return;

    FPhysicsSystem& PhysicsSystem = FPhysicsSystem::GetInstance();
    if (!PhysicsSystem.GetPhysics()) return;

    OwnerComponent = InOwnerComponent;

    // ===== Joint Frame 계산 (언리얼 방식) =====
    PxTransform ParentGlobalPose = Parent->getGlobalPose();
    PxTransform ChildGlobalPose = Child->getGlobalPose();

    // Joint 위치: 자식 Body의 위치
    PxVec3 JointWorldPos = ChildGlobalPose.p;

    // 본 방향: 부모→자식 (Twist 축으로 사용)
    PxVec3 BoneDirection = (ChildGlobalPose.p - ParentGlobalPose.p);
    if (BoneDirection.magnitude() < 0.001f)
    {
        BoneDirection = PxVec3(1, 0, 0);
    }

    // Joint Frame 회전: 본 방향을 Twist 축(X)으로
    PxQuat JointRotation = ComputeJointFrameRotation(BoneDirection);

    // 부모/자식 로컬 좌표계에서의 Joint Frame
    PxTransform LocalFrame1 = ParentGlobalPose.getInverse() * PxTransform(JointWorldPos, JointRotation);
    PxTransform LocalFrame2 = ChildGlobalPose.getInverse() * PxTransform(JointWorldPos, JointRotation);

    // Joint 생성
    PxD6Joint* Joint = PxD6JointCreate(
        *PhysicsSystem.GetPhysics(),
        Parent, LocalFrame1,
        Child, LocalFrame2
    );
    if (!Joint) return;

    // Linear DOF: 잠금
    Joint->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED);
    Joint->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED);
    Joint->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);

    // Angular DOF: LIMITED
    Joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
    Joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
    Joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);

    // 각도 제한 적용
    float TwistRad = DegreesToRadians(TwistLimitAngle);
    float Swing1Rad = DegreesToRadians(Swing1LimitAngle);
    float Swing2Rad = DegreesToRadians(Swing2LimitAngle);
    float ContactDist = 0.01f;

    Joint->setTwistLimit(PxJointAngularLimitPair(-TwistRad, TwistRad, ContactDist));
    Joint->setSwingLimit(PxJointLimitCone(Swing1Rad, Swing2Rad, ContactDist));

    // userData에 이 인스턴스 포인터 저장 (과제 요구사항)
    Joint->userData = this;

    ConstraintData = Joint;
}

void FConstraintInstance::TermConstraint()
{
    if (ConstraintData)
    {
        ConstraintData->release();
        ConstraintData = nullptr;
    }
    OwnerComponent = nullptr;
}
