#pragma once

#include "Vector.h"
#include "FKConvexElem.generated.h"

// ===== Convex Hull Collision Element =====
USTRUCT(DisplayName="Convex Element", Description = "컨벡스 충돌체")
struct FKConvexElem
{
    GENERATED_REFLECTION_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Convex")
    FName Name;

    UPROPERTY(EditAnywhere, Category="Convex")
    TArray<FVector> VertexData;

    TArray<int32> IndexData;  // Triangle indices

    FKConvexElem() = default;
};
