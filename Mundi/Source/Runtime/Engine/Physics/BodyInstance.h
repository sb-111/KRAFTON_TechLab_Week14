#pragma once

class UPrimitiveComponent;

using namespace physx;

// 컴포넌트와 시뮬레이션 씬의 엑터를 이어주는 중간다리 역할
struct FBodyInstance
{
	// 오너 컴포넌트 참조 이유: 이벤트 처리
	UPrimitiveComponent* OwnerComponent = nullptr;

	// RigidActor 참조 이유: 업데이트 or 물리 명령 시뮬레이션 액터에 보내야 함.
	PxRigidActor* RigidActor = nullptr;

	FBodyInstance(UPrimitiveComponent* InOwnerComponent);

	~FBodyInstance();

	static void InitPhysics(UPrimitiveComponent* Component);

	static void AddShapesRecursively(USceneComponent* CurrentComponent, UPrimitiveComponent* RootComponent, PxRigidActor* PhysicsActor);

	static void SetCollisionType(PxShape* Shape, UPrimitiveComponent* Component);
};