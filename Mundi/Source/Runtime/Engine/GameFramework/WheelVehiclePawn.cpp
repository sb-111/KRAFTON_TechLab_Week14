#include "pch.h"
#include "WheelVehiclePawn.h"
#include "SkeletalMeshComponent.h"
#include "VehicleMovementComponent.h"


AWheeledVehiclePawn::AWheeledVehiclePawn()
{
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("VehicleMesh");
	RootComponent = MeshComponent;

	MeshComponent->SetSimulatePhysics(true);
}

void AWheeledVehiclePawn::SetThrottle(float Value)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetThrottle(Value);
	}
}

void AWheeledVehiclePawn::SetBrake(float Value)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetBrake(Value);
	}
}

void AWheeledVehiclePawn::SetSteering(float Value)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetSteering(Value);
	}
}

void AWheeledVehiclePawn::SetGear(int32 InGear)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetGear(InGear);
	}
}