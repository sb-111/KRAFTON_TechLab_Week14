#pragma once

#include "VehicleMovementComponent.h"
#include "UVehicleMovementComponent4W.generated.h"

using namespace physx;

UCLASS(DisplayName = "4륜 비히클", Description = "4륜 비히클입니다")
class UVehicleMovementComponent4W : public UVehicleMovementComponent
{
	GENERATED_REFLECTION_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Wheel")
	FVector WheelOffset0{};

	UPROPERTY(EditAnywhere, Category = "Wheel")
	FVector WheelOffset1{};

	UPROPERTY(EditAnywhere, Category = "Wheel")
	FVector WheelOffset2{};

	UPROPERTY(EditAnywhere, Category = "Wheel")
	FVector WheelOffset3{};

	UPROPERTY(EditAnywhere, Category = "Wheel")
	float WheelMass = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Wheel")
	float WheelRadius = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Wheel")
	float WheelWidth = 0.20f;

	//관성모먼트
	UPROPERTY(EditAnywhere, Category = "Wheel")
	float WheelInertiaFactor = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Wheel")
	float MaxBrakeTorque = 1500.0f;


	// 서스펜션 눌리는 정도
	UPROPERTY(EditAnywhere, Category = "Suspension")
	float MaxCompression = 0.3f;
	
	// 서스펜션 늘어나는 정도
	UPROPERTY(EditAnywhere, Category = "Suspension")
	float MaxDroop = 0.1f;

	// 스프링 강도
	UPROPERTY(EditAnywhere, Category = "Suspension")
	float SpringStrength = 35000.0f;

	// 출렁거림 방지
	UPROPERTY(EditAnywhere, Category = "Suspension")
	float SpringDamperRate = 4500.0f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float PeakTorque = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float MaxRPM = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float GearSwitchTime = 0.5f;

	PxVehicleDriveSimData4W DriveSimData;
	PxVehicleWheelsSimData* WheelsSimData = nullptr;

	UVehicleMovementComponent4W() = default;

	physx::PxVehicleWheels* CreatePhysicsVehicle() override;

	void InitWheelSimData(PxRigidDynamic* RigidDynamic);

	void InitDriveSimData();

private:


};