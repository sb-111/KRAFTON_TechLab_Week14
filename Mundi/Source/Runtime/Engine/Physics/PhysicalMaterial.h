#pragma once

#include "Object.h"
#include "UPhysicalMaterial.generated.h"

// ===== Physical Material =====
// 물리 표면 속성을 정의하는 클래스
// UBodySetup에서 참조하여 마찰 및 반발 계수를 설정
UCLASS(DisplayName="Physical Material", Description = "물리 표면 속성 (마찰, 반발 계수)")
class UPhysicalMaterial : public UObject
{
    GENERATED_REFLECTION_BODY()
public:
    // Friction coefficient (0.0 = ice, 1.0 = rubber)
    UPROPERTY(EditAnywhere, Category="Physical Material")
    float Friction = 0.5f;

    // Restitution / Bounciness (0.0 = clay, 1.0 = bouncy ball)
    UPROPERTY(EditAnywhere, Category="Physical Material")
    float Restitution = 0.3f;

    UPhysicalMaterial() = default;
    virtual ~UPhysicalMaterial() = default;
};
