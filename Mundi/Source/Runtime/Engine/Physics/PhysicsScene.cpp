#include "pch.h"
#include "PhysicsScene.h"

#include <algorithm>
#include "BodyInstance.h"
#include "ConstraintInstance.h"
#include "HitResult.h"
#include "LuaCoroutineScheduler.h"
#include "PrimitiveComponent.h"
#include "PhysicsSystem.h"
#include "SkeletalMeshComponent.h"
#include "RagdollStats.h"


#define SCOPED_READ_LOCK(Scene) PxSceneReadLock ScopedReadLock(Scene);
#define SCOPED_WRITE_LOCK(Scene) PxSceneWriteLock ScopedWriteLock(Scene);

FPhysicsScene::FPhysicsScene(PxScene* InScene) : Scene(InScene)
{
	EventCallback = new FPhysicsSimulationEventCallback();
	Scene->setSimulationEventCallback(EventCallback);
}

FPhysicsScene::~FPhysicsScene()
{
	if (VehicleSDKInitialized)
	{
		ReleaseVehicleSDK();
	}
	if (Scene)
	{
		// Scene 해제 전에 데스노트에 남아있는 모든 액터 정리
		// (~UWorld에서 액터 삭제 시 TermBody() → WriteInTheDeathNote()가 호출되어
		// 데스노트에 추가된 액터들이 있을 수 있음)
		PendingDestroyInDeathNote();

		Scene->release();
		Scene = nullptr;
	}
	if (EventCallback)
	{
		delete EventCallback;
	}
}

void FPhysicsScene::RegisterPrePhysics(IPrePhysics* Object)
{
	PreUpdateList.Add(Object);
}

void FPhysicsScene::UnRegisterPrePhysics(IPrePhysics* Object)
{
	PreUpdateList.erase(Object);
}

void FPhysicsScene::EnqueueCommand(std::function<void()> Command)
{
	CommandQueue.Add(Command);
}

void FPhysicsScene::ProcessCommandQueue()
{
	for (const auto& Command : CommandQueue)
	{
		if(Command) Command(); // 저장된 함수(삭제/추가) 실행
	}
	CommandQueue.clear();
}


//void FPhysicsScene::RegisterTemporal(IPrePhysics* Object)
//{
//	PreUpdateListTemporal.Add(Object);
//}
//
//void FPhysicsScene::UnRegisterTemporal(IPrePhysics* Object)
//{
//	for (int Index = 0; Index < PreUpdateListTemporal.Num(); Index++)
//	{
//		if (PreUpdateListTemporal[Index] == Object)
//		{
//			PreUpdateListTemporal.RemoveAtSwap(Index, 1);
//			return;
//		}
//	}
//}

void FPhysicsScene::AddActor(PxActor* Actor)
{
	EnqueueCommand([this, Actor]()
		{
			if (Scene && Actor)
			{
				Scene->addActor(*Actor);
			}
		});
}

void FPhysicsScene::RemoveActor(PxActor* Actor)
{
	EnqueueCommand([this, Actor]()
		{
			if (Scene && Actor)
			{
				Scene->removeActor(*Actor);
			}
		});
}

void FPhysicsScene::Simulate(float DeltaTime)
{
	// 델타 타임 캡 (MaxFrameTime)
	constexpr float MaxFrameTime = 0.1f;
	if (DeltaTime > MaxFrameTime) // std::min 대신 명시적 조건문 사용 (취향 차이)
	{
		DeltaTime = MaxFrameTime;
	}

	LeftoverTime += DeltaTime;

	constexpr int32 MaxSubSteps = 10;
	int32 CurrentStep = 0;

	// 고정 시간 시뮬레이션 (Sub-stepping)
	while (LeftoverTime >= FixedDeltaTime && CurrentStep < MaxSubSteps)
	{
		LeftoverTime -= FixedDeltaTime;
		CurrentStep++;

		// 1. 물리 업데이트 전 처리
		for (IPrePhysics* PrePhysics : PreUpdateList)
		{
			PrePhysics->PrePhysicsUpdate(FixedDeltaTime);
		}

		// 2. 시뮬레이션 시작
		Scene->simulate(FixedDeltaTime);
		bIsSimulated = true; // 시뮬레이션 시작했으므로 true

		// 3. 결과 대기 및 동기화 (Blocking)
		// 서브스테핑 중에는 반드시 이전 단계가 끝나야 다음 단계를 계산할 수 있으므로
		// 여기서 fetchResults를 통해 기다립니다.
		FetchAndUpdate(); 
       
		// [삭제됨] bIsSimulated = true; 
		// FetchAndUpdate 안에서 bIsSimulated가 false가 되었고, 
		// PhysX도 Idle 상태가 되었으므로 이 상태가 맞습니다.
	}

	// 루프 제한으로 남은 시간 버림
	if (CurrentStep >= MaxSubSteps)
	{
		LeftoverTime = 0.0f;
	}
}

void FPhysicsScene::FetchAndUpdate()
{
	if (!bIsSimulated)
	{
		return;
	}

    bIsSimulated = false;
	Scene->fetchResults(true);

	PxU32 NumActiveActors = 0;
	PxActor** ActiveActors = Scene->getActiveActors(NumActiveActors);

	// 래그돌 통계 수집을 위한 처리된 컴포넌트 추적
	TSet<USkeletalMeshComponent*> ProcessedRagdolls;

	for (PxU32 Index = 0; Index < NumActiveActors; Index++)
	{
		PxActor* Actor = ActiveActors[Index];

		if (Actor->userData)
		{
			FBodyInstance* Instance = (FBodyInstance*)Actor->userData;

			if (Instance->OwnerComponent)
			{
				Instance->OwnerComponent->ApplyPhysicsResult();
			}
		}
	}

	PendingDestroyInDeathNote();
}

void FPhysicsScene::WriteInTheDeathNote(physx::PxActor* ActorToDie)
{
	FBodyInstance* Instance = (FBodyInstance*)ActorToDie->userData;
	UnRegisterPrePhysics(Instance->OwnerComponent);
	//UnRegisterTemporal(Instance->OwnerComponent);
	ActorToDie->userData = nullptr;
	ActorToDie->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, true);
	ActorDeathNote.Add(ActorToDie);
	
}

void FPhysicsScene::PendingDestroyInDeathNote()
{
	for (PxActor* ActorToDie : ActorDeathNote)
	{
		// 현재 씬에 속한 액터지만 확실히 처리
		if (ActorToDie->getScene())
		{
			ActorToDie->getScene()->removeActor(*ActorToDie);
		}
		ActorToDie->release();
	}
	ActorDeathNote.clear();
}

// FilterData0: Ray
// FilterData1: Shape
static PxQueryHitType::Enum WheelRaycasyPreFilter(
	PxFilterData FilterData0, PxFilterData FilterData1,
	const void* constantBlock, PxU32 constantBlockSize,
	PxHitFlags& queryFlags
)
{
	
	// 캐스터와 Shape이 ID가 같으면 무시(word3: 안 쓰는 비트에 차량 ID 넣어줌)
	if (FilterData1.word2 == 1)
	{
		if (FilterData0.word3 == FilterData1.word3)
		{
			return PxQueryHitType::eNONE;
		}
	}

	
	return PxQueryHitType::eBLOCK;
}

void FPhysicsScene::InitVehicleSDK()
{
	uint32 NumRaysMax = MAX_VEHICLE * MAX_WHEELS_PER_VEHICLE;

	RaycastResult.SetNum(NumRaysMax);
	RaycastHits.SetNum(NumRaysMax);

	//t
	PxBatchQueryDesc QueryDesc(NumRaysMax, 0, 0);
	QueryDesc.queryMemory.userRaycastResultBuffer = RaycastResult.GetData();
	QueryDesc.queryMemory.userRaycastTouchBuffer = RaycastHits.GetData();
	QueryDesc.queryMemory.raycastTouchBufferSize = NumRaysMax;
	QueryDesc.preFilterShader = WheelRaycasyPreFilter;

	BatchQuery = Scene->createBatchQuery(QueryDesc);

	

	PxVehicleDrivableSurfaceType SurfaceType[1];
	SurfaceType[0].mType = 0;

	const PxMaterial* Materials[] = { FPhysicsSystem::GetInstance().GetDefaultMaterial() };

	// 타이어 1개, 표면 재질 1개만 매핑
	FrictionPairs = PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(2, 1);
	// 표면과 표면 타입 매핑
	FrictionPairs->setup(2, 1, Materials, SurfaceType);

	// 0번 표면 타입에서(앞바퀴) 0번 타이어 마찰력은 1.0f
	FrictionPairs->setTypePairFriction(0, 0, 1.0f);
	// 뒷바퀴 2배
	FrictionPairs->setTypePairFriction(0, 1, 2.0f);

	VehicleSDKInitialized = true;
}

void FPhysicsScene::ReleaseVehicleSDK()
{
	if (BatchQuery)
	{

		BatchQuery->release();
		BatchQuery = nullptr;
	}

	if (FrictionPairs)
	{
		FrictionPairs->release();
		FrictionPairs = nullptr;
	}
}

PxVec3 FPhysicsScene::GetGravity()
{
	return Scene->getGravity();
}

PxBatchQuery* FPhysicsScene::GetBatchQuery()
{
	return BatchQuery;
}

PxRaycastQueryResult* FPhysicsScene::GetRaycastQueryResult()
{
	return RaycastResult.GetData();
}

uint32 FPhysicsScene::GetRaycastQueryResultSize()
{
	return RaycastResult.Num();
}

PxVehicleDrivableSurfaceToTireFrictionPairs* FPhysicsScene::GetFrictionPairs()
{
	return FrictionPairs;
}

bool FPhysicsScene::Raycast(const FVector& Origin, const FVector& Direction, float MaxDistance, FHitResult& OutHit)
{
	if (!Scene) return false;

    PxVec3 POrigin = PhysxConverter::ToPxVec3(Origin); 
    PxVec3 PDir = PhysxConverter::ToPxVec3(Direction);
    PDir.normalize();

    PxRaycastBuffer HitBuffer;
    
    // Static(벽)과 Dynamic(캐릭터/물체) 모두 충돌 검사
    PxQueryFilterData FilterData(PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC);
    
    bool bHit = Scene->raycast(POrigin, PDir, MaxDistance, HitBuffer, PxHitFlag::eDEFAULT, FilterData);

    // 4. 충돌 결과 처리
    if (bHit && HitBuffer.hasBlock)
    {
        const PxRaycastHit& PxHit = HitBuffer.block;

        // --- 결과 채우기 ---
        OutHit.bBlockingHit = true;
        OutHit.Distance = PxHit.distance;
        OutHit.ImpactPoint = PhysxConverter::ToFVector(PxHit.position);
        OutHit.ImpactNormal = PhysxConverter::ToFVector(PxHit.normal);

        if (PxHit.actor && PxHit.actor->userData)
        {
            FBodyInstance* BodyInst = static_cast<FBodyInstance*>(PxHit.actor->userData);
            if (BodyInst && BodyInst->OwnerComponent)
            {
                OutHit.Component = BodyInst->OwnerComponent;
                OutHit.Actor = BodyInst->OwnerComponent->GetOwner();
            }
        }

        if (PxHit.shape && PxHit.shape->userData)
        {
            FName* BoneNamePtr = static_cast<FName*>(PxHit.shape->userData);
            if (BoneNamePtr)
            {
                OutHit.BoneName = *BoneNamePtr;
            }
        }
        
        // Face Index (필요시)
        OutHit.Item = PxHit.faceIndex;

        return true;
    }

    // 충돌 없음
    OutHit.bBlockingHit = false;
    OutHit.Distance = 0.0f;
    OutHit.Actor = nullptr;
    OutHit.Component = nullptr;
    
    return false;
}

// PairHeader: 누가 누구랑 부딪혔는지 저장
// Pair: 어떻게 부딪혔는지 충돌 정보 저장
void FPhysicsSimulationEventCallback::onContact(const PxContactPairHeader& PairHeader, const PxContactPair* Pairs, PxU32 NumPairs)
{
    UPrimitiveComponent* CompA = GetCompFromPxActor(PairHeader.actors[0]);
    UPrimitiveComponent* CompB = GetCompFromPxActor(PairHeader.actors[1]);

    if (!CompA || !CompB || CompA->IsPendingDestroy() || CompB->IsPendingDestroy()) { return; }

    AActor* ActorA = CompA->GetOwner();
    AActor* ActorB = CompB->GetOwner();

    if (!ActorA || !ActorB) { return; }

    PxContactPairPoint ContactPoints[16];

    for (PxU32 i = 0; i < NumPairs; ++i)
    {
        const PxContactPair& CP = Pairs[i];

        if (CP.events & PxPairFlag::eNOTIFY_TOUCH_FOUND)
        {
            const PxShape* ShapeA = CP.shapes[0];
            const PxShape* ShapeB = CP.shapes[1];

            FName BoneNameA = GetBoneNameFromShape(ShapeA);
            FName BoneNameB = GetBoneNameFromShape(ShapeB);
            
            PxU32 ContactCount = CP.extractContacts(ContactPoints, 16);

            if (ContactCount > 0)
            {
                const PxContactPairPoint& Point = ContactPoints[0];
                FVector WorldNormal = PhysxConverter::ToFVector(Point.normal);
                FVector WorldPos = PhysxConverter::ToFVector(Point.position);

                // --- [HitResult A 생성 (A 입장에서의 충돌)] ---
                FHitResult HitA;
                HitA.bBlockingHit = true;
                HitA.Actor = ActorB;
                HitA.Component = CompB;
                HitA.ImpactPoint = WorldPos;
                HitA.ImpactNormal = WorldNormal; 
                HitA.Location = CompA->GetWorldLocation();
                HitA.Distance = 0.0f; // 물리 충돌은 거리 0
                HitA.Item = Point.internalFaceIndex1;
                HitA.MyBoneName = BoneNameA; 
                HitA.BoneName = BoneNameB;
                
                ActorA->OnComponentHit.Broadcast(CompA, CompB, HitA);

                // --- [HitResult B 생성 (B 입장에서의 충돌)] ---
                FHitResult HitB;
                HitB.bBlockingHit = true;
                HitB.Actor = ActorA;
                HitB.Component = CompA;
                HitB.ImpactPoint = WorldPos;
                HitB.ImpactNormal = -WorldNormal; 
                HitB.Location = CompB->GetWorldLocation();
                HitB.Distance = 0.0f; // 물리 충돌은 거리 0
                HitB.Item = Point.internalFaceIndex1;
                HitA.MyBoneName = BoneNameB;
                HitA.BoneName = BoneNameA;

                ActorB->OnComponentHit.Broadcast(CompA, CompB, HitB);
            }
        }
    }
}

void FPhysicsSimulationEventCallback::onTrigger(PxTriggerPair* Pairs, PxU32 Count)
{
	// 이번 호출에서 이미 이벤트를 보낸 액터 쌍을 기억하기 위한 Set
	// Key: 두 액터의 포인터를 조합한 해시값 혹은 단순 포인터 쌍
	std::set<std::pair<AActor*, AActor*>> ProcessedPairs;
	
	for (PxU32 i = 0; i < Count; ++i)
	{
		const PxTriggerPair& TP = Pairs[i];

		// 삭제된 액터면 패스
		if (TP.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
			continue;
        
		UPrimitiveComponent* TriggerComp = GetCompFromPxActor(TP.triggerActor);
		UPrimitiveComponent* OtherComp = GetCompFromPxActor(TP.otherActor);
		if (!TriggerComp || !OtherComp) { continue; }

		AActor* TriggerActor = TriggerComp->GetOwner();
		AActor* OtherActor = OtherComp->GetOwner();
		if (!TriggerActor || !OtherActor) { continue; }
       
		// 자기 자신과의 충돌은 무시 (혹시 모를 상황)
		if (TriggerActor == OtherActor) continue;
		
		AActor* ActorA = TriggerActor < OtherActor ? TriggerActor : OtherActor;
		AActor* ActorB = TriggerActor < OtherActor ? OtherActor : TriggerActor;
       
		std::pair<AActor*, AActor*> CurrentPair = { ActorA, ActorB };

		if (ProcessedPairs.find(CurrentPair) != ProcessedPairs.end())
		{
			continue; 
		}
		ProcessedPairs.insert(CurrentPair);

		// [중복 방지 2단계 - 권장] 실제 엔진처럼 하려면 Actor나 Component에 '현재 겹친 목록'을 저장해야 함.
		// 예: if (TriggerActor->IsOverlapping(OtherActor)) return; 
		// 일단 여기서는 1단계(쉐이프 중복 방지)만 적용해도 많이 개선됨.

		if (TP.status == PxPairFlag::eNOTIFY_TOUCH_FOUND)
		{
			TriggerActor->OnComponentBeginOverlap.Broadcast(TriggerComp, OtherComp);
			OtherActor->OnComponentBeginOverlap.Broadcast(OtherComp, TriggerComp);
		}
		else if (TP.status == PxPairFlag::eNOTIFY_TOUCH_LOST)
		{
			TriggerActor->OnComponentEndOverlap.Broadcast(TriggerComp, OtherComp);
			OtherActor->OnComponentEndOverlap.Broadcast(OtherComp, TriggerComp);
		}
	}
}

UPrimitiveComponent* FPhysicsSimulationEventCallback::GetCompFromPxActor(physx::PxRigidActor* InActor)
{
	if (!InActor || !InActor->userData) { return nullptr; }

	auto* BodyInst = static_cast<FBodyInstance*>(InActor->userData);
	if (!BodyInst) { return nullptr; }

	return BodyInst->OwnerComponent;
}

FName FPhysicsSimulationEventCallback::GetBoneNameFromShape(const physx::PxShape* Shape)
{
	if (!Shape || !Shape->userData) return FName();
	FName* NamePtr = static_cast<FName*>(Shape->userData);
	return *NamePtr;
}
