#pragma once
#include "PrePhysics.h"

using namespace physx;
class FBodyInstance;

struct FCollisionEvent
{
	FBodyInstance* BodyA = nullptr;
	FBodyInstance* BodyB = nullptr;

	// 충돌지점 위치
	FVector ContactPosition{};
	// 충돌 지점의 표면에 수직한 벡터(방향: Body B -> Body A)
	FVector ContactNormal{};

	// 충격량 크기 (스칼라값으로 충분할 때가 많음)
	float ImpulseMagnitude = 0.0f;  
	// 스치는지 정통으로 박는지 구분 가능(ContactNormal이랑 내적) -> 내적 값에 따라 끼이익.. 쾅!! 사운드 다르게 적용 가능
	FVector RelativeVelocity{};

	// 스켈레탈 메시의 경우 필요
	int32 ShapeIndexA = 0; // BodyA의 몇 번째 Shape인지 저장 (헤드샷 판별 가능)
	int32 ShapeIndexB = 0; 
};

class FPhysicsSimulationEventCallback : public PxSimulationEventCallback
{
public:

	TArray<FCollisionEvent> EventQueue;
	std::mutex QueueMutex;

	void onContact(const PxContactPairHeader& PairHeader, const PxContactPair* Pairs, PxU32 NumPairs) override;


	virtual void onTrigger(PxTriggerPair* Pairs, PxU32 Count) override {}
	virtual void onConstraintBreak(PxConstraintInfo* Constraints, PxU32 Count) override {}
	virtual void onWake(PxActor** Actors, PxU32 Count) override {}
	virtual void onSleep(PxActor** Actors, PxU32 Count) override {}
	virtual void onAdvance(const PxRigidBody* const*, const PxTransform*, const PxU32) override {}
};

class FPhysicsScene
{
public:
	
	FPhysicsScene(PxScene* InScene);

	~FPhysicsScene();

	FPhysicsScene(const FPhysicsScene&) = delete;
	FPhysicsScene& operator=(const FPhysicsScene&) = delete;

	void RegisterPrePhysics(IPrePhysics* Object);

	void UnRegisterPrePhysics(IPrePhysics* Object);

	void EnqueueCommand(std::function<void()> Command);

	void ProcessCommandQueue();
	/*void RegisterTemporal(IPrePhysics* Object);
	
	void UnRegisterTemporal(IPrePhysics* Object);*/

	void AddActor(PxActor& NewActor);

	void Simulate(float DeltaTime);

	void FetchAndUpdate();

	void WriteInTheDeathNote(physx::PxActor* ActorToDie);

	void PendingDestroyInDeathNote();
private:

	// Un/Register 쉽게하려고 Set 사용
	TSet<IPrePhysics*> PreUpdateList;      

	// 매 프레임 업데이트되는 리스트라서 Array사용(외력)
	//TArray<IPrePhysics*> PreUpdateListTemporal;

	TArray<std::function<void()>> CommandQueue;

	// 시뮬레이션 도중 엑터 삭제하면 안되서 PendingDestroy
	TArray<PxActor*> ActorDeathNote;

	PxScene* Scene = nullptr;

	std::unique_ptr<FPhysicsSimulationEventCallback> EventCallback = nullptr;

};