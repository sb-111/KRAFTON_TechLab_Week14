#include "pch.h"
#include "CurtainActor.h"
#include "ClothComponent.h"

IMPLEMENT_CLASS(ACurtainActor)

ACurtainActor::ACurtainActor()
{
    // ClothComponent 생성 (시뮬레이션 + 렌더링 모두 담당)
    ClothComp = CreateDefaultSubobject<UClothComponent>("ClothComponent");
    SetRootComponent(ClothComp);

    bCanEverTick = false;  // ClothComponent가 Tick 처리
}

ACurtainActor::~ACurtainActor()
{
}

void ACurtainActor::BeginPlay()
{
    Super::BeginPlay();

    if (!ClothComp)
    {
        return;
    }

    // Cloth 메시 생성 및 초기화
    ClothComp->CreateClothFromPlane(GridSizeX, GridSizeY, QuadSize);
    ClothComp->InitializeCloth();
}
