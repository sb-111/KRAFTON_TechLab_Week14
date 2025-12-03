#pragma once
#include "PrimitiveComponent.h"
#include "Vector.h"
#include "UClothComponent.generated.h"

// ========================================================================================================
// UClothComponent - NvCloth 기반 천(Cloth) 시뮬레이션 및 렌더링 컴포넌트
// ========================================================================================================
//
// === NvCloth Simulation Flow ===
//
// 1. [초기화] UClothComponent 생성
//    - Constructor에서 DynamicMesh, MeshData 생성
//
// 2. [메시 생성] ACurtainActor::BeginPlay()
//    - CreateClothFromPlane(GridX, GridY, QuadSize) 호출
//    - 정점 위치, InverseMass, 삼각형 인덱스 생성
//
// 3. [NvCloth 초기화] InitializeCloth()
//    - Fabric 생성: 제약 조건(Distance Constraints) 정의
//      * Factory::createFabric(numParticles, phases, sets, restLengths, stiffness, indices, ...)
//    - Cloth 인스턴스 생성: 실제 정점 위치/속도 등 상태 포함
//      * Factory::createCloth(particleRange, fabric)
//    - Solver에 추가: 시뮬레이션 대상으로 등록
//      * Solver::addCloth(cloth)
//    - 시뮬레이션 파라미터 적용: 중력, 댐핑, 공기저항 등
//      * Cloth::setGravity(), setDamping(), setDragCoefficient(), setLiftCoefficient()
//
// 4. [시뮬레이션 루프] 매 프레임
//    a. World::Tick() -> ClothSystem::Update(DeltaTime)
//       * Solver::beginSimulation(dt)
//       * Solver::simulateChunk(i) for each chunk (병렬 처리 가능)
//       * Solver::endSimulation()
//       -> 모든 Cloth의 정점 위치가 업데이트됨
//
//    b. UClothComponent::TickComponent()
//       * UpdateSimulationResult(): Cloth::getCurrentParticles()로 정점 위치 가져오기
//       * UpdateDynamicMesh(): MeshData에 정점/노말 복사 -> GPU 버퍼 업데이트
//
// 5. [렌더링] FSceneRenderer::RenderClothPass()
//    - ClothComponent 수집 (GatherVisibleProxies)
//    - CollectMeshBatches(): FMeshBatchElement 생성 (VertexBuffer, IndexBuffer, Shader 등)
//    - DrawMeshBatches(): GPU에 그리기 명령 전송
//
// 6. [정리] 컴포넌트 파괴 시
//    - ReleaseCloth(): Solver::removeCloth(), Fabric::decRefCount(), delete Cloth
//
// === 주요 NvCloth API 함수 ===
//
// - InitializeNvCloth(): 라이브러리 전역 초기화 (1회만 호출)
// - NvClothCreateFactoryCPU(): CPU Factory 생성
// - Factory::createFabric(): Fabric(제약 구조) 생성
// - Factory::createCloth(): Cloth 인스턴스 생성
// - Factory::createSolver(): Solver(시뮬레이션 엔진) 생성
// - Solver::addCloth(): Cloth를 시뮬레이션 대상으로 등록
// - Solver::removeCloth(): Cloth를 시뮬레이션 대상에서 제거
// - Solver::beginSimulation(): 시뮬레이션 시작
// - Solver::simulateChunk(): 특정 Chunk 시뮬레이션 (병렬 처리 가능)
// - Solver::endSimulation(): 시뮬레이션 종료 및 결과 적용
// - Cloth::getCurrentParticles(): 시뮬레이션 결과(정점 위치) 가져오기 (읽기 전용)
// - Cloth::setGravity(): 중력 설정
// - Cloth::setDamping(): 댐핑(감쇠) 설정
// - Cloth::setDragCoefficient(): 공기 저항 계수 설정
// - Cloth::setLiftCoefficient(): 양력 계수 설정
// - Fabric::decRefCount(): Fabric 참조 카운트 감소 (0이 되면 자동 삭제)
//
// ========================================================================================================

class UDynamicMesh;
struct FMeshData;

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

UCLASS(DisplayName="Cloth Component", Description="NvCloth 기반 천 시뮬레이션 및 렌더링 컴포넌트")
class UClothComponent : public UPrimitiveComponent
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
    FVector Gravity = FVector(0, 0, -0.98f);  // cm/s^2 (지구 중력의 1/10)

    UPROPERTY(EditAnywhere, Category="Cloth Simulation")
    float Damping = 0.4f;  // 천의 진동 감쇠 (높을수록 빨리 안정화)

    UPROPERTY(EditAnywhere, Category="Cloth Simulation")
    float Stiffness = 0.9f;  // 천의 강성도 (0~1, 높을수록 단단하고 덜 늘어남)

    UPROPERTY(EditAnywhere, Category="Cloth Simulation")
    float DragCoefficient = 0.3f;  // 공기 저항 계수 (0~1, 높을수록 바람 영향 큼)

    UPROPERTY(EditAnywhere, Category="Cloth Simulation")
    float LiftCoefficient = 0.4f;  // 양력 계수 (0~1, 높을수록 바람에 들려 올라감)

    // ===== Wind Parameters =====
    UPROPERTY(EditAnywhere, Category="Cloth Simulation|Wind")
    bool bEnableWind = true;  // 바람 효과 활성화

    UPROPERTY(EditAnywhere, Category="Cloth Simulation|Wind")
    float WindStrength = 50.0f;  // 바람 세기 (cm/s)

    UPROPERTY(EditAnywhere, Category="Cloth Simulation|Wind")
    FVector WindDirection = FVector(1.0f, 0.0f, 0.0f);  // 바람 방향

    UPROPERTY(EditAnywhere, Category="Cloth Simulation|Wind")
    float WindTurbulence = 0.5f;  // 바람 난류 (0~1)

    UPROPERTY(EditAnywhere, Category="Cloth Simulation|Wind")
    float WindFrequency = 1.0f;  // 바람 주파수 (Hz)

    // ===== Visual Parameters =====
    UPROPERTY(EditAnywhere, Category="Cloth Appearance")
    FVector ClothColor = FVector(1.0f, 1.0f, 1.0f);  // 천 색상 (R, G, B)

    // ===== Data Access =====

    const TArray<FVector>& GetClothVertices() const { return ClothVertices; }
    const TArray<uint32_t>& GetClothIndices() const { return ClothIndices; }

    // ===== Rendering =====
    UDynamicMesh* GetDynamicMesh() const { return DynamicMesh; }
    void UpdateDynamicMesh();

    // ===== Batch Rendering =====
    void CollectMeshBatches(TArray<struct FMeshBatchElement>& OutMeshBatchElements, const struct FSceneView* View) override;

    // ===== Duplication =====
    void DuplicateSubObjects() override;

protected:
    // ===== Cloth Instance =====
    nv::cloth::Cloth* ClothInstance = nullptr;
    nv::cloth::Fabric* ClothFabric = nullptr;

    // ===== Mesh Data =====
    TArray<FVector> ClothVertices;      // 현재 정점 위치
    TArray<FVector> InitialLocalPositions;  // 초기 로컬 공간 위치 (Transform 추적용)
    TArray<float> InverseMasses;        // 정점별 역질량 (0 = 고정)
    TArray<uint32_t> ClothIndices;      // 삼각형 인덱스
    int ClothGridSizeX = 0;             // 천 그리드 가로 크기
    int ClothGridSizeY = 0;             // 천 그리드 세로 크기
    float ClothQuadSize = 10.0f;        // 각 쿼드의 크기 (cm)

    // ===== Transform Tracking =====
    FTransform PreviousWorldTransform;  // 이전 프레임의 Transform (변화 감지용)

    // ===== Rendering Resources =====
    UDynamicMesh* DynamicMesh = nullptr;
    FMeshData* MeshData = nullptr;
    class UMaterial* ClothMaterial = nullptr;  // Cloth 전용 Material (Vertex Color 사용)
    ID3D11ShaderResourceView* WhiteTextureSRV = nullptr;  // 1x1 흰색 텍스처 (Vertex Color 유지용)

    // ===== Helper Functions =====
    void UpdateSimulationResult();      // Cloth에서 정점 데이터 가져오기
    void ApplySimulationParameters();   // 파라미터를 Cloth에 적용
    void UpdateWind(float DeltaTime);   // 바람 효과 업데이트

    // ===== Wind State =====
    float WindTime = 0.0f;  // 바람 시간 누적
};