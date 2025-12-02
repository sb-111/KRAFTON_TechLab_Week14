#pragma once

#include "Vector.h"
#include "Name.h"
#include "FConstraintInstance.generated.h"

// Forward declarations
namespace physx { class PxD6Joint; }
class FBodyInstance;
class UPrimitiveComponent;

// ===== Angular Constraint Motion =====
// 각도 제한 모션 타입 (회전)
enum class EAngularConstraintMotion : uint8
{
    Free,       // 자유 회전
    Limited,    // 제한된 회전
    Locked      // 회전 잠금
};

// ===== Linear Constraint Motion =====
// 선형 제한 모션 타입 (이동)
enum class ELinearConstraintMotion : uint8
{
    Free,       // 자유 이동
    Limited,    // 제한된 이동
    Locked      // 이동 잠금
};

// ===== Constraint Instance =====
// 두 본(Body) 사이의 물리 제약 조건
// Ragdoll 관절 설정에 사용
USTRUCT(DisplayName = "Constraint Instance", Description = "두 본 사이의 관절 제약 조건")
struct FConstraintInstance
{
    GENERATED_REFLECTION_BODY()
public:
    // ===== Constraint Bodies =====
    // 어떤 뼈와 어떤 뼈를 연결할지 (언리얼 규칙: 1=Child, 2=Parent)
    UPROPERTY(EditAnywhere, Category = "Constraint")
    FName ConstraintBone1;  // Child Bone (자손 본, 현재 본)

    UPROPERTY(EditAnywhere, Category = "Constraint")
    FName ConstraintBone2;  // Parent Bone (부모 본, 이전 본)

    // ===== Linear Limits (이동 제한) =====
    // Ragdoll은 뼈가 빠지면 안 되므로 보통 다 Locked
    UPROPERTY(EditAnywhere, Category = "Linear Limits")
    ELinearConstraintMotion LinearXMotion = ELinearConstraintMotion::Locked;

    UPROPERTY(EditAnywhere, Category = "Linear Limits")
    ELinearConstraintMotion LinearYMotion = ELinearConstraintMotion::Locked;

    UPROPERTY(EditAnywhere, Category = "Linear Limits")
    ELinearConstraintMotion LinearZMotion = ELinearConstraintMotion::Locked;

    // 선형 제한 거리 (Limited일 때 사용)
    UPROPERTY(EditAnywhere, Category = "Linear Limits")
    float LinearLimit = 0.0f;

    // ===== Angular Limits (회전 제한) =====
    // PhysX D6 Joint 기준: Twist=X축, Swing1=Y축, Swing2=Z축
    // PhysX 내부적으로 Local X: Twist(비틀기), Local Y: Swing1(위아래 꺾기), Local Z: Swing2(좌우 꺾기)
    // Joint가 생성된 로컬 공간 얘기하는 것

    UPROPERTY(EditAnywhere, Category = "Angular Limits")
    EAngularConstraintMotion TwistMotion = EAngularConstraintMotion::Limited;

    UPROPERTY(EditAnywhere, Category = "Angular Limits")
    EAngularConstraintMotion Swing1Motion = EAngularConstraintMotion::Limited;

    UPROPERTY(EditAnywhere, Category = "Angular Limits")
    EAngularConstraintMotion Swing2Motion = EAngularConstraintMotion::Limited;

    // 각도 제한값 (Degrees)
    UPROPERTY(EditAnywhere, Category = "Angular Limits")
    float TwistLimitAngle = 45.0f;      // X축 회전 제한

    UPROPERTY(EditAnywhere, Category = "Angular Limits")
    float Swing1LimitAngle = 45.0f;     // Y축 회전 제한

    UPROPERTY(EditAnywhere, Category = "Angular Limits")
    float Swing2LimitAngle = 45.0f;     // Z축 회전 제한

    // ===== Constraint Transform (Frame) =====
    // ★★★ [중요] 회전(Rotation) 포함 ★★★
    // 이 값이 없으면 관절의 꺾이는 방향(축)을 설정할 수 없음
    // PhysX D6 Joint에서 X축은 Twist(비틀기) 축
    // 
    // 보통 3D툴에서 뼈를 심을 때, 뼈의 길이 방향이 중요함.
    // 뼈의 긴축이 Y축이어도 Joint 설정을 위해 X축으로 정렬해야 Twist가 성립함
    // 어떤 모델러는 Y축을 뼈의 길이 방향으로 잡기도 함.
    // 그냥 Joint를 생성하면, 뼈의 길이(Y)가 아니라 뼈의 옆구리(X)를 기준으로 비틀려고함.
    // 그러면 팔이 부러지는 방향으로 회전하려고 함.
    // Rotation 변수 존재 이유: 뼈의 좌표계가 어떻게 생겨 먹었든, PhysX가 좋아하는 X축(Twist) 방향으로 뼈의 길이 방향을 맞춰주기 위해서임
    // 예를들어 팔뚝뼈가 Y축 길이 방향이라고 하면, PhysX는 X축으로 비틀면 대참사가 일어남. Rotation1에 Z축으로 -90도 회전값을 넣음. 그러면 Joint .X축이 회전해서 뼈의 Y축(길이 방향)과 일치하게 됨

    // Child Bone (Bone1) 공간에서의 조인트 위치 및 회전
    UPROPERTY(EditAnywhere, Category = "Transform")
    FVector Position1 = FVector::Zero();

    UPROPERTY(EditAnywhere, Category = "Transform")
    FVector Rotation1 = FVector::Zero();    // Euler Angles (Degrees)

    // Parent Bone (Bone2) 공간에서의 조인트 위치 및 회전
    UPROPERTY(EditAnywhere, Category = "Transform")
    FVector Position2 = FVector::Zero();

    UPROPERTY(EditAnywhere, Category = "Transform")
    FVector Rotation2 = FVector::Zero();    // Euler Angles (Degrees)

    // ===== Collision Settings (충돌 설정) =====
    // 언리얼 방식: Joint로 연결된 두 Body 간의 충돌 비활성화 여부
    // true = 인접 본 간 충돌 무시 (래그돌 기본값)
    // false = 인접 본 간 충돌 활성화
    UPROPERTY(EditAnywhere, Category = "Collision")
    bool bDisableCollision = true;

    // ===== Motor Settings (Drive) =====
    // 시체가 된 후에도 특정 자세를 유지하려 하거나 힘을 줄 때 사용
    UPROPERTY(EditAnywhere, Category = "Motor")
    bool bAngularMotorEnabled = false;

    UPROPERTY(EditAnywhere, Category = "Motor")
    float AngularMotorStrength = 0.0f;  // Stiffness (스프링 강도)

    UPROPERTY(EditAnywhere, Category = "Motor")
    float AngularMotorDamping = 0.0f;   // Damping (감쇠) - 떨림 방지

    // ===== Runtime Members (런타임 전용, 직렬화 안 됨) =====
    // 과제 요구사항: PxJoint = FConstraintInstance
    UPrimitiveComponent* OwnerComponent = nullptr;  // 소유자 컴포넌트
    physx::PxD6Joint* ConstraintData = nullptr;     // PhysX Joint (런타임에 생성)

    // ===== Runtime Methods =====
    FConstraintInstance() = default;
    ~FConstraintInstance();

    // Joint 초기화 (두 FBodyInstance 사이에 Joint 생성)
    // Body1 = Child Body, Body2 = Parent Body (언리얼 규칙)
    void InitConstraint(FBodyInstance* Body1, FBodyInstance* Body2, UPrimitiveComponent* InOwnerComponent);

    // Joint 해제
    void TermConstraint();

    // Joint가 유효한지 확인
    bool IsValidConstraintInstance() const { return ConstraintData != nullptr; }

    // PhysX Joint 접근
    physx::PxD6Joint* GetPxD6Joint() const { return ConstraintData; }

};
