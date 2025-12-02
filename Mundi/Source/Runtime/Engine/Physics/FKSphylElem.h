#pragma once

#include "Vector.h"
#include "PhysxConverter.h"
#include "FKSphylElem.generated.h"

// ===== Capsule (Sphyl) Collision Element =====
USTRUCT(DisplayName="Capsule Element", Description = "캡슐 충돌체")
struct FKSphylElem
{
    GENERATED_REFLECTION_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Capsule")
    FName Name;

    UPROPERTY(EditAnywhere, Category="Capsule")
    FVector Center = FVector::Zero();

    UPROPERTY(EditAnywhere, Category="Capsule")
    FVector Rotation = FVector::Zero();  // Euler angles in degrees

    UPROPERTY(EditAnywhere, Category="Capsule")
    float Radius = 1.0f;

    UPROPERTY(EditAnywhere, Category="Capsule")
    float Length = 1.0f;  // Total cylinder length

    UPROPERTY(EditAnywhere, Category="Collision")
    ECollisionEnabled CollisionEnabled = ECollisionEnabled::PhysicsAndQuery;

    FKSphylElem() = default;
    FKSphylElem(float InRadius, float InLength) : Radius(InRadius), Length(InLength) {}
};
