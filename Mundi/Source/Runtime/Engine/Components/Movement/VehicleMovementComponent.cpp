#include "pch.h"
#include "VehicleMovementComponent.h"
#include "PhysicsScene.h"

uint32 UVehicleMovementComponent::VehicleID = 0;

void UVehicleMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();



	GWorld->GetPhysicsScene()->RegisterPrePhysics(this);
}

// 0.0 ~ 1.0
void UVehicleMovementComponent::SetThrottle(float Value)
{
    if (!VehicleInstance)
    {
        return;
    }

    PxVehicleDrive4W* Car = (PxVehicleDrive4W*)VehicleInstance;

    // 그냥 내부 변수 바꾸는거고 physxActor 업데이트 하는게 아니라서 틱 도중에 해도 됨.
    Car->mDriveDynData.setAnalogInput(PxVehicleDrive4WControl::eANALOG_INPUT_ACCEL, Value);
}

void UVehicleMovementComponent::SetBrake(float Value)
{
    if (!VehicleInstance)
    {
        return;
    }
    PxVehicleDrive4W* Car = (PxVehicleDrive4W*)VehicleInstance;
    Car->mDriveDynData.setAnalogInput(PxVehicleDrive4WControl::eANALOG_INPUT_BRAKE, Value);
}


void UVehicleMovementComponent::SetSteering(float Value)
{
    if (!VehicleInstance)
    {
        return;
    }
    PxVehicleDrive4W* Car = (PxVehicleDrive4W*)VehicleInstance;
    Car->mDriveDynData.setAnalogInput(PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT, Value);
}

void UVehicleMovementComponent::SetGear(int32 TargetGear)
{
    if (!VehicleInstance) return;
    PxVehicleDrive4W* Car = (PxVehicleDrive4W*)VehicleInstance;

    // 강제로 기어 박기
    Car->mDriveDynData.forceGearChange(TargetGear);
}

void UVehicleMovementComponent::PrePhysicsUpdate(float DeltaTime)
{
	if (VehicleInstance)
	{
        FPhysicsScene* Scene = GWorld->GetPhysicsScene();

        // 바퀴에서 땅으로 레이캐스팅, 결과(서스펜션, 땅과 바퀴 FrictionPair 등등) Instance에 저장
        // 업데이트 전에 해야 업데이트 반영이 되고 틱 도중에 하면 락걸려서 비효율적임
        PxVehicleSuspensionRaycasts(
            Scene->GetBatchQuery(),
            1, &VehicleInstance,
            Scene->GetRaycastQueryResultSize(),
            Scene->GetRaycastQueryResult());

        // 위의 결과를 이용해서 Update(Gravity와 서스펜션 밀당)
        // 왜 분리? -> 커스텀으로 mWheelsDynData 조작해서 서스펜션, 마찰 계산 가능/ 상황에 따라 스윕 함수 사용 가능/ 스케쥴링 다르게 적용 가능
        PxVehicleUpdates(
            DeltaTime,
            Scene->GetGravity(),
            *Scene->GetFrictionPairs(),
            1, &VehicleInstance,
            nullptr);
	}
}
