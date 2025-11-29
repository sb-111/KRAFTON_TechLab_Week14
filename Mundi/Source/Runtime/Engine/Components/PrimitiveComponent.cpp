#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "WorldPartitionManager.h"
#include "BodyInstance.h"

// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UPrimitiveComponent::UPrimitiveComponent() : bGenerateOverlapEvents(true)
{
}


void UPrimitiveComponent::ApplyPhysicsResult()
{
    if (BodyInstance && BodyInstance->RigidActor)
    {
        physx::PxTransform PhysicsTransform = BodyInstance->RigidActor->getGlobalPose();

        FTransform Transform = PhysxConverter::ToFTransform(PhysicsTransform);

        Transform.Scale3D = GetWorldScale();

        SetWorldTransform(Transform);
    }
}

// true인 경우 하나의 엑터에 셰입들이 용접됨. 
bool UPrimitiveComponent::ShouldWelding()
{
    UPrimitiveComponent* PrimitiveParent = GetPrimitiveParent();
    
    if (PrimitiveParent)
    {
        if (PrimitiveParent->MobilityType == EMobilityType::Movable && MobilityType == EMobilityType::Static)
        {
            UE_LOG("부모가 Movable인데 자식이 Static일 수 없습니다, 자식을 Movable로 변환합니다");
            MobilityType = EMobilityType::Movable;
        }
        if (bSimulatePhysics && MobilityType == EMobilityType::Static)
        {
            UE_LOG("Static이면서 동시에 SimulatePhysics할 수 없습니다, SimulatePhysics를 Off합니다");
            bSimulatePhysics = false;
        }
        // 자식이 독립적으로 시뮬레이션 되지 않고 부모와 모빌리티가 같을때만 용접
        if (PrimitiveParent->MobilityType == MobilityType && !bSimulatePhysics)
        {
            return true;
        }
        
    }
}

UPrimitiveComponent* UPrimitiveComponent::GetPrimitiveParent()
{
    USceneComponent* Parent = AttachParent;

    while (Parent)
    {
        if (UPrimitiveComponent* PrimitiveParent = Cast<UPrimitiveComponent>(Parent))
        {
            return PrimitiveParent;
        }
        Parent = Parent->GetAttachParent();
    }
    return nullptr;
}

physx::PxGeometryHolder UPrimitiveComponent::GetGeometry()
{
    return physx::PxGeometryHolder();
}


void UPrimitiveComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);

    // UStaticMeshComponent라면 World Partition에 추가. (null 체크는 Register 내부에서 수행)
    if (InWorld)
    {
        if (UWorldPartitionManager* Partition = InWorld->GetPartitionManager())
        {
            Partition->Register(this);
        }
    }
}

void UPrimitiveComponent::OnUnregister()
{
    if (UWorld* World = GetWorld())
    {
        if (UWorldPartitionManager* Partition = World->GetPartitionManager())
        {
            Partition->Unregister(this);
        }
    }

    Super::OnUnregister();
}

void UPrimitiveComponent::BeginPlay()
{
}

void UPrimitiveComponent::EndPlay()
{
    if (BodyInstance)
    {
        delete BodyInstance;
        BodyInstance = nullptr;
    }
}

void UPrimitiveComponent::SetMaterialByName(uint32 InElementIndex, const FString& InMaterialName)
{
    SetMaterial(InElementIndex, UResourceManager::GetInstance().Load<UMaterial>(InMaterialName));
} 
 
void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UPrimitiveComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* Other) const
{
    if (!Other)
    {
        return false;
    }

    const TArray<FOverlapInfo>& Infos = GetOverlapInfos();
    for (const FOverlapInfo& Info : Infos)
    {
        if (Info.Other)
        {
            if (AActor* Owner = Info.Other->GetOwner())
            {
                if (Owner == Other)
                {
                    return true;
                }
            }
        }
    }
    return false;
}
