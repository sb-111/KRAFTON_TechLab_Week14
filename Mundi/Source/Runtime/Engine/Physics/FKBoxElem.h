#pragma once

#include "Vector.h"
#include "PhysxConverter.h"
#include "FKBoxElem.generated.h"

// ===== Box Collision Element =====
USTRUCT(DisplayName="Box Element", Description = "박스 충돌체")
struct FKBoxElem
{
    GENERATED_REFLECTION_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Box")
    FName Name;

    UPROPERTY(EditAnywhere, Category="Box")
    FVector Center = FVector::Zero();

    UPROPERTY(EditAnywhere, Category="Box")
    FVector Rotation = FVector::Zero();  // Euler angles (Roll, Pitch, Yaw) in degrees

    UPROPERTY(EditAnywhere, Category="Box")
    float X = 1.0f;  // Half extent X

    UPROPERTY(EditAnywhere, Category="Box")
    float Y = 1.0f;  // Half extent Y

    UPROPERTY(EditAnywhere, Category="Box")
    float Z = 1.0f;  // Half extent Z

    UPROPERTY(EditAnywhere, Category="Collision")
    ECollisionEnabled CollisionEnabled = ECollisionEnabled::PhysicsAndQuery;

    FKBoxElem() = default;
    FKBoxElem(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}
};
