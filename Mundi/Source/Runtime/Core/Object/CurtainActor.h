#pragma once
#include "Actor.h"
#include "ACurtainActor.generated.h"

class UClothComponent;
class UDynamicMesh;
struct FMeshData;

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
    void Tick(float DeltaSeconds) override;

    // ===== Components =====
    UClothComponent* ClothComp = nullptr;
    UDynamicMesh* DynamicMesh = nullptr;

    // ===== Cloth Settings =====
    UPROPERTY(EditAnywhere, Category="Cloth Settings")
    int GridSizeX = 15;

    UPROPERTY(EditAnywhere, Category="Cloth Settings")
    int GridSizeY = 20;

    UPROPERTY(EditAnywhere, Category="Cloth Settings")
    float QuadSize = 10.0f;  // cm

protected:
    FMeshData* MeshData = nullptr;
    void UpdateMeshFromCloth();
};
