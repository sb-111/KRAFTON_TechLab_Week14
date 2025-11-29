#pragma once

#include "Vector.h"
#include "FKSphereElem.generated.h"

// ===== Sphere Collision Element =====
USTRUCT(DisplayName="Sphere Element", Description = "구 충돌체")
struct FKSphereElem
{
    GENERATED_REFLECTION_BODY()
public:
    // 에디터에서 이름 지을 때 필요
    // ex) Head_Collision
    UPROPERTY(EditAnywhere, Category = "Sphere")
    FName Name;

    UPROPERTY(EditAnywhere, Category="Sphere")
    FVector Center = FVector::Zero();

    UPROPERTY(EditAnywhere, Category="Sphere")
    float Radius = 1.0f;

    FKSphereElem() = default;
    FKSphereElem(float InRadius) : Radius(InRadius) {}
    FKSphereElem(const FVector& InCenter, float InRadius) : Center(InCenter), Radius(InRadius) {}
};
