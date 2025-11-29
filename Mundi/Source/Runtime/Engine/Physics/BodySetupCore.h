#pragma once

#include "Object.h"
#include "UBodySetupCore.generated.h"

// ===== Body Setup Core =====
// 물리 바디의 기본 정보 (본 이름만 포함)
// UBodySetup의 부모 클래스
UCLASS(DisplayName = "Body Setup Core", Description = "물리 바디 기본 클래스")
class UBodySetupCore : public UObject
{
    GENERATED_REFLECTION_BODY()
public:
    // Bone name this body is attached to
    UPROPERTY(EditAnywhere, Category = "Bone")
    FName BoneName;

    UBodySetupCore() = default;
    virtual ~UBodySetupCore() = default;
};
