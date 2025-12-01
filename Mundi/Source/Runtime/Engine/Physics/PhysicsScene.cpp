#include "pch.h"
#include "PhysicsScene.h"
#include "BodyInstance.h"
#include "PrimitiveComponent.h"

FPhysicsScene::FPhysicsScene(PxScene* InScene)
	:Scene(InScene),
	EventCallback(std::make_unique<FPhysicsSimulationEventCallback>())
{
	Scene->setSimulationEventCallback(EventCallback.get());
}

FPhysicsScene::~FPhysicsScene()
{
	if (Scene)
	{
		// Scene 해제 전에 데스노트에 남아있는 모든 액터 정리
		// (~UWorld에서 액터 삭제 시 TermBody() → WriteInTheDeathNote()가 호출되어
		// 데스노트에 추가된 액터들이 있을 수 있음)
		PendingDestroyInDeathNote();

		Scene->release();
		Scene = nullptr;
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
	for (std::function<void()> Command : CommandQueue)
	{
		Command();
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

void FPhysicsScene::AddActor(PxActor& NewActor)
{
	Scene->addActor(NewActor);
}
 
void FPhysicsScene::Simulate(float DeltaTime)
{
	for (IPrePhysics* PrePhysics : PreUpdateList)
	{
		PrePhysics->PrePhysicsUpdate(DeltaTime);
	}
	//for (IPrePhysics* PrePhysics : PreUpdateListTemporal)
	//{
	//	PrePhysics->PrePhysicsUpdate(DeltaTime);
	//}
	//PreUpdateListTemporal.clear();
	Scene->simulate(DeltaTime);
}

void FPhysicsScene::FetchAndUpdate()
{
	Scene->fetchResults(true);

	{
		std::lock_guard<std::mutex> Guard(EventCallback->QueueMutex);
		for (const FCollisionEvent& Event : EventCallback->EventQueue)
		{
			if (Event.BodyA->OwnerComponent && Event.BodyB->OwnerComponent)
			{
				Event.BodyA->OwnerComponent->OnComponentHit(Event.BodyB->OwnerComponent);
				Event.BodyB->OwnerComponent->OnComponentHit(Event.BodyA->OwnerComponent);
				UE_LOG("ContactPosition: (%f, %f, %f)\n ContactNormal: (%f, %f, %f)\n ImpulseMagnitude: %f \n RelativeVelocity: (%f, %f, %f)\n ShapeIndex: (%d, %d)",
					Event.ContactPosition.X, Event.ContactPosition.Y, Event.ContactPosition.Z,
					Event.ContactNormal.X, Event.ContactNormal.Y, Event.ContactNormal.Z,
					Event.ImpulseMagnitude,
					Event.RelativeVelocity.X, Event.RelativeVelocity.Y, Event.RelativeVelocity.Z,
					Event.ShapeIndexA, Event.ShapeIndexA);
			}
		}
		EventCallback->EventQueue.clear();
	}

	PxU32 NumActiveActors = 0;
	PxActor** ActiveActors = Scene->getActiveActors(NumActiveActors);
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

// PairHeader: 누가 누구랑 부딪혔는지 저장
// Pair: 어떻게 부딪혔는지 충돌 정보 저장
void FPhysicsSimulationEventCallback::onContact(const PxContactPairHeader& PairHeader, const PxContactPair* Pairs, PxU32 NumPairs)
{
	if (!PairHeader.actors[0] || !PairHeader.actors[1])
	{
		return;
	}

	FBodyInstance* BodyA = (FBodyInstance*)PairHeader.actors[0]->userData;
	FBodyInstance* BodyB = (FBodyInstance*)PairHeader.actors[1]->userData;

	if (!BodyA || !BodyB)
	{
		return;
	}

	for (PxU32 Index = 0; Index < NumPairs; Index++)
	{
		const PxContactPair& Pair = Pairs[Index];

		// 비트연산으로 충돌 정보 저장 ( eNOTIFY... : 이번 프레임에 처음으로 충돌했다)
		// 필터 셰이더에서 추가했기 때문에 가능한 것. 다른 플래그도 추가해야할 수 있음
		if (Pair.events & PxPairFlag::eNOTIFY_TOUCH_FOUND)
		{
			FCollisionEvent NewEvent;
			NewEvent.BodyA = BodyA;
			NewEvent.BodyB = BodyB;

			// 충돌점 추출(Physx는 점 하나만 리턴하는게 아니라 접촉면을 만들어서 여러개 준다고 함)
			PxContactPairPoint ContactPoints[16];
			PxU32 NumContacts = Pair.extractContacts(ContactPoints, 16);

			if (NumContacts > 0)
			{
				// 일단 0번째 충돌점으로 단순계산(평균이 제일 정확한데 어차피 구별 안되서 가장 중요도 높은 0번을 쓴다고 함)
				NewEvent.ContactPosition = PhysxConverter::ToFVector(ContactPoints[0].position);
				NewEvent.ContactNormal = PhysxConverter::ToFVector(ContactPoints[0].normal);
				NewEvent.ImpulseMagnitude = ContactPoints[0].impulse.magnitude();

				// void*를(8바이트) int32로 바꾸면 컴파일 에러 -> 8바이트 정수형 포인터 변환 후 int32로 변환
				NewEvent.ShapeIndexA = (int32)(uintptr_t)Pair.shapes[0]->userData;
				NewEvent.ShapeIndexB = (int32)(uintptr_t)Pair.shapes[1]->userData;


				// RigidDynamic이 아닌 RigidBody -> PxArticulationLink -> 쓸 일 없어서 일단 기존 함수 씀.
				PxRigidDynamic* RigidDynamicA = BodyA->GetPxRigidDynamic();
				PxRigidDynamic* RigidDynamicB = BodyA->GetPxRigidDynamic();

				FVector VelocityA = RigidDynamicA ? PhysxConverter::ToFVector(RigidDynamicA->getLinearVelocity()) : FVector::Zero();
				FVector VelocityB = RigidDynamicB ? PhysxConverter::ToFVector(RigidDynamicB->getLinearVelocity()) : FVector::Zero();

				// B가 가만히 있을때 A가 박는 상황을 생각해보면 A에서 B를 빼야한다.(노말이 B에서 A쪽으로 향하니까)
				NewEvent.RelativeVelocity = VelocityA - VelocityB;
			}

			// std::lock은 unlock 명시적으로 해줘야해서 락가드 씀
			std::lock_guard<std::mutex> Guard(QueueMutex);
			EventQueue.Add(NewEvent);
		}
	}
}
