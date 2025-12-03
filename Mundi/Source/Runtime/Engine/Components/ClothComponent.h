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

    // ========== 메시 생성 파라미터 ==========
    UPROPERTY(EditAnywhere, Category="Cloth|Mesh Generation", ToolTip="천의 가로 방향 쿼드 개수 (정점 개수는 +1)\n높을수록 부드럽지만 성능 저하\n권장: 100~300")
    int MeshGridSizeX = 20;

    UPROPERTY(EditAnywhere, Category="Cloth|Mesh Generation", ToolTip="천의 세로 방향 쿼드 개수 (정점 개수는 +1)\n높을수록 부드럽지만 성능 저하\n권장: 100~300")
    int MeshGridSizeY = 20;

    UPROPERTY(EditAnywhere, Category="Cloth|Mesh Generation", ToolTip="각 쿼드의 크기 (cm 단위)\n작을수록 정밀하지만 정점 수 동일\n권장: 0.1~1.0")
    float MeshQuadSize = 1.f;

    // ========== 기본 물리 파라미터 ==========
    UPROPERTY(EditAnywhere, Category="Cloth|Basic Physics", ToolTip="중력 가속도 (cm/s²)\nZ-up 좌표계에서 Z가 음수면 아래로 떨어짐\n기본값: (0, 0, -0.98) = 지구 중력의 1/100")
    FVector Gravity = FVector(0, 0, -980.0f);

    UPROPERTY(EditAnywhere, Category="Cloth|Basic Physics", ToolTip="속도 감쇠 계수 (0~1)\n높을수록 천의 진동이 빠르게 멈춤\n너무 높으면 부자연스러움\n권장: 0.3~0.6")
    float Damping = 0.6f;  // 0.5 → 0.6 (안정성 향상)

    UPROPERTY(EditAnywhere, Category="Cloth|Basic Physics", ToolTip="마찰 계수 (0~1)\n천이 접힐 때 마찰력\n높을수록 부드럽게 접힘\n권장: 0.2~0.5")
    float Friction = 0.4f;  // 0.3 → 0.4 (더 부드러운 접힘)

    // ========== 제약 조건 강성도 ==========
    UPROPERTY(EditAnywhere, Category="Cloth|Constraints|Stiffness", ToolTip="기본 제약 조건 강성도 (0~1)\n높을수록 단단하고 덜 늘어남\n낮을수록 부드럽고 잘 늘어남\n권장: 0.5~0.7")
    float Stiffness = 0.5f;  // 0.6 → 0.5 (더 부드럽게)

    UPROPERTY(EditAnywhere, Category="Cloth|Constraints|Stiffness", ToolTip="대각선 제약의 강성도 배율 (0~1)\n기본 Stiffness에 곱해짐\n낮을수록 더 유연한 대각선 변형\n권장: 0.8~0.95")
    float DiagonalStiffnessMultiplier = 0.85f;  // 0.9 → 0.85 (더 유연하게)

    UPROPERTY(EditAnywhere, Category="Cloth|Constraints|Stiffness", ToolTip="2칸 떨어진 정점의 굽힘 제약 배율 (0~1)\n낮을수록 부드럽게 구부러짐\n권장: 0.1~0.3")
    float Bending2HopMultiplier = 0.15f;  // 0.2 → 0.15 (더 부드럽게 굽힘)

    UPROPERTY(EditAnywhere, Category="Cloth|Constraints|Stiffness", ToolTip="3칸 떨어진 정점의 굽힘 제약 배율 (0~1)\n낮을수록 더욱 부드러운 곡선 형성\n권장: 0.05~0.15")
    float Bending3HopMultiplier = 0.08f;  // 0.1 → 0.08 (더욱 부드러운 곡선)

    // ========== Phase Config 설정 ==========
    UPROPERTY(EditAnywhere, Category="Cloth|Constraints|Phase Config", ToolTip="제약 조건 강성도 배율 (0~2)\n1.0보다 높으면 더 강하게, 낮으면 더 약하게\n권장: 0.8~1.2")
    float PhaseStiffnessMultiplier = 0.9f;  // 1.0 → 0.9 (더 부드럽게)

    UPROPERTY(EditAnywhere, Category="Cloth|Constraints|Phase Config", ToolTip="압축 제한 (0~1)\n1.0 = 압축 없음, 0.9 = 10%까지 압축 허용\n천은 보통 압축되지 않으므로 1.0 권장")
    float CompressionLimit = 1.0f;

    UPROPERTY(EditAnywhere, Category="Cloth|Constraints|Phase Config", ToolTip="늘어남 제한 (1.0~2.0)\n1.0 = 늘어남 없음, 1.1 = 10%까지 늘어남 허용\n높을수록 부드럽지만 많이 늘어남\n권장: 1.05~1.15")
    float StretchLimit = 1.12f;  // 1.08 → 1.12 (더 많이 늘어남 허용, 부드러운 변형)

    // ========== Solver 설정 ==========
    UPROPERTY(EditAnywhere, Category="Cloth|Solver", ToolTip="Solver 주파수 (Hz)\n한 프레임당 제약 조건을 몇 번 해결할지\n높을수록 정확하지만 뾰족하게 접힘\n낮을수록 부드럽지만 불안정할 수 있음\n권장: 60~120")
    float SolverFrequency = 45.0f;  // 60 → 45 (매우 부드러운 굽힘)

    // ========== Tether 제약 조건 ==========
    UPROPERTY(EditAnywhere, Category="Cloth|Tether Constraints", ToolTip="Tether 길이 배율 (0.5~2.0)\nFabric에 정의된 Tether 길이에 곱해짐\n1.0 = 원래 길이 그대로\n권장: 0.9~1.1")
    float TetherConstraintScale = 1.0f;

    UPROPERTY(EditAnywhere, Category="Cloth|Tether Constraints", ToolTip="Tether 제약 강성도 (0~1)\n1.0 = 강하게 제한 (날아가지 않음)\n0.5 = 약하게 제한 (약간 늘어남)\n권장: 0.8~1.0")
    float TetherConstraintStiffness = 0.9f;  // 1.0 → 0.9 (약간 더 유연하게)

    UPROPERTY(EditAnywhere, Category="Cloth|Tether Constraints", ToolTip="Tether 최대 거리 배율 (1.0~1.5)\n초기 거리의 몇 배까지 허용할지\n1.05 = 5%까지 늘어남\n낮을수록 안정적이지만 딱딱함\n권장: 1.03~1.08")
    float TetherMaxDistanceMultiplier = 1.08f;  // 1.05 → 1.08 (더 늘어남 허용)

    // ========== 바람 시뮬레이션 ==========
    UPROPERTY(EditAnywhere, Category="Cloth|Wind", ToolTip="바람 효과 활성화")
    bool bEnableWind = true;

    UPROPERTY(EditAnywhere, Category="Cloth|Wind", ToolTip="바람 세기 (cm/s)\n높을수록 강한 바람\n너무 높으면 정점이 날아갈 수 있음\n권장: 10~30")
    float WindStrength = 15.0f;

    UPROPERTY(EditAnywhere, Category="Cloth|Wind", ToolTip="바람 방향 벡터\n정규화되어 사용됨\n예: (1,0,0) = X축 방향")
    FVector WindDirection = FVector(1.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, Category="Cloth|Wind", ToolTip="바람 난류 강도 (0~1)\n바람의 불규칙성\n높을수록 변화가 심함\n권장: 0.3~0.7")
    float WindTurbulence = 0.5f;

    UPROPERTY(EditAnywhere, Category="Cloth|Wind", ToolTip="바람 주파수 (Hz)\n바람이 변화하는 속도\n높을수록 빠르게 변함\n권장: 0.5~2.0")
    float WindFrequency = 1.0f;

    UPROPERTY(EditAnywhere, Category="Cloth|Wind", ToolTip="공기 저항 계수 (0~1)\nDrag Force: Fd = ½ρv²cdragA\n높을수록 바람의 영향을 많이 받음\n권장: 0.1~0.4")
    float DragCoefficient = 0.2f;

    UPROPERTY(EditAnywhere, Category="Cloth|Wind", ToolTip="양력 계수 (0~1)\nLift Force: Fl = clift½ρv²A\n높을수록 바람에 들려 올라감\n권장: 0.2~0.5")
    float LiftCoefficient = 0.3f;

    UPROPERTY(EditAnywhere, Category="Cloth|Wind", ToolTip="유체 밀도 (0~2)\n바람의 힘은 이 값에 비례\n높을수록 강한 바람 효과\n권장: 0.3~0.8")
    float FluidDensity = 0.5f;

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