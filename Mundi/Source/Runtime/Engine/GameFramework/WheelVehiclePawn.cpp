#include "pch.h"
#include "WheelVehiclePawn.h"
#include "SkeletalMeshComponent.h"
#include "Movement/VehicleMovementComponent.h"


AWheeledVehiclePawn::AWheeledVehiclePawn()
{
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("VehicleMesh");
	RootComponent = MeshComponent;

	MeshComponent->SetSimulatePhysics(true);
}


void AWheeledVehiclePawn::ThrottleSteerInput(float Throttle, float Steer)
{
	if (Throttle > 0.1f)
	{
		PxReal ForwardSpeed = VehicleMovement->GetForwardSpeed();
		if (ForwardSpeed < 0.0f)
		{
			VehicleMovement->SetThrottle(0.0f);
			VehicleMovement->SetBrake(1.0f);
		}
		else
		{
			VehicleMovement->SetThrottle(1.0f);
			VehicleMovement->SetBrake(0.0f);
		}
		VehicleMovement->SetGear(PxVehicleGearsData::eFIRST);

	}
	else if (Throttle < -0.1f)
	{
		PxReal ForwardSpeed = VehicleMovement->GetForwardSpeed();
		if (ForwardSpeed > 0.0f)
		{
			VehicleMovement->SetThrottle(0.0f);
			VehicleMovement->SetBrake(1.0f);
		}
		else
		{
			VehicleMovement->SetBrake(0.0f);
			VehicleMovement->SetThrottle(0.8f);
			VehicleMovement->SetGear(PxVehicleGearsData::eREVERSE);
		}
	}
	else
	{
		VehicleMovement->SetThrottle(0.0f);
		VehicleMovement->SetBrake(0.7f);
	}

	if (Steer > 0.1f)
	{
		VehicleMovement->SetSteering(-1.0f);
	}
	else if (Steer < -0.1f)
	{
		VehicleMovement->SetSteering(1.0f);
	}
	else
	{
		VehicleMovement->SetSteering(0.0f);
	}
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