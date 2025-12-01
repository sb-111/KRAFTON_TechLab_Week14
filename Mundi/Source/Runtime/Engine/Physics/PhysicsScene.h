#pragma once
#include "PrePhysics.h"

using namespace physx;

class FPhysicsScene
{
public:
	
	FPhysicsScene(PxScene* InScene);

	~FPhysicsScene();

	FPhysicsScene(const FPhysicsScene&) = delete;
	FPhysicsScene& operator=(const FPhysicsScene&) = delete;

	void RegisterPrePhysics(IPrePhysics* Object);

	void UnRegisterPrePhysics(IPrePhysics* Object);

	void RegisterTemporal(IPrePhysics* Object);
	
	void UnRegisterTemporal(IPrePhysics* Object);

	void AddActor(PxActor& NewActor);

	void Simulate(float DeltaTime);

	void FetchAndUpdate();

	void WriteInTheDeathNote(physx::PxActor* ActorToDie);

	void PendingDestroyInDeathNote();
private:

	// Un/Register 쉽게하려고 Set 사용
	TSet<IPrePhysics*> PreUpdateList;      

	// 매 프레임 업데이트되는 리스트라서 Array사용(외력)
	TArray<IPrePhysics*> PreUpdateListTemporal;

	// 시뮬레이션 도중 엑터 삭제하면 안되서 PendingDestroy
	TArray<PxActor*> ActorDeathNote;

	PxScene* Scene = nullptr;

};