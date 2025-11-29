#include "pch.h"
#include "BodyInstance.h"
#include "PrimitiveComponent.h"
#include "PhysicsSystem.h"
#include "StaticMeshComponent.h"

using namespace physx;

FBodyInstance::FBodyInstance(UPrimitiveComponent* InOwnerComponent)
	:OwnerComponent(InOwnerComponent)
{
}

FBodyInstance::~FBodyInstance()
{
	if (RigidActor)
	{
		if (RigidActor->getScene())
		{
			RigidActor->getScene()->removeActor(*RigidActor);
		}
		RigidActor->release();
		RigidActor = nullptr;
	}
}

void FBodyInstance::InitPhysics(UPrimitiveComponent* InOwnerComponent)
{
	if (!InOwnerComponent || 
		InOwnerComponent->BodyInstance ||	// 이미 인스턴스 존재
		InOwnerComponent->CollisionType == ECollisionEnabled::None)
	{
		return;
	}

	FBodyInstance* NewInstance = new FBodyInstance(InOwnerComponent);

	// 소멸자 호출, 업데이트 데이터 가져오는 용도
	InOwnerComponent->BodyInstance = NewInstance;
	
	FPhysicsSystem& PhysicsSystem = FPhysicsSystem::GetInstance();

	PxTransform InitTransform = PhysxConverter::ToPxTransform(InOwnerComponent->GetWorldTransform());

	PxRigidActor* NewActor = nullptr;
	if (InOwnerComponent->MobilityType == EMobilityType::Movable)
	{
		// RigidDynamic: 움직이는 강체
		NewActor = PhysicsSystem.GetPhysics()->createRigidDynamic(InitTransform);
	}
	else
	{
		NewActor = PhysicsSystem.GetPhysics()->createRigidStatic(InitTransform);
	}


	AddShapesRecursively(InOwnerComponent, InOwnerComponent, NewActor);

	// 시뮬레이션 액터에 인스턴스를 참조시켜서 이벤트 처리 시 인스턴스 호출하게 하고 인스턴스가 컴포넌트 호출.
	NewActor->userData = (void*)NewInstance;
	NewInstance->RigidActor = NewActor;

	PhysicsSystem.GetScene()->addActor(*NewActor);
}

void FBodyInstance::AddShapesRecursively(USceneComponent* CurrentComponent, UPrimitiveComponent* RootComponent, PxRigidActor* PhysicsActor)
{
	FTransform WorldTransform = CurrentComponent->GetWorldTransform();
	FTransform RootWorldTransform = RootComponent->GetWorldTransform();
	FTransform LocalTransform = RootWorldTransform.GetRelativeTransform(WorldTransform);

	PxShape* Shape = nullptr;
	if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(CurrentComponent))
	{
		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PrimitiveComponent))
		{
			// TODO: 에셋화한 지오매트리 필요, 일단 구현 패스
		}
		else
		{
			// Shape을 감싸는 홀더, 어떤 Shape이든 저장하고 받기 위함
			PxGeometryHolder Geometry = PrimitiveComponent->GetGeometry();

			if (Geometry.getType() != PxGeometryType::eINVALID)
			{
				PxMaterial* Material = FPhysicsSystem::GetInstance().GetDefaultMaterial();

				// 실제 Shape, Exclusive: Actor가 독점하는 Shape -> Actor 소멸시 같이 소멸, Material은 공유자원이라 소멸하지 않음
				Shape = PxRigidActorExt::createExclusiveShape(*PhysicsActor, Geometry.any(), *Material);

				Shape->setLocalPose(PhysxConverter::ToPxTransform(LocalTransform));

				SetCollisionType(Shape, PrimitiveComponent);
			}
		}
	}

	const TArray<USceneComponent*>& Children = CurrentComponent->GetAttachChildren();
	for (int Index = 0; Index < Children.Num(); Index++)
	{
		if (UPrimitiveComponent* Child = Cast<UPrimitiveComponent>(Children[Index]))
		{
			if (Child->CollisionType == ECollisionEnabled::None)
			{
				continue;
			}
			if (!Child->ShouldWelding())
			{
				continue;
			}
		}
		// 씬컴 자식도 프컴일 수 있어서 테스트해야됌
		AddShapesRecursively(Children[Index], RootComponent, PhysicsActor);
	}
}

void FBodyInstance::SetCollisionType(PxShape* Shape, UPrimitiveComponent* Component)
{
	switch (Component->CollisionType)
	{
	case ECollisionEnabled::QueryOnly:
	{
		Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
	}
	break;
	case ECollisionEnabled::PhysicsOnly:
	{
		Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
	}
	break;
	case ECollisionEnabled::PhysicsAndQuery:
	{
		Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
	}
	}
	// 충돌처리 하지 않고 이벤트만 처리하는 컴포넌트
	if (Component->bIsTrigger)
	{
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
		Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
	}
}
