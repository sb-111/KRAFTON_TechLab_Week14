#pragma once
#include "Pawn.h"
#include "AWheeledVehiclePawn.generated.h"

UCLASS(Abstract)
class AWheeledVehiclePawn : public APawn
{
	GENERATED_REFLECTION_BODY()
protected:

	class UVehicleMovementComponent* VehicleMovement = nullptr;
	class USkeletalMeshComponent* MeshComponent = nullptr;

public:

	AWheeledVehiclePawn();

	void SetThrottle(float Value);

	void SetBrake(float Value);

	void SetSteering(float Value);

	void SetGear(int32 InGear);
}; 