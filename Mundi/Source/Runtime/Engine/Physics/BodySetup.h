#pragma once

#include "BodySetupCore.h"
#include "AggregateGeometry.h"
#include "UBodySetup.generated.h"

// Physics body type
UENUM(DisplayName = "물리 바디 타입")
enum class EPhysicsType : uint8
{
    Default,      // Use parent setting
    Kinematic,    // Animation driven (before Ragdoll)
    Simulated     // Physics simulation
};

// Collision enabled type
UENUM(DisplayName = "충돌 허용 타입")
enum class ECollisionEnabled : uint8
{
    NoCollision,
    QueryOnly,        // Raycast, Overlap only
    PhysicsOnly,      // Physics only
    QueryAndPhysics   // Both
};

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

    // Physics type
    UPROPERTY(EditAnywhere, Category="Physics")
    EPhysicsType PhysicsType = EPhysicsType::Default;

    // Collision enabled
    UPROPERTY(EditAnywhere, Category="Physics")
    ECollisionEnabled CollisionEnabled = ECollisionEnabled::QueryAndPhysics;

    // Mass in kg
    UPROPERTY(EditAnywhere, Category="Physics")
    float MassInKg = 1.0f;

    // Linear damping
    UPROPERTY(EditAnywhere, Category="Physics")
    float LinearDamping = 0.01f;

    // Angular damping
    UPROPERTY(EditAnywhere, Category="Physics")
    float AngularDamping = 0.05f;

    // Friction (0.0 = ice, 1.0 = rubber)
    UPROPERTY(EditAnywhere, Category="Physics Material")
    float Friction = 0.5f;

    // Restitution / Bounciness (0.0 = clay, 1.0 = bouncy ball)
    UPROPERTY(EditAnywhere, Category="Physics Material")
    float Restitution = 0.1f;

    UBodySetup() = default;
    virtual ~UBodySetup() = default;
};
