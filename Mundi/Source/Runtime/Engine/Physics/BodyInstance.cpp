#include "pch.h"
#include "BodyInstance.h"
#include "PrimitiveComponent.h"
#include "PhysicsSystem.h"
#include "PhysicsScene.h"
#include "StaticMeshComponent.h"
#include "extensions/PxRigidBodyExt.h"
#include "BodySetup.h"
#include "AggregateGeometry.h"
#include "PhysxConverter.h"

using namespace physx;

FBodyInstance::~FBodyInstance()
{
	TermBody();
}

void FBodyInstance::TermBody()
{
	if (RigidActor)
	{
		GWorld->GetPhysicsScene()->WriteInTheDeathNote(RigidActor);
		RigidActor = nullptr;
	}
	BodySetup = nullptr;
	BoneIndex = -1;
}

void FBodyInstance::AddForce(const FVector& InForce)
{
	bHasPendingForce = true;
	PendingForce += InForce;
}

void FBodyInstance::AddTorque(const FVector& InTorque)
{
	bHasPendingForce = true;
	PendingTorque += InTorque;
}

void FBodyInstance::FlushPendingForce()
{
	if (!bHasPendingForce)
	{
		return;
	}
	PxRigidBody* RigidBody = RigidActor->is<PxRigidBody>();

	if (RigidBody)
	{
		RigidBody->addForce(PhysxConverter::ToPxVec3(PendingForce));
		RigidBody->addTorque(PhysxConverter::ToPxVec3(PendingTorque));
	}
	PendingForce = FVector::Zero();
	PendingTorque = FVector::Zero();

	bHasPendingForce = false;
}

void FBodyInstance::UpdateMassProperty()
{
	if (!RigidActor)
	{
		return;
	}
	PxRigidBody* RigidBody = RigidActor->is<PxRigidBody>();

	if (!RigidBody)
	{
		UE_LOG("Static Actor는 질량을 바꿀 수 없습니다.");
		return;
	}
	
	if (OwnerComponent->bOverrideMass)
	{
		PxRigidBodyExt::setMassAndUpdateInertia(*RigidBody, OwnerComponent->Mass);
	}
	else
	{
		PxRigidBodyExt::updateMassAndInertia(*RigidBody, OwnerComponent->Density);
	}

}

void FBodyInstance::InitPhysics(UPrimitiveComponent* InOwnerComponent)
{
	if (RigidActor || InOwnerComponent->CollisionType == ECollisionEnabled::None)
	{
		return;
	}

	OwnerComponent = InOwnerComponent;
	
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
		// 프로퍼티에서 Setter를 부르지 않아서 ShouldWelding과 코드가 겹침. 
		// TODO: 프로퍼티에서 수정 시 즉시 Invalid상태 방어하기
		if (InOwnerComponent->bSimulatePhysics)
		{
			UE_LOG("Static이면서 동시에 SimulatePhysics할 수 없습니다, SimulatePhysics를 Off합니다");
			InOwnerComponent->bSimulatePhysics = false;
		}
		NewActor = PhysicsSystem.GetPhysics()->createRigidStatic(InitTransform);
	}


	AddShapesRecursively(InOwnerComponent, InOwnerComponent, NewActor);

	// 시뮬레이션 액터에 인스턴스를 참조시켜서 이벤트 처리 시 인스턴스 호출하게 하고 인스턴스가 컴포넌트 호출.
	NewActor->userData = (void*)this;
	RigidActor = NewActor;

	UpdateMassProperty();

	GWorld->GetPhysicsScene()->AddActor(*NewActor);
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

// ===== 래그돌용 함수들 (언리얼 방식) =====

void FBodyInstance::InitBody(UBodySetup* Setup, const FTransform& WorldTransform, int32 InBoneIndex)
{
	if (!Setup) return;
	if (RigidActor) return;	// 이미 초기화됨

	FPhysicsSystem& PhysicsSystem = FPhysicsSystem::GetInstance();
	if (!PhysicsSystem.GetPhysics() || !GWorld->GetPhysicsScene()) return;

	BodySetup = Setup;
	BoneIndex = InBoneIndex;

	// RigidDynamic 생성
	PxTransform InitTransform = PhysxConverter::ToPxTransform(WorldTransform);
	PxRigidDynamic* Body = PhysicsSystem.GetPhysics()->createRigidDynamic(InitTransform);
	if (!Body) return;

	// UBodySetup의 AggGeom에서 Shape들 생성
	CreateShapesFromBodySetup(Setup, Body);

	// 질량 설정
	PxRigidBodyExt::setMassAndUpdateInertia(*Body, Setup->MassInKg);

	// Damping 설정
	Body->setLinearDamping(Setup->LinearDamping);
	Body->setAngularDamping(Setup->AngularDamping);

	
	// Scene에 추가
	GWorld->GetPhysicsScene()->AddActor(*Body);
	Body->wakeUp();

	Body->userData = (void*)this;
	RigidActor = Body;
}

void FBodyInstance::CreateShapesFromBodySetup(UBodySetup* Setup, PxRigidDynamic* Body)
{
	if (!Setup || !Body) return;

	FPhysicsSystem& PhysicsSystem = FPhysicsSystem::GetInstance();
	if (!PhysicsSystem.GetPhysics()) return;

	// UBodySetup의 물리 재질로 PxMaterial 생성
	PxMaterial* Material = PhysicsSystem.GetPhysics()->createMaterial(
		Setup->Friction,
		Setup->Friction,
		Setup->Restitution
	);
	if (!Material) return;

	const FKAggregateGeom& AggGeom = Setup->AggGeom;

	// === Sphere Shape 생성 ===
	for (int32 i = 0; i < AggGeom.SphereElems.Num(); ++i)
	{
		const FKSphereElem& Sphere = AggGeom.SphereElems[i];
		PxSphereGeometry Geom(Sphere.Radius);
		PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Body, Geom, *Material);
		if (Shape)
		{
			PxTransform LocalPose(PhysxConverter::ToPxVec3(Sphere.Center));
			Shape->setLocalPose(LocalPose);
		}
	}

	// === Box Shape 생성 ===
	for (int32 i = 0; i < AggGeom.BoxElems.Num(); ++i)
	{
		const FKBoxElem& Box = AggGeom.BoxElems[i];
		// 좌표계 변환
		PxVec3 HalfExtent = PhysxConverter::ToPxVec3(FVector(Box.X, Box.Y, Box.Z));
		HalfExtent.x = std::abs(HalfExtent.x);
		HalfExtent.y = std::abs(HalfExtent.y);
		HalfExtent.z = std::abs(HalfExtent.z);
		PxBoxGeometry Geom(HalfExtent.x, HalfExtent.y, HalfExtent.z);
		PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Body, Geom, *Material);
		if (Shape)
		{
			FQuat Rotation = FQuat::MakeFromEulerZYX(Box.Rotation);
			PxTransform LocalPose(
				PhysxConverter::ToPxVec3(Box.Center),
				PhysxConverter::ToPxQuat(Rotation)
			);
			Shape->setLocalPose(LocalPose);
		}
	}

	// === Capsule (Sphyl) Shape 생성 ===
	for (int32 i = 0; i < AggGeom.SphylElems.Num(); ++i)
	{
		const FKSphylElem& Capsule = AggGeom.SphylElems[i];
		PxCapsuleGeometry Geom(Capsule.Radius, Capsule.Length / 2.0f);
		PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Body, Geom, *Material);
		if (Shape)
		{
			// 기본 축 회전: 엔진 캡슐(Z축) → PhysX 캡슐(X축)
			FQuat BaseRotation = FQuat::MakeFromEulerZYX(FVector(-90.0f, 0.0f, 0.0f));
			FQuat UserRotation = FQuat::MakeFromEulerZYX(Capsule.Rotation);
			FQuat FinalRotation = UserRotation * BaseRotation;

			PxTransform LocalPose(
				PhysxConverter::ToPxVec3(Capsule.Center),
				PhysxConverter::ToPxQuat(FinalRotation)
			);
			Shape->setLocalPose(LocalPose);
		}
	}

	// Material release (Shape들이 참조하고 있으므로 refcount만 감소)
	Material->release();
}

FTransform FBodyInstance::GetWorldTransform() const
{
	if (!RigidActor) return FTransform();

	PxTransform PhysicsTransform = RigidActor->getGlobalPose();
	return PhysxConverter::ToFTransform(PhysicsTransform);
}

void FBodyInstance::SetWorldTransform(const FTransform& NewTransform)
{
	if (!RigidActor) return;

	PxTransform PhysicsTransform = PhysxConverter::ToPxTransform(NewTransform);
	RigidActor->setGlobalPose(PhysicsTransform);
}

PxRigidDynamic* FBodyInstance::GetPxRigidDynamic() const
{
	return RigidActor ? RigidActor->is<PxRigidDynamic>() : nullptr;
}
