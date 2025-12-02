#pragma once
#include "WheelVehiclePawn.h"
#include "ATestCar.generated.h"

UCLASS(DisplayName = "테스트 카", Description = "테스트 카입니다")
class ATestCar : public AWheeledVehiclePawn
{
	GENERATED_REFLECTION_BODY()

public:
	ATestCar();

	void BeginPlay() override;

	void DuplicateSubObjects() override;

	void Tick(float DeltaTime) override;
};