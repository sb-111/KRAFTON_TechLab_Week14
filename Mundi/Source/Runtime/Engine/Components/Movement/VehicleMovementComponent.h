#pragma once
#include "PrePhysics.h"
#include "MovementComponent.h"
#include "UVehicleMovementComponent.generated.h"
#include "pch.h"

UCLASS(Abstract)
class UVehicleMovementComponent : public UMovementComponent, public IPrePhysics
{
	GENERATED_REFLECTION_BODY()

protected:
	physx::PxVehicleWheels* VehicleInstance = nullptr;

	TArray<int32> WheelBoneIndex;

	static uint32 VehicleID;

public:
	UVehicleMovementComponent() = default;

	void InitializeComponent() override;

	void SetThrottle(float Value);

	void SetBrake(float Value);

	void SetSteering(float Value);

	void SetGear(int32 InGear);


	// 순수 가상함수 쓰면 리플렉션 안됨 omg
	virtual physx::PxVehicleWheels* CreatePhysicsVehicle() { return nullptr; }

	void PrePhysicsUpdate(float DeltaTime) override;

	bool bFirstFrame = true;
};
