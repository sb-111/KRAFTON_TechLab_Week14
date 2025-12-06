#pragma once
#include "PrePhysics.h"

using namespace physx;
class FBodyInstance;

class FPhysicsSimulationEventCallback : public PxSimulationEventCallback
{
public:
	void onContact(const PxContactPairHeader& PairHeader, const PxContactPair* Pairs, PxU32 NumPairs) override;
	void onTrigger(PxTriggerPair* Pairs, PxU32 Count) override;
	void onConstraintBreak(PxConstraintInfo* Constraints, PxU32 Count) override {}
	void onWake(PxActor** Actors, PxU32 Count) override {}
	void onSleep(PxActor** Actors, PxU32 Count) override {}
	void onAdvance(const PxRigidBody* const*, const PxTransform*, const PxU32) override {}

private:
	UPrimitiveComponent* GetCompFromPxActor(physx::PxRigidActor* InActor);
	FName GetBoneNameFromShape(const physx::PxShape* Shape);
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


	// Vehicle
	void InitVehicleSDK();

	void ReleaseVehicleSDK();

	PxVec3 GetGravity();

	PxBatchQuery* GetBatchQuery();

	PxRaycastQueryResult* GetRaycastQueryResult();

	uint32 GetRaycastQueryResultSize();

	PxVehicleDrivableSurfaceToTireFrictionPairs* GetFrictionPairs();

	// 시작점, 방향(정규화 안해도 내부에서 함), 거리, 결과값(Ref)
	bool Raycast(const FVector& Origin, const FVector& Direction, float MaxDistance, FHitResult& OutHit);
	
private:

	// Un/Register 쉽게하려고 Set 사용
	TSet<IPrePhysics*> PreUpdateList;      

	// 매 프레임 업데이트되는 리스트라서 Array사용(외력)
	//TArray<IPrePhysics*> PreUpdateListTemporal;

	TArray<std::function<void()>> CommandQueue;

	// 시뮬레이션 도중 엑터 삭제하면 안되서 PendingDestroy
	TArray<PxActor*> ActorDeathNote;

	PxScene* Scene = nullptr;
	FPhysicsSimulationEventCallback* EventCallback = nullptr;

	const float FixedDeltaTime = 1.0f / 60.0f;
	float LeftoverTime = 0.0f;
	bool bIsSimulated = false;  // 이번 Tick에 Simulate 을 수행했는지
	
	// 자동차 레이캐스팅 묶음 
	PxBatchQuery* BatchQuery = nullptr;

	// 충돌 정보 인덱싱용
	TArray<PxRaycastQueryResult> RaycastResult;
	// 실제 충돌 정보
	TArray<PxRaycastHit> RaycastHits;

	// 어떤 타이어가 어떤 바닥을 밟았느냐에 따른 마찰력을 저장하는 테이블
	PxVehicleDrivableSurfaceToTireFrictionPairs* FrictionPairs = nullptr;

	// 메모리 미리 잡아두려고 최댓값 정해놓음
	const uint32 MAX_VEHICLE = 4;
	const uint32 MAX_WHEELS_PER_VEHICLE = 4;

	bool VehicleSDKInitialized = false;
};