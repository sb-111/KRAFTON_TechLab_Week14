#include "pch.h"
#include "VehicleMovementComponent4W.h"
#include "PrimitiveComponent.h"
#include "SkeletalMeshComponent.h"
#include "PhysicsSystem.h"

physx::PxVehicleWheels* UVehicleMovementComponent4W::CreatePhysicsVehicle()
{
    PxRigidDynamic* RigidDynamic = nullptr;

    if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(UpdatedComponent))
    {
        if (SkeletalMeshComponent->GetBodies().Num() > 0)
        {
            RigidDynamic = SkeletalMeshComponent->GetBodies()[0]->GetPxRigidDynamic();
        }
    }
    else if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(UpdatedComponent))
    {
        RigidDynamic = PrimitiveComponent->BodyInstance.GetPxRigidDynamic();
        //RigidDynamic->setCMassLocalPose()
    }
    if (!RigidDynamic)
    {
        return nullptr;
    }
    PxPhysics* Physics = FPhysicsSystem::GetInstance().GetPhysics();

    const PxU32 NumShapes = RigidDynamic->getNbShapes();
    TArray<PxShape*> Shapes(NumShapes);
    RigidDynamic->getShapes(Shapes.data(), NumShapes);

    PxFilterData FilterData;
    FilterData.word2 = VEHICLE_FILTER;
    FilterData.word3 = VehicleID;

    for (PxShape* Shape : Shapes)
    {
        Shape->setQueryFilterData(FilterData);
    }
    
    InitWheelSimData(RigidDynamic);

    InitDriveSimData();
    

    PxVehicleDrive4W* Car = PxVehicleDrive4W::allocate(4);
    // 0: 디퍼런셜에 연결 안된 바퀴 수
    Car->setup(FPhysicsSystem::GetInstance().GetPhysics(), RigidDynamic, *WheelsSimData, DriveSimData, 0);

    WheelsSimData->free();
    VehicleInstance = Car;
    VehicleID++;
    return Car;
}

void UVehicleMovementComponent4W::InitWheelSimData(PxRigidDynamic* RigidDynamic)
{
    WheelsSimData = PxVehicleWheelsSimData::allocate(4);

    PxVehicleWheelData WheelData;

    WheelData.mMass = WheelMass;
    WheelData.mRadius = WheelRadius;
    WheelData.mWidth = WheelWidth;
    WheelData.mMOI = WheelInertiaFactor * WheelMass * WheelRadius * WheelRadius;

    WheelData.mMaxBrakeTorque = MaxBrakeTorque;

    PxVehicleTireData TireData;
    // 타이어가 지나갈 표면(눈, 비, 진흙.. ) 일단 기본값 0
    TireData.mType = 0;

    PxVehicleSuspensionData SuspensionData;
    SuspensionData.mMaxCompression = MaxCompression;
    SuspensionData.mMaxDroop = MaxDroop;
    SuspensionData.mSpringStrength = SpringStrength;
    SuspensionData.mSpringDamperRate = SpringDamperRate;

    const PxVec3 WheelOffsets[] = { PhysxConverter::ToPxVec3(WheelOffset0), PhysxConverter::ToPxVec3(WheelOffset1),
        PhysxConverter::ToPxVec3(WheelOffset2), PhysxConverter::ToPxVec3(WheelOffset3) };

    PxF32 SprungMasses[4];
    PxVehicleComputeSprungMasses(4, WheelOffsets, PxVec3(0, 0, 0), RigidDynamic->getMass(), 1, SprungMasses);

    PxFilterData RayFilter;
    RayFilter.word3 = VehicleID;

    for (int Index = 0; Index < 4; Index++)
    {
        WheelsSimData->setSuspTravelDirection(Index, PhysxConverter::ToPxVec3(FVector(0.0f, 0.0f, -1.0f)));
        SuspensionData.mSprungMass = SprungMasses[Index];
        WheelsSimData->setWheelData(Index, WheelData);
        WheelsSimData->setTireData(Index, TireData);
        WheelsSimData->setSuspensionData(Index, SuspensionData);
        WheelsSimData->setSceneQueryFilterData(Index, RayFilter);

        WheelsSimData->setSuspForceAppPointOffset(Index, WheelOffsets[Index]);
        WheelsSimData->setWheelCentreOffset(Index, WheelOffsets[Index]);
        WheelsSimData->setWheelShapeMapping(Index, -1);

        // 서스펜션 힘 작용점 (보통 바퀴 위치랑 같게 함)
        WheelsSimData->setTireForceAppPointOffset(Index, WheelOffsets[Index]);
    }
    
}

void UVehicleMovementComponent4W::InitDriveSimData()
{
    PxVehicleEngineData EngineData;
    EngineData.mPeakTorque = PeakTorque; 
    EngineData.mMaxOmega = MaxRPM;   
    DriveSimData.setEngineData(EngineData);


    PxVehicleGearsData GearsData;
    GearsData.mSwitchTime = GearSwitchTime;
    DriveSimData.setGearsData(GearsData);

    // 디퍼런셜: 양쪽 바퀴가 다른 속도로 회전해야 함, 저항이 적은 쪽에 더 많이 돌려주는 장치.(코너 돌때 바깥쪽이 더 많이 돔)
    // 오픈 디퍼런셜: 한쪽 바퀴가 진흙에 빠져서 아예 저항이 없으면 그냥 한쪽에 싹다 몰아줌(반대쪽 바퀴는 아예 안 돌아가서 차가 안 움직임)
    // LSD(Limited Slip Differential): 위의 상황에서 제한을 걸어서 반대쪽도 어느정도 돌려줌
    // 용접 데후(LOCKED_4WD): 그냥 양쪽이 똑같이 돔(드리프트)
    PxVehicleDifferential4WData DiffData;
    DiffData.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD; // 4륜 구동
    DriveSimData.setDiffData(DiffData);

    // 클러치 기본값 씀
    PxVehicleClutchData ClutchData;
    DriveSimData.setClutchData(ClutchData);

    PxVehicleAckermannGeometryData Ackermann;
    Ackermann.mAccuracy = 1.0f; // 정밀도 (기본 1.0)

    Ackermann.mFrontWidth = FMath::Abs(WheelOffset1.Y - WheelOffset0.Y);


    Ackermann.mRearWidth = FMath::Abs(WheelOffset3.Y - WheelOffset2.Y);

    Ackermann.mAxleSeparation = FMath::Abs(WheelOffset0.X - WheelOffset2.X);


    if (Ackermann.mFrontWidth < 0.1f) Ackermann.mFrontWidth = 100.0f;
    if (Ackermann.mRearWidth < 0.1f)  Ackermann.mRearWidth = 100.0f;
    if (Ackermann.mAxleSeparation < 0.1f) Ackermann.mAxleSeparation = 200.0f;

  
    DriveSimData.setAckermannGeometryData(Ackermann);
}
