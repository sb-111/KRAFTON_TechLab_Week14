#pragma once

#include "BodySetupCore.h"
#include "AggregateGeometry.h"
#include "PhysicalMaterial.h"
#include "UBodySetup.generated.h"

// ===== Body Setup =====
// Physics body configuration for a single bone
// Inherits BoneName from UBodySetupCore
UCLASS(DisplayName="Body Setup", Description = "하나의 본에 대한 물리 설정")
class UBodySetup : public UBodySetupCore
{
    GENERATED_REFLECTION_BODY()
public:
    // Collision geometry (Primitives)
    UPROPERTY(EditAnywhere, Category="Primitives")
    FKAggregateGeom AggGeom;

    // Mass in kg
    UPROPERTY(EditAnywhere, Category="Physics")
    float MassInKg = 1.0f;

    // Linear damping
    UPROPERTY(EditAnywhere, Category="Physics")
    float LinearDamping = 0.01f;

    // Angular damping
    UPROPERTY(EditAnywhere, Category="Physics")
    float AngularDamping = 0.05f;

    // Physical Material (Friction, Restitution)
    UPROPERTY(EditAnywhere, Category="Physical Material")
    UPhysicalMaterial* PhysMaterial = nullptr;

    // Physical Material 에셋 경로 (드롭다운 선택 시 저장)
    FString PhysMaterialPath;

    UBodySetup() = default;
    virtual ~UBodySetup() = default;

};
