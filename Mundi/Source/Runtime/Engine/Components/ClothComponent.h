#pragma once
#include "ActorComponent.h"
#include "Vector.h"
#include "UClothComponent.generated.h"

namespace nv
{
    namespace cloth
    {
        class Cloth;
        class Fabric;
    }
}

namespace physx
{
    class PxVec4;
    class PxVec3;
}

UCLASS(DisplayName="Cloth Component", Description="NvCloth 기반 천 시뮬레이션 컴포넌트")
class UClothComponent : public UActorComponent
{
public:
    GENERATED_REFLECTION_BODY()

    UClothComponent();

protected:
    ~UClothComponent() override;

public:
    // ===== Lifecycle =====
    void InitializeComponent() override;
    void BeginPlay() override;
    void TickComponent(float DeltaTime) override;
    void EndPlay() override;

    void OnRegister(UWorld* InWorld) override;
    void OnUnregister() override;

    // ===== Cloth Setup =====
    void CreateClothFromPlane(int GridSizeX, int GridSizeY, float QuadSize);
    void InitializeCloth();
    void ReleaseCloth();

    // ===== Simulation Parameters =====
    UPROPERTY(EditAnywhere, Category="Cloth Simulation")
    FVector Gravity = FVector(0, 0, -980.0f);   // cm/s^2

    UPROPERTY(EditAnywhere, Category="Cloth Simulation")
    float Damping = 0.3f;

    UPROPERTY(EditAnywhere, Category="Cloth Simulation")
    float Stiffness = 1.0f;

    // ===== Data Access =====

    const TArray<FVector>& GetClothVertices() const { return ClothVertices; }
    const TArray<uint32_t>& GetClothIndices() const { return ClothIndices; }

protected:
    // ===== Cloth Instance =====
    nv::cloth::Cloth* ClothInstance = nullptr;
    nv::cloth::Fabric* ClothFabric = nullptr;

    // ===== Mesh Data =====
    TArray<FVector> ClothVertices;      // 현재 정점 위치
    TArray<float> InverseMasses;        // 정점별 역질량 (0 = 고정)
    TArray<uint32_t> ClothIndices;      // 삼각형 인덱스

    // ===== Helper Functions =====
    void UpdateSimulationResult();      // Cloth에서 정점 데이터 가져오기
    void ApplySimulationParameters();   // 파라미터를 Cloth에 적용
};