#include "pch.h"
#include "CurtainActor.h"
#include "ClothComponent.h"

IMPLEMENT_CLASS(ACurtainActor)

ACurtainActor::ACurtainActor()
{
    // ClothComponent 생성 (시뮬레이션 + 렌더링 모두 담당)
    ClothComp = CreateDefaultSubobject<UClothComponent>("ClothComponent");
    SetRootComponent(ClothComp);

    bTickInEditor = false;
}

ACurtainActor::~ACurtainActor()
{
}

void ACurtainActor::BeginPlay()
{
    Super::BeginPlay();

    if (!ClothComp)
    {
        UE_LOG("CurtainActor::BeginPlay - ERROR: ClothComp is null!");
        return;
    }
}
