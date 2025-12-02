#include "pch.h"
#include "TestCar.h"
#include "VehicleMovementComponent4W.h"
#include "BoxComponent.h"
#include "StaticMeshComponent.h"

ATestCar::ATestCar()
{
	VehicleMovement = CreateDefaultSubobject<UVehicleMovementComponent4W>("VehicleMovement");
	
}

void ATestCar::BeginPlay()
{
	Super::BeginPlay();

	if (VehicleMovement)
	{
		VehicleMovement->CreatePhysicsVehicle();
		VehicleMovement->SetGear(PxVehicleGearsData::eFIRST);
	}

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
		else if (UBoxComponent* SkeletalMesh = Cast<UBoxComponent>(Component))
		{
			MeshComponent = SkeletalMesh;
		}
	}
}

void ATestCar::Tick(float DeltaTime)
{
	VehicleMovement->SetThrottle(1.0f);
}
