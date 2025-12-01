#pragma once

class UPrimitiveComponent;
class UBodySetup;

using namespace physx;

// 컴포넌트와 시뮬레이션 씬의 엑터를 이어주는 중간다리 역할
// 래그돌에서도 각 본마다 하나의 FBodyInstance를 사용 (언리얼 방식)
struct FBodyInstance
{
	// 오너 컴포넌트 참조 이유: 이벤트 처리
	UPrimitiveComponent* OwnerComponent = nullptr;

	// RigidActor 참조 이유: 업데이트 or 물리 명령 시뮬레이션 액터에 보내야 함.
	PxRigidActor* RigidActor = nullptr;

	// === 래그돌 지원 (언리얼 방식) ===
	UBodySetup* BodySetup = nullptr;	// 래그돌 본의 물리 설정
	int32 BoneIndex = -1;				// 스켈레톤에서 이 바디에 대응하는 본 인덱스

	FVector PendingForce = FVector::Zero();
	FVector PendingTorque = FVector::Zero();

	bool bHasPendingForce = false;

	FBodyInstance() = default;

	~FBodyInstance();

	void AddForce(const FVector& InForce);

	void AddTorque(const FVector& InTorque);

	void FlushPendingForce();

	void UpdateMassProperty();

	// 기존 PrimitiveComponent용 초기화
	void InitPhysics(UPrimitiveComponent* Component);

	// 래그돌용 초기화 (UBodySetup 기반)
	void InitBody(UBodySetup* Setup, const FTransform& WorldTransform, int32 InBoneIndex = -1);

	// 물리 바디 정리
	void TermBody();

	// Transform 접근자
	FTransform GetWorldTransform() const;
	void SetWorldTransform(const FTransform& NewTransform);

	// 유틸리티
	PxRigidDynamic* GetPxRigidDynamic() const;
	bool IsValidBodyInstance() const { return RigidActor != nullptr; }

	static void AddShapesRecursively(USceneComponent* CurrentComponent, UPrimitiveComponent* RootComponent, PxRigidActor* PhysicsActor);

	static void SetCollisionType(PxShape* Shape, UPrimitiveComponent* Component);

private:
	// UBodySetup의 AggGeom에서 Shape들 생성 (래그돌용)
	void CreateShapesFromBodySetup(UBodySetup* Setup, PxRigidDynamic* Body);
};