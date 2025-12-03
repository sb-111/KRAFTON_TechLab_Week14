#include "pch.h"
#include "TestCar.h"
#include "VehicleMovementComponent4W.h"
#include "SkeletalMeshComponent.h"

ATestCar::ATestCar()
{
	VehicleMovement = CreateDefaultSubobject<UVehicleMovementComponent4W>("VehicleMovement");
	bTickInEditor = false;
}

void ATestCar::BeginPlay()
{
	Super::BeginPlay();

	if (VehicleMovement)
	{
		VehicleMovement->CreatePhysicsVehicle();
		VehicleMovement->SetGear(PxVehicleGearsData::eFIRST);
	}
	MeshComponent->bIsCar = true;

}

void ATestCar::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UVehicleMovementComponent* Movement = Cast< UVehicleMovementComponent>(Component))
		{
			VehicleMovement = Movement;
		}
		else if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Component))
		{
			MeshComponent = SkeletalMesh;
		}
	}
}

void ATestCar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
