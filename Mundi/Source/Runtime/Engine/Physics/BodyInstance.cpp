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
#include "FKConvexElem.h"

using namespace physx;

FBodyInstance::~FBodyInstance()
{
	TermBody();
}

void FBodyInstance::TermBody()
{
	if (RigidActor)
	{
		// OwnerScene을 사용하여 올바른 PhysicsScene에서 제거
		// (PIE 전환 시 GWorld가 바뀌어도 원래 등록된 Scene에서 제거됨)
		if (OwnerScene)
		{
			OwnerScene->WriteInTheDeathNote(RigidActor);
		}
		else if (GWorld && GWorld->GetPhysicsScene())
		{
			// 폴백: OwnerScene이 없으면 GWorld 사용 (기존 동작)
			GWorld->GetPhysicsScene()->WriteInTheDeathNote(RigidActor);
		}
		RigidActor = nullptr;
	}
	OwnerScene = nullptr;
	BodySetup = nullptr;
	BoneIndex = -1;
}

void FBodyInstance::AddForce(const FVector& InForce)
{
	PxRigidActor* Actor = RigidActor;
	GWorld->GetPhysicsScene()->EnqueueCommand([Actor, InForce] {
		if (Actor)
		{
			PxRigidBody* Body = Actor->is<PxRigidBody>();
			if (Body)
			{
				Body->addForce(PhysxConverter::ToPxVec3(InForce));
			}
		}
		});
	/*bHasPendingForce = true;
	PendingForce += InForce;*/
}

void FBodyInstance::AddTorque(const FVector& InTorque)
{
	PxRigidActor* Actor = RigidActor;
	GWorld->GetPhysicsScene()->EnqueueCommand([Actor, InTorque] {
		if (Actor)
		{
			PxRigidBody* Body = Actor->is<PxRigidBody>();
			if (Body)
			{
				Body->addTorque(PhysxConverter::ToPxVec3(InTorque));
			}
		}
		});
	/*bHasPendingForce = true;
	PendingTorque += InTorque;*/
}

void FBodyInstance::UpdateTransform(const FTransform& InTransform)
{
	PxRigidActor* Actor = RigidActor;
	GWorld->GetPhysicsScene()->EnqueueCommand([Actor, InTransform] {
		if (Actor)
		{
			PxRigidBody* Body = Actor->is<PxRigidBody>();
			if (Body)
			{
				Body->setGlobalPose(PhysxConverter::ToPxTransform(InTransform));
				//Body->setLinearVelocity(PxVec3(0, 0, 0));
			}
		}
		});
}

//void FBodyInstance::FlushPendingForce()
//{
//	if (!bHasPendingForce)
//	{
//		return;
//	}
//	PxRigidBody* RigidBody = RigidActor->is<PxRigidBody>();
//
//	if (RigidBody)
//	{
//		RigidBody->addForce(PhysxConverter::ToPxVec3(PendingForce));
//		RigidBody->addTorque(PhysxConverter::ToPxVec3(PendingTorque));
//	}
//	PendingForce = FVector::Zero();
//	PendingTorque = FVector::Zero();
//
//	bHasPendingForce = false;
//}

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
		PxRigidDynamic* DynamicActor = PhysicsSystem.GetPhysics()->createRigidDynamic(InitTransform);
		if (InOwnerComponent->bSimulatePhysics)
		{
			DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
		}
		else
		{
			DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
		}
		NewActor = DynamicActor;
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

	// Scene에 추가하고 OwnerScene 저장 (PIE 전환 시 올바른 Scene에서 제거하기 위해)
	// 컴포넌트의 World 사용 (Preview World 지원)
	UWorld* OwnerWorld = InOwnerComponent->GetWorld();
	if (!OwnerWorld) OwnerWorld = GWorld;
	FPhysicsScene* Scene = OwnerWorld->GetPhysicsScene();
	Scene->AddActor(*NewActor);
	OwnerScene = Scene;
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
			if (StaticMeshComponent->GetStaticMesh()->GetBodySetup())
			{
				if (StaticMeshComponent->GetStaticMesh()->GetBodySetup()->AggGeom.ConvexElems.Num() > 0)
				{
					FKConvexElem* Convex = &StaticMeshComponent->GetStaticMesh()->GetBodySetup()->AggGeom.ConvexElems[0];
					CreateShapeFromConvexElement(Convex, PhysicsActor);
				}
			}
			
			
			
			
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
{PxShapeFlags NewFlags = PxShapeFlag::eVISUALIZATION; 

	// 트리거 여부 우선 확인
	if (Component->bIsTrigger)
	{
		NewFlags |= PxShapeFlag::eTRIGGER_SHAPE;
		NewFlags |= PxShapeFlag::eSCENE_QUERY_SHAPE;
	}
	else
	{
		switch (Component->CollisionType)
		{
		case ECollisionEnabled::None:
			// 아무것도 안 함
			break;
		case ECollisionEnabled::QueryOnly:
			NewFlags |= PxShapeFlag::eSCENE_QUERY_SHAPE;
			break;
		case ECollisionEnabled::PhysicsOnly:
			NewFlags |= PxShapeFlag::eSIMULATION_SHAPE;
			break;
		case ECollisionEnabled::PhysicsAndQuery:
			NewFlags |= PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE;
			break;
		}
	}

	// 4. 완성된 플래그를 한 번에 적용 (중간 상태가 존재하지 않아 안전함)
	Shape->setFlags(NewFlags);
}

// Shape별 CollisionEnabled를 직접 받는 함수 (래그돌용)
static void SetCollisionTypeFromShapeSettings(PxShape* Shape, ECollisionEnabled CollisionEnabled)
{
	switch (CollisionEnabled)
	{
	case ECollisionEnabled::None:
		Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
		break;
	case ECollisionEnabled::QueryOnly:
		Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
		break;
	case ECollisionEnabled::PhysicsOnly:
		Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
		break;
	case ECollisionEnabled::PhysicsAndQuery:
	default:
		Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
		break;
	}
}

// ===== 래그돌용 함수들 (언리얼 방식) =====

void FBodyInstance::InitBody(UBodySetup* Setup, UPrimitiveComponent* InOwnerComponent, const FTransform& WorldTransform, int32 InBoneIndex, uint32 InRagdollOwnerID)
{
	if (!Setup) return;
	if (RigidActor) return;	// 이미 초기화됨

	FPhysicsSystem& PhysicsSystem = FPhysicsSystem::GetInstance();

	// OwnerComponent의 World 사용 (피직스 에셋 에디터의 Preview World 지원)
	UWorld* OwnerWorld = InOwnerComponent->GetWorld();
	if (!OwnerWorld) OwnerWorld = GWorld;
	if (!PhysicsSystem.GetPhysics() || !OwnerWorld || !OwnerWorld->GetPhysicsScene()) return;

	BodySetup = Setup;
	BoneIndex = InBoneIndex;
	RagdollOwnerID = InRagdollOwnerID;
	OwnerComponent = InOwnerComponent;

	PxTransform InitTransform = PhysxConverter::ToPxTransform(WorldTransform);
	PxRigidActor* Body = nullptr;

	// Preview World이거나 Movable이거나 PIE가 아니면 RigidDynamic 생성
	bool bIsPreviewWorld = (OwnerWorld->GetWorldType() == EWorldType::PreviewMinimal);
	if (InOwnerComponent->MobilityType == EMobilityType::Movable || !OwnerWorld->bPie || bIsPreviewWorld)
	{
		// RigidDynamic: 움직이는 강체
		PxRigidDynamic* DynamicActor = PhysicsSystem.GetPhysics()->createRigidDynamic(InitTransform);
		if (!DynamicActor) return;

		// Preview World에서는 항상 kinematic false로 시작 (시뮬레이션 가능하게)
		if (InOwnerComponent->bSimulatePhysics || bIsPreviewWorld)
		{
			DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
		}
		else
		{
			DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
		}
		Body = DynamicActor;
		CreateShapesFromBodySetup(Setup, Body, InOwnerComponent);
		// Damping 설정
		DynamicActor->setLinearDamping(Setup->LinearDamping);
		DynamicActor->setAngularDamping(Setup->AngularDamping);
		// Setup의 MassInKg 값을 직접 사용하여 질량 설정
		PxRigidBodyExt::setMassAndUpdateInertia(*DynamicActor, Setup->MassInKg);
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
		Body = PhysicsSystem.GetPhysics()->createRigidStatic(InitTransform);
		if (!Body) return;
		CreateShapesFromBodySetup(Setup, Body, InOwnerComponent);
	}

	// Scene에 추가하고 OwnerScene 저장 (PIE 전환 시 올바른 Scene에서 제거하기 위해)
	FPhysicsScene* Scene = OwnerWorld->GetPhysicsScene();
	Scene->AddActor(*Body);
	OwnerScene = Scene;

	Body->userData = (void*)this;
	RigidActor = Body;
}

void FBodyInstance::CreateShapesFromBodySetup(UBodySetup* Setup, PxRigidActor* Body, UPrimitiveComponent* OwnerComponent)
{
	if (!Setup || !Body) return;

	FPhysicsSystem& PhysicsSystem = FPhysicsSystem::GetInstance();
	if (!PhysicsSystem.GetPhysics()) return;

	// UBodySetup의 PhysMaterial에서 값 추출 (없으면 0 사용)
	float Friction = 0.0f;
	float Restitution = 0.0f;
	if (Setup->PhysMaterial)
	{
		Friction = Setup->PhysMaterial->Friction;
		Restitution = Setup->PhysMaterial->Restitution;
	}

	// PxMaterial 생성
	PxMaterial* Material = PhysicsSystem.GetPhysics()->createMaterial(
		Friction,
		Friction,
		Restitution
	);
	if (!Material) return;

	// 래그돌 자체 충돌 방지용 Filter Data
	// word0 = Owner ID, word1 = 1 (래그돌 Shape 표시)
	PxFilterData RagdollFilterData;
	RagdollFilterData.word0 = RagdollOwnerID;
	RagdollFilterData.word1 = (RagdollOwnerID != 0) ? 1 : 0;  // 래그돌이면 1

	const FKAggregateGeom& AggGeom = Setup->AggGeom;

	// 각 Shape 생성 시 충돌 설정 적용하는 람다 (Shape별 CollisionEnabled 지원)
	auto ConfigureShape = [&](PxShape* Shape, ECollisionEnabled ShapeCollisionEnabled) {
		if (Shape)
		{
			// Shape별 CollisionEnabled 적용 (에디터에서 설정한 값 사용)
			SetCollisionTypeFromShapeSettings(Shape, ShapeCollisionEnabled);
			Shape->setSimulationFilterData(RagdollFilterData);
			Shape->userData = (void*)(uintptr_t)BoneIndex;
		}
	};

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
			ConfigureShape(Shape, Sphere.CollisionEnabled);
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
			ConfigureShape(Shape, Box.CollisionEnabled);
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
			ConfigureShape(Shape, Capsule.CollisionEnabled);
		}
	}
	// Material release (Shape들이 참조하고 있으므로 refcount만 감소)
	Material->release();
}

void FBodyInstance::CreateShapeFromConvexElement(FKConvexElem* ConvexElement, PxRigidActor* Actor)
{
	if (ConvexElement->VertexData.Num() < 3)
	{
		return;
	}
	PxConvexMesh* ConvexMesh = FPhysicsSystem::GetInstance().CreateConvexMesh(ConvexElement);

	if (!ConvexMesh)
	{
		return;
	}
	PxVec3 Scale = PhysxConverter::ToPxVec3(ConvexElement->Transform.Scale3D);
	Scale.x = std::abs(Scale.x);	Scale.y = std::abs(Scale.y); Scale.z = std::abs(Scale.z);
	PxMeshScale MeshScale(Scale, PxQuat(PxIdentity));

	PxConvexMeshGeometry ConvexGeometry(ConvexMesh, MeshScale);

	PxMaterial* Material = FPhysicsSystem::GetInstance().GetDefaultMaterial();

	PxShape* NewShape = PxRigidActorExt::createExclusiveShape(*Actor, ConvexGeometry, *Material);

	if (NewShape)
	{
		FTransform TransformUnitScale = ConvexElement->Transform;
		TransformUnitScale.Scale3D = FVector(1.0f);

		bool bSimulate = (ConvexElement->CollisionEnabled == ECollisionEnabled::PhysicsAndQuery || ConvexElement->CollisionEnabled == ECollisionEnabled::PhysicsOnly);
		bool bQuery = (ConvexElement->CollisionEnabled == ECollisionEnabled::PhysicsAndQuery || ConvexElement->CollisionEnabled == ECollisionEnabled::QueryOnly);

		NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, bSimulate);
		NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, bQuery);
	}
	ConvexMesh->release();
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
