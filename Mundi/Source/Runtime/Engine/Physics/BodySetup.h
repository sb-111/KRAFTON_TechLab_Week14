#pragma once

#include "Object.h"
#include "AggregateGeometry.h"
#include "UBodySetup.generated.h"

// ===== Body Setup =====
// Physics body configuration for a single bone
UCLASS(DisplayName="Body Setup", Description = "하나의 본에 대한 설정")
class UBodySetup : public UObject
{
    GENERATED_REFLECTION_BODY()
public:

    // Bone name this body is attached to
    UPROPERTY(EditAnywhere, Category="Bone")
    FName BoneName;

    // Collision geometry
    UPROPERTY(EditAnywhere, Category="Collision")
    FKAggregateGeom AggGeom;

    // Physics type
    UPROPERTY(EditAnywhere, Category="Physics")
    bool bSimulatePhysics = false;

    // Collision enabled
    UPROPERTY(EditAnywhere, Category="Physics")
    ECollisionEnabled CollisionEnabled = ECollisionEnabled::PhysicsAndQuery;

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
