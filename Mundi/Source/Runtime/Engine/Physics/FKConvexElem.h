#pragma once

#include "Vector.h"
#include "PhysxConverter.h"
#include "FKConvexElem.generated.h"

// ===== Convex Hull Collision Element =====
USTRUCT(DisplayName="Convex Element", Description = "컨벡스 충돌체")
struct FKConvexElem
{
    GENERATED_REFLECTION_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Convex")
    FName Name;

    /** Transform of this convex element (Translation, Rotation, Scale) */
    FTransform Transform;

    UPROPERTY(EditAnywhere, Category="Convex")
    TArray<FVector> VertexData;

    TArray<int32> IndexData;  // Triangle indices

    UPROPERTY(EditAnywhere, Category="Collision")
    ECollisionEnabled CollisionEnabled = ECollisionEnabled::PhysicsAndQuery;

    FKConvexElem() = default;

    /** Get current transform */
    const FTransform& GetTransform() const { return Transform; }

    /** Set transform */
    void SetTransform(const FTransform& InTransform) { Transform = InTransform; }
};
