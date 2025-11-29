#pragma once

#include "CollisionShapes.h"
#include "FKAggregateGeom.generated.h"

// ===== Aggregate Geometry =====
// Container for multiple collision shapes attached to a single body
// 모양들의 집합소 (Container)
// 왜 필요할까?
// 사람의 몸통을 생각하면 단순한 하나의 박스로 표현이 안됨.
// 가슴쪽은 큰 박스, 배쪽은 약간 작은 박스 2개를 겹쳐 표현해야 할 수 있음.
USTRUCT(DisplayName="Aggregate Geometry", Description = "하나의 뼈(Bone)에 여러개의 Shape를 붙일 수 있게 해줌")
struct FKAggregateGeom
{
    GENERATED_REFLECTION_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Primitives")
    TArray<FKSphereElem> SphereElems;

    UPROPERTY(EditAnywhere, Category="Primitives")
    TArray<FKBoxElem> BoxElems;

    UPROPERTY(EditAnywhere, Category="Primitives")
    TArray<FKSphylElem> SphylElems;

    UPROPERTY(EditAnywhere, Category="Primitives")
    TArray<FKConvexElem> ConvexElems;

    FKAggregateGeom() = default;

    int32 GetElementCount() const
    {
        return SphereElems.Num() + BoxElems.Num() + SphylElems.Num() + ConvexElems.Num();
    }

    void EmptyElements()
    {
        SphereElems.Empty();
        BoxElems.Empty();
        SphylElems.Empty();
        ConvexElems.Empty();
    }
};
