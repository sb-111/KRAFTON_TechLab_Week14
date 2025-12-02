#include "pch.h"
#include "ConstraintInstance.h"
#include "BodyInstance.h"
#include "PhysicsSystem.h"
#include "PhysxConverter.h"
#include <cmath>

using namespace physx;

// ===== 헬퍼: Motion 타입 변환 (언리얼 방식) =====
// EAngularConstraintMotion → PxD6Motion
static PxD6Motion::Enum ConvertAngularMotion(EAngularConstraintMotion Motion)
{
    switch (Motion)
    {
    case EAngularConstraintMotion::Free:    return PxD6Motion::eFREE;
    case EAngularConstraintMotion::Limited: return PxD6Motion::eLIMITED;
    case EAngularConstraintMotion::Locked:  return PxD6Motion::eLOCKED;
    default:                                return PxD6Motion::eLIMITED;
    }
}

// ELinearConstraintMotion → PxD6Motion
static PxD6Motion::Enum ConvertLinearMotion(ELinearConstraintMotion Motion)
{
    switch (Motion)
    {
    case ELinearConstraintMotion::Free:    return PxD6Motion::eFREE;
    case ELinearConstraintMotion::Limited: return PxD6Motion::eLIMITED;
    case ELinearConstraintMotion::Locked:  return PxD6Motion::eLOCKED;
    default:                               return PxD6Motion::eLOCKED;
    }
}

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

    // 언리얼 규칙: Body1 = Child, Body2 = Parent
    PxRigidDynamic* Child = Body1->GetPxRigidDynamic();
    PxRigidDynamic* Parent = Body2->GetPxRigidDynamic();
    if (!Child || !Parent) return;

    FPhysicsSystem& PhysicsSystem = FPhysicsSystem::GetInstance();
    if (!PhysicsSystem.GetPhysics()) return;

    OwnerComponent = InOwnerComponent;

    // ===== Joint Frame 계산 =====
    // LocalFrame1 = Child(Body1) 로컬, LocalFrame2 = Parent(Body2) 로컬
    PxTransform LocalFrame1, LocalFrame2;

    // Position/Rotation 상태 확인
    bool bHasPosition1 = !Position1.IsZero();
    bool bHasPosition2 = !Position2.IsZero();
    bool bHasRotation1 = !Rotation1.IsZero();
    bool bHasRotation2 = !Rotation2.IsZero();

    PxTransform ChildGlobalPose = Child->getGlobalPose();
    PxTransform ParentGlobalPose = Parent->getGlobalPose();

    if (bHasPosition2 && bHasRotation2)
    {
        // Position2/Rotation2가 설정됨 (언리얼 방식: Parent 기준으로 Joint Frame 설정)
        // Joint 위치 = Child 본 원점 (Position1 = 0,0,0)
        // Rotation2 = Parent에서 봤을 때 Joint가 향하는 방향

        // Parent 로컬에서의 Joint 트랜스폼
        FQuat Rot2 = FQuat::MakeFromEulerZYX(Rotation2);
        PxVec3 LocalPos2 = PhysxConverter::ToPxVec3(Position2);
        PxQuat LocalRot2 = PhysxConverter::ToPxQuat(Rot2);

        // Parent 로컬 → 월드 변환
        PxTransform JointLocalInParent(LocalPos2, LocalRot2);
        PxTransform JointWorld = ParentGlobalPose * JointLocalInParent;

        // 각 Body 로컬로 변환
        LocalFrame1 = ChildGlobalPose.getInverse() * JointWorld;
        LocalFrame2 = JointLocalInParent;  // 이미 Parent 로컬
    }
    else if (bHasPosition1 && bHasRotation1)
    {
        // Position1/Rotation1 기준으로 Joint 월드 좌표 계산 후 각 Body 로컬로 변환
        // (언리얼 규칙: Position1/Rotation1 = Child 로컬)
        FQuat Rot1 = FQuat::MakeFromEulerZYX(Rotation1);
        PxVec3 LocalPos1 = PhysxConverter::ToPxVec3(Position1);
        PxQuat LocalRot1 = PhysxConverter::ToPxQuat(Rot1);

        // Child Body 로컬 → 월드 변환
        PxTransform JointLocalInChild(LocalPos1, LocalRot1);
        PxTransform JointWorld = ChildGlobalPose * JointLocalInChild;

        // 각 Body 로컬로 변환
        LocalFrame1 = JointLocalInChild;  // 이미 Child 로컬
        LocalFrame2 = ParentGlobalPose.getInverse() * JointWorld;
    }
    else if (bHasPosition1 || bHasPosition2)
    {
        // Position만 설정됨 → Position 사용, Rotation은 본 방향 기반 자동 계산
        // 본 방향 = Parent → Child (자식 본 방향)
        PxVec3 BoneDirection = (ChildGlobalPose.p - ParentGlobalPose.p);
        if (BoneDirection.magnitude() < 0.001f)
        {
            BoneDirection = PxVec3(1, 0, 0);
        }
        PxQuat JointRotation = ComputeJointFrameRotation(BoneDirection);

        // Joint 월드 위치 계산
        PxVec3 JointWorldPos = bHasPosition1
            ? ChildGlobalPose.p + ChildGlobalPose.q.rotate(PhysxConverter::ToPxVec3(Position1))
            : ParentGlobalPose.p + ParentGlobalPose.q.rotate(PhysxConverter::ToPxVec3(Position2));

        // 각 Body 로컬로 변환
        LocalFrame1 = ChildGlobalPose.getInverse() * PxTransform(JointWorldPos, JointRotation);
        LocalFrame2 = ParentGlobalPose.getInverse() * PxTransform(JointWorldPos, JointRotation);
    }
    else
    {
        // 둘 다 없음 → 완전 자동 계산 (Child 본 위치에 Joint 생성)
        PxVec3 JointWorldPos = ChildGlobalPose.p;
        // 본 방향 = Parent → Child (자식 본 방향)
        PxVec3 BoneDirection = (ChildGlobalPose.p - ParentGlobalPose.p);
        if (BoneDirection.magnitude() < 0.001f)
        {
            BoneDirection = PxVec3(1, 0, 0);
        }

        PxQuat JointRotation = ComputeJointFrameRotation(BoneDirection);

        LocalFrame1 = ChildGlobalPose.getInverse() * PxTransform(JointWorldPos, JointRotation);
        LocalFrame2 = ParentGlobalPose.getInverse() * PxTransform(JointWorldPos, JointRotation);
    }

    // Joint 생성 (actor0=Child, actor1=Parent)
    PxD6Joint* Joint = PxD6JointCreate(
        *PhysicsSystem.GetPhysics(),
        Child, LocalFrame1,
        Parent, LocalFrame2
    );
    if (!Joint) return;

    // ===== Linear DOF 설정 (언리얼 방식: 멤버 변수 사용) =====
    Joint->setMotion(PxD6Axis::eX, ConvertLinearMotion(LinearXMotion));
    Joint->setMotion(PxD6Axis::eY, ConvertLinearMotion(LinearYMotion));
    Joint->setMotion(PxD6Axis::eZ, ConvertLinearMotion(LinearZMotion));

    // Linear Limit 적용 (Limited인 경우에만)
    if (LinearXMotion == ELinearConstraintMotion::Limited ||
        LinearYMotion == ELinearConstraintMotion::Limited ||
        LinearZMotion == ELinearConstraintMotion::Limited)
    {
        Joint->setLinearLimit(PxJointLinearLimit(LinearLimit, PxSpring(0, 0)));
    }

    // ===== Angular DOF 설정 (언리얼 방식: 멤버 변수 사용) =====
    Joint->setMotion(PxD6Axis::eTWIST, ConvertAngularMotion(TwistMotion));
    Joint->setMotion(PxD6Axis::eSWING1, ConvertAngularMotion(Swing1Motion));
    Joint->setMotion(PxD6Axis::eSWING2, ConvertAngularMotion(Swing2Motion));

    // 각도 제한 적용 (Limited인 경우에만)
    float ContactDist = 0.01f;

    if (TwistMotion == EAngularConstraintMotion::Limited)
    {
        float TwistRad = DegreesToRadians(TwistLimitAngle);
        Joint->setTwistLimit(PxJointAngularLimitPair(-TwistRad, TwistRad, ContactDist));
    }

    if (Swing1Motion == EAngularConstraintMotion::Limited ||
        Swing2Motion == EAngularConstraintMotion::Limited)
    {
        float Swing1Rad = DegreesToRadians(Swing1LimitAngle);
        float Swing2Rad = DegreesToRadians(Swing2LimitAngle);
        Joint->setSwingLimit(PxJointLimitCone(Swing1Rad, Swing2Rad, ContactDist));
    }

    // ===== 충돌 설정 (언리얼 방식) =====
    // bDisableCollision = true면 Joint로 연결된 두 Body 간 충돌 비활성화
    Joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, !bDisableCollision);

    // ===== Angular Motor (Drive) 설정 =====
    // 래그돌이 특정 자세를 유지하려 하거나 힘을 줄 때 사용
    if (bAngularMotorEnabled && (AngularMotorStrength > 0.0f || AngularMotorDamping > 0.0f))
    {
        // SLERP 드라이브: 모든 회전 축에 대해 부드럽게 목표로 끌어당김
        PxD6JointDrive angularDrive(AngularMotorStrength, AngularMotorDamping, PX_MAX_F32, true);
        Joint->setDrive(PxD6Drive::eSLERP, angularDrive);

        // 드라이브 목표: 초기 상대 위치로 설정 (기본 자세 유지)
        Joint->setDrivePosition(PxTransform(PxIdentity));
    }

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
