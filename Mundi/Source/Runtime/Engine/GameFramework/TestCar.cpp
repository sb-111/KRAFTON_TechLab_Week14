#include "pch.h"
#include "TestCar.h"
#include "SkeletalMeshComponent.h"
#include "Movement/VehicleMovementComponent4W.h"

ATestCar::ATestCar()
{
	bTickInEditor = false;
}

void ATestCar::BeginPlay()
{
	Super::BeginPlay();

	if (!(VehicleMovement = Cast<UVehicleMovementComponent4W>(GetComponent(UVehicleMovementComponent4W::StaticClass()))))
	{
		VehicleMovement = CreateDefaultSubobject<UVehicleMovementComponent4W>("VehicleMovement");
	}
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
