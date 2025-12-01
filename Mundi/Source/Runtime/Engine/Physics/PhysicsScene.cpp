#include "pch.h"
#include "PhysicsScene.h"
#include "BodyInstance.h"
#include "PrimitiveComponent.h"

FPhysicsScene::FPhysicsScene(PxScene* InScene)
	:Scene(InScene)
{

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

void FPhysicsScene::RegisterTemporal(IPrePhysics* Object)
{
	PreUpdateListTemporal.Add(Object);
}

void FPhysicsScene::UnRegisterTemporal(IPrePhysics* Object)
{
	for (int Index = 0; Index < PreUpdateListTemporal.Num(); Index++)
	{
		if (PreUpdateListTemporal[Index] == Object)
		{
			PreUpdateListTemporal.RemoveAtSwap(Index, 1);
			return;
		}
	}
}

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
	for (IPrePhysics* PrePhysics : PreUpdateListTemporal)
	{
		PrePhysics->PrePhysicsUpdate(DeltaTime);
	}
	PreUpdateListTemporal.clear();
	Scene->simulate(DeltaTime);
}

void FPhysicsScene::FetchAndUpdate()
{
	Scene->fetchResults(true);

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
	UnRegisterTemporal(Instance->OwnerComponent);
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

