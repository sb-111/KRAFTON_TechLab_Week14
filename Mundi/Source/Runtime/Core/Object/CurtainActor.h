#pragma once
#include "Actor.h"
#include "ACurtainActor.generated.h"

class UClothComponent;

UCLASS(DisplayName="Curtain Actor", Description="천 시뮬레이션 테스트 액터 (커튼)")
class ACurtainActor : public AActor
{
public:
    GENERATED_REFLECTION_BODY()

    ACurtainActor();

protected:
    ~ACurtainActor() override;

public:
    void BeginPlay() override;

    // ===== Components =====
    UClothComponent* ClothComp = nullptr;
};
