#include "pch.h"
#include "ClothComponent.h"
#include "ClothSystem.h"
#include "DynamicMesh.h"
#include "VertexData.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "Material.h"
#include "Shader.h"
#include "SceneView.h"
#include "MeshBatchElement.h"
#include <NvCloth/Factory.h>
#include <NvCloth/Cloth.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Solver.h>
#include <foundation/PxVec4.h>
#include <foundation/PxVec3.h>
#include <cmath>

IMPLEMENT_CLASS(UClothComponent)

UClothComponent::UClothComponent()
{
    bCanEverTick = true;
    bTickEnabled = true;
    bTickInEditor = true;  // 에디터에서도 시뮬레이션 보기

    // 렌더링 설정
    SetVisibility(true);

    // 렌더링 리소스 생성
    DynamicMesh = NewObject<UDynamicMesh>();
    MeshData = new FMeshData();

    CreateClothFromPlane(15, 20, 1);
    InitializeCloth();
}

UClothComponent::~UClothComponent()
{
    ReleaseCloth();

    if (MeshData)
    {
        delete MeshData;
        MeshData = nullptr;
    }
}

void UClothComponent::InitializeComponent()
{
    Super::InitializeComponent();
}

void UClothComponent::BeginPlay()
{
    UE_LOG("[ClothComponent::BeginPlay] Called");
    Super::BeginPlay();
    UE_LOG("[ClothComponent::BeginPlay] Complete");
}

void UClothComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    static int tickCount = 0;
    if (tickCount++ % 120 == 0)
    {
        UE_LOG("[ClothComponent::TickComponent] Tick %d: ClothInstance=%p", tickCount, ClothInstance);
    }

    if (ClothInstance)
    {
        UpdateSimulationResult();
        UpdateDynamicMesh();
    }
    else
    {
        static int logCount = 0;
        if (logCount++ < 3)
        {
            UE_LOG("[ClothComponent::TickComponent] ClothInstance is null! (DeltaTime: %.4f)", DeltaTime);
        }
    }
}

void UClothComponent::EndPlay()
{
    UE_LOG("[ClothComponent::EndPlay] Called");
    Super::EndPlay();
    UE_LOG("[ClothComponent::EndPlay] Complete");
}

void UClothComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
}

void UClothComponent::OnUnregister()
{
    UE_LOG("[ClothComponent::OnUnregister] Called");
    Super::OnUnregister();
    ReleaseCloth();
    UE_LOG("[ClothComponent::OnUnregister] Complete");
}

void UClothComponent::DuplicateSubObjects()
{
    UE_LOG("[DuplicateSubObjects] START - ClothInstance=%p, ClothFabric=%p, GridSize=(%d,%d), QuadSize=%.1f",
        ClothInstance, ClothFabric, ClothGridSizeX, ClothGridSizeY, ClothQuadSize);

    Super::DuplicateSubObjects();

    // ===== MeshData 복제 =====
    MeshData = new FMeshData();

    // ===== DynamicMesh 복제 =====
    DynamicMesh = NewObject<UDynamicMesh>();

    // ===== Cloth 시뮬레이션 리소스 재생성 =====
    ClothInstance = nullptr;
    ClothFabric = nullptr;

    // 원본의 그리드 크기와 QuadSize를 사용하여 복제
    int gridX = (ClothGridSizeX > 0) ? ClothGridSizeX : 1;
    int gridY = (ClothGridSizeY > 0) ? ClothGridSizeY : 1;
    float quadSize = (ClothQuadSize > 0.0f) ? ClothQuadSize : 10.0f;

    UE_LOG("[DuplicateSubObjects] Creating cloth with GridSize=(%d,%d), QuadSize=%.1f", gridX, gridY, quadSize);
    CreateClothFromPlane(gridX, gridY, quadSize);

    UE_LOG("[DuplicateSubObjects] Calling InitializeCloth...");
    InitializeCloth();

    UE_LOG("[DuplicateSubObjects] END - ClothInstance=%p, ClothFabric=%p", ClothInstance, ClothFabric);
}

// ===== Cloth 메시 생성 =====
// @brief 평면 그리드 형태의 Cloth 메시 데이터를 생성합니다
// @param GridSizeX 가로 방향 쿼드 개수 (정점은 GridSizeX+1개)
// @param GridSizeY 세로 방향 쿼드 개수 (정점은 GridSizeY+1개)
// @param QuadSize 각 쿼드의 크기 (cm 단위)
// @note 이 함수는 단순히 CPU 측 데이터만 생성하며, InitializeCloth()를 호출해야 실제 시뮬레이션이 시작됩니다
void UClothComponent::CreateClothFromPlane(int GridSizeX, int GridSizeY, float QuadSize)
{
    UE_LOG("[CreateClothFromPlane] START - GridSize=(%d,%d), QuadSize=%.1f", GridSizeX, GridSizeY, QuadSize);

    ClothVertices.clear();
    InverseMasses.clear();
    ClothIndices.clear();

    // 그리드 크기 및 QuadSize 저장
    ClothGridSizeX = GridSizeX;
    ClothGridSizeY = GridSizeY;
    ClothQuadSize = QuadSize;

    // ===== 정점 생성 =====
    // 예: GridSize 10x10 -> 정점 11x11 = 121개
    // 각 정점은 위치(position)와 질량의 역수(inverseMass)를 가집니다
    for (int y = 0; y <= GridSizeY; y++)
    {
        for (int x = 0; x <= GridSizeX; x++)
        {
            // Z-up 왼손 좌표계: X-앞, Y-오른쪽, Z-위
            // 커튼은 YZ 평면에 위치 (X=0), 위에서 아래로 늘어뜨림
            FVector pos(0.0f, x * QuadSize, -y * QuadSize);
            ClothVertices.push_back(pos);

            // ===== InverseMass (질량의 역수) 설정 =====
            // inverseMass = 0 -> 무한대 질량 = 고정된 정점 (Fixed/Pinned)
            // inverseMass = 1 -> 질량 1.0 = 시뮬레이션되는 정점 (Dynamic)
            // 최상단 행(y==0)만 고정하여 커튼처럼 위쪽이 고정되도록 설정
            // Tether 제약이 작동하려면 고정 앵커가 필요함
            float invMass = (y == 0) ? 0.0f : 1.0f;
            InverseMasses.push_back(invMass);
        }
    }

    // ===== 삼각형 인덱스 생성 =====
    // 각 쿼드(사각형)를 2개의 삼각형으로 분할하여 렌더링 및 시뮬레이션에 사용
    // CCW(Counter-Clockwise) 순서로 인덱스를 구성하여 왼손 좌표계에서 앞면이 보이도록 설정
    for (int y = 0; y < GridSizeY; y++)
    {
        for (int x = 0; x < GridSizeX; x++)
        {
            // 한 쿼드를 구성하는 4개 정점의 인덱스
            int i0 = y * (GridSizeX + 1) + x;           // 좌상단
            int i1 = i0 + 1;                             // 우상단
            int i2 = i0 + (GridSizeX + 1);               // 좌하단
            int i3 = i2 + 1;                             // 우하단

            // 삼각형 1 (i0, i2, i1) - CCW 순서
            ClothIndices.push_back(i0);
            ClothIndices.push_back(i2);
            ClothIndices.push_back(i1);

            // 삼각형 2 (i2, i3, i1) - CCW 순서
            ClothIndices.push_back(i2);
            ClothIndices.push_back(i3);
            ClothIndices.push_back(i1);
        }
    }

    UE_LOG("[CreateClothFromPlane] END - Vertices:%d, Indices:%d", ClothVertices.size(), ClothIndices.size());
}

// @brief NvCloth를 초기화하고 시뮬레이션을 시작합니다
// @note 반드시 CreateClothFromPlane()을 먼저 호출하여 메시 데이터를 준비해야 합니다
//
// === NvCloth 초기화 프로세스 ===
// 1. Fabric 생성: 천의 구조적 특성 정의 (제약 조건, 강성도 등)
// 2. Cloth 인스턴스 생성: 실제 시뮬레이션 대상 객체 생성
// 3. Solver에 추가: 시뮬레이션 엔진에 등록
// 4. 시뮬레이션 파라미터 적용: 중력, 댐핑, 공기저항 등
// 5. 렌더링 리소스 초기화: DynamicMesh, MeshData 설정
void UClothComponent::InitializeCloth()
{
    UE_LOG("[InitializeCloth] ENTER - Vertices:%d, Indices:%d, ClothInstance=%p, ClothFabric=%p",
        ClothVertices.size(), ClothIndices.size(), ClothInstance, ClothFabric);

    if (ClothVertices.empty() || ClothIndices.empty())
    {
        UE_LOG("[InitializeCloth] EARLY RETURN - No vertices or indices!");
        return;  // CreateClothFromPlane()을 먼저 호출해야 함
    }

    UE_LOG("[InitializeCloth] Starting (V:%d, I:%d)", ClothVertices.size(), ClothIndices.size());

    ReleaseCloth();  // 기존 Cloth 정리

    UE_LOG("[InitializeCloth] After ReleaseCloth, getting Factory...");

    // ===== NvCloth Factory 가져오기 =====
    // Factory: NvCloth 객체(Fabric, Cloth, Solver)를 생성하는 팩토리 클래스
    // CPU 또는 GPU 기반 Factory가 있으며, 현재는 CPU Factory 사용
    nv::cloth::Factory* factory = FClothSystem::GetInstance().GetFactory();
    UE_LOG("[InitializeCloth] Factory=%p", factory);
    if (!factory)
    {
        UE_LOG("[InitializeCloth] ERROR: Factory is null! RETURN!");
        return;
    }

    UE_LOG("[InitializeCloth] Factory OK, creating Fabric...");

    // ===== 1. Fabric 생성 (천의 구조적 특성 정의) =====
    // Fabric: 천의 "청사진"으로, 정점 간 제약 조건(Constraints)을 정의합니다
    //   - Distance Constraints: 두 정점 사이의 거리 유지 (신축성 제어)
    //   - Bending Constraints: 굽힘 저항
    //   - Tether Constraints: 초기 위치로부터의 최대 거리 제한
    // 여러 Cloth 인스턴스가 같은 Fabric을 공유할 수 있습니다 (메모리 절약)

    TArray<uint32_t> phaseIndices;      // 각 Phase의 시작 인덱스 배열
    TArray<uint32_t> sets;              // 각 Set의 제약 개수 배열
    TArray<float> restvalues;           // 각 제약의 Rest Length (초기 거리)
    TArray<float> stiffnessValues;      // 각 제약의 Stiffness (강성도, 0~1)
    TArray<uint32_t> indices;           // 제약 대상 정점 쌍 (i0, i1, i0, i1, ...)

    // ===== Phase와 Set의 개념 =====
    // Phase: 병렬 처리 가능한 제약 그룹 (같은 Phase 내 제약들은 동시에 처리 가능)
    // Set: 각 Phase 내에서의 제약 묶음
    // phaseIndices는 각 Phase의 시작 인덱스 + 마지막 종료 인덱스를 포함해야 함
    // 예: 1개 Phase에 N개 제약이면 phaseIndices = [0, N]
    phaseIndices.push_back(0);  // Phase 0 시작 인덱스
    uint32_t constraintCount = 0;

    // ===== Distance Constraints 생성 =====
    // 수평/수직 인접 정점 간 거리 제약을 생성하여 천의 구조를 유지합니다
    int numVerticesX = ClothGridSizeX + 1;  // 정점 개수 = 그리드 크기 + 1
    int numVerticesY = ClothGridSizeY + 1;

    for (int y = 0; y < numVerticesY; y++)
    {
        for (int x = 0; x < numVerticesX; x++)
        {
            int i = y * numVerticesX + x;

            // 오른쪽 이웃 정점과 제약 생성 (수평 방향)
            if (x < numVerticesX - 1)
            {
                int neighbor = i + 1;
                indices.push_back((uint32_t)i);
                indices.push_back((uint32_t)neighbor);

                float dist = (ClothVertices[i] - ClothVertices[neighbor]).Size();
                restvalues.push_back(dist);
                stiffnessValues.push_back(Stiffness);
                constraintCount++;
            }

            // 아래쪽 이웃 정점과 제약 생성 (수직 방향)
            if (y < numVerticesY - 1)
            {
                int neighbor = i + numVerticesX;
                indices.push_back((uint32_t)i);
                indices.push_back((uint32_t)neighbor);

                float dist = (ClothVertices[i] - ClothVertices[neighbor]).Size();
                restvalues.push_back(dist);
                stiffnessValues.push_back(Stiffness);
                constraintCount++;
            }
        }
    }

    phaseIndices.push_back(constraintCount);

    sets.push_back(1);

    UE_LOG("[InitializeCloth] Constraint Summary: Total=%d, PhaseIndices=[0,%d], NumSets=1",
        constraintCount, constraintCount);

    // ===== Tether Constraints 생성 =====
    // Tether: 각 정점이 고정된 앵커로부터 너무 멀리 떨어지지 못하게 제한
    // 각 정점에서 가장 가까운 고정 앵커(최상단 행)까지의 최대 거리를 설정
    TArray<uint32_t> tetherAnchors;  // 각 정점의 가장 가까운 앵커 인덱스
    TArray<float> tetherLengths;      // 각 정점의 최대 이동 거리

    for (int y = 0; y < numVerticesY; y++)
    {
        for (int x = 0; x < numVerticesX; x++)
        {
            int i = y * numVerticesX + x;

            // 가장 가까운 고정 앵커 찾기 (최상단 행의 같은 x 좌표)
            int anchorIdx = x;  // 최상단 행 (y=0)의 같은 x 위치
            tetherAnchors.push_back((uint32_t)anchorIdx);

            // 최대 이동 거리: 초기 위치에서 앵커까지의 거리 * 1.5
            FVector pos = ClothVertices[i];
            FVector anchorPos = ClothVertices[anchorIdx];
            float initialDist = (pos - anchorPos).Size();
            float maxDist = initialDist * 1.5f;  // 초기 거리의 1.5배까지 허용
            tetherLengths.push_back(maxDist);
        }
    }

    // ===== NvCloth Range 생성 =====
    // Range: NvCloth API에서 배열을 전달하는 방식 (시작 포인터, 끝 포인터)
    nv::cloth::Range<const uint32_t> phaseRange(phaseIndices.data(), phaseIndices.data() + phaseIndices.size());
    nv::cloth::Range<const uint32_t> setsRange(sets.data(), sets.data() + sets.size());
    nv::cloth::Range<const float> restRange(restvalues.data(), restvalues.data() + restvalues.size());
    nv::cloth::Range<const float> stiffRange(stiffnessValues.data(), stiffnessValues.data() + stiffnessValues.size());
    nv::cloth::Range<const uint32_t> indicesRange(indices.data(), indices.data() + indices.size());
    nv::cloth::Range<const uint32_t> tetherAnchorsRange(tetherAnchors.data(), tetherAnchors.data() + tetherAnchors.size());
    nv::cloth::Range<const float> tetherLengthsRange(tetherLengths.data(), tetherLengths.data() + tetherLengths.size());

    // ===== Factory::createFabric() 호출 =====
    // @brief Fabric 객체를 생성합니다 (천의 구조적 특성 정의)
    // @param numParticles: 정점(파티클) 개수
    // @param phaseRange: Phase 시작 인덱스 배열
    // @param setsRange: 각 Set의 제약 개수 배열
    // @param restRange: 각 제약의 Rest Length 배열
    // @param stiffRange: 각 제약의 Stiffness 배열
    // @param indicesRange: 제약 대상 정점 쌍 배열 (i0, i1, i0, i1, ...)
    // @param anchors: Tether 앵커 인덱스 배열
    // @param tetherLengths: Tether 제약 길이 (각 정점의 최대 이동 거리)
    // @param triangles: 삼각형 인덱스 (충돌/셀프 콜리전용, 현재 미사용)
    ClothFabric = factory->createFabric(
        (uint32_t)ClothVertices.size(),
        phaseRange,
        setsRange,
        restRange,
        stiffRange,
        indicesRange,
        tetherAnchorsRange,    // Tether 앵커 인덱스
        tetherLengthsRange,    // Tether 최대 거리
        nv::cloth::Range<const uint32_t>()   // triangles (미사용)
    );

    if (!ClothFabric)
    {
        UE_LOG("ClothComponent::InitializeCloth - Failed to create Fabric");
        return;
    }

    // ===== 2. Cloth 인스턴스 생성 =====
    // Cloth: 실제 시뮬레이션 대상 객체로, Fabric의 "인스턴스"입니다
    //   - Fabric은 구조(제약)를 정의, Cloth는 실제 정점 위치/속도 등 상태를 가짐
    //   - 여러 Cloth가 같은 Fabric을 참조할 수 있음 (예: 여러 커튼이 같은 구조)

    // ===== Particle 데이터 준비 =====
    // Particle: Cloth의 각 정점을 의미 (NvCloth에서는 "Particle"로 통칭)
    // PxVec4 형식: (x, y, z, inverseMass)
    //   - x, y, z: 정점의 초기 위치
    //   - inverseMass: 질량의 역수 (0 = 고정, 1 = 동적)
    TArray<physx::PxVec4> particles;
    for (size_t i = 0; i < ClothVertices.size(); i++)
    {
        const FVector& v = ClothVertices[i];
        float invMass = InverseMasses[i];
        particles.push_back(physx::PxVec4(v.X, v.Y, v.Z, invMass));
    }

    nv::cloth::Range<const physx::PxVec4> particleRange(particles.data(), particles.data() + particles.size());

    // ===== Factory::createCloth() 호출 =====
    // @brief Cloth 인스턴스를 생성합니다
    // @param particleRange: 초기 정점 위치 및 inverseMass 배열 (PxVec4 형식)
    // @param fabric: 이 Cloth가 사용할 Fabric (구조 정의)
    // @return Cloth 객체 포인터 (실패 시 nullptr)
    ClothInstance = factory->createCloth(particleRange, *ClothFabric);
    UE_LOG("[InitializeCloth] createCloth returned: %p", ClothInstance);

    if (!ClothInstance)
    {
        UE_LOG("[InitializeCloth] ERROR: Failed to create Cloth! RETURN!");
        ClothFabric->decRefCount();  // Fabric 참조 카운트 감소
        ClothFabric = nullptr;
        return;
    }

    // ===== 3. Solver에 추가 =====
    // Solver: 물리 시뮬레이션 엔진으로, 여러 Cloth를 관리하고 업데이트합니다
    //   - 매 프레임 Solver::simulate()를 호출하면 등록된 모든 Cloth가 시뮬레이션됨
    //   - 병렬 처리, 충돌 감지 등을 담당
    nv::cloth::Solver* solver = FClothSystem::GetInstance().GetSolver();
    UE_LOG("[InitializeCloth] Solver=%p", solver);
    if (solver)
    {
        // ===== Solver::addCloth() 호출 =====
        // @brief Cloth를 Solver에 등록하여 시뮬레이션 대상으로 만듭니다
        // @param cloth: 등록할 Cloth 객체
        // @note 등록 후 Solver::simulate()가 호출될 때마다 이 Cloth가 업데이트됩니다
        solver->addCloth(ClothInstance);
        UE_LOG("[InitializeCloth] Cloth added to Solver");
    }
    else
    {
        UE_LOG("[InitializeCloth] WARNING: Solver is null! Cloth not added to solver!");
    }

    // ===== 4. 시뮬레이션 파라미터 적용 =====
    ApplySimulationParameters();

    // ===== PhaseConfig 설정 (제약 조건 해결 설정) =====
    nv::cloth::PhaseConfig phaseConfig;
    phaseConfig.mPhaseIndex = 0;
    phaseConfig.mStiffness = Stiffness;
    phaseConfig.mStiffnessMultiplier = 1.0f;
    phaseConfig.mCompressionLimit = 1.0f;  // 압축 제한 (1.0 = 제한 없음)
    phaseConfig.mStretchLimit = 1.01f;     // 늘어남 제한 (1.01 = 1%까지만 늘어남 허용)
    ClothInstance->setPhaseConfig(nv::cloth::Range<nv::cloth::PhaseConfig>(&phaseConfig, &phaseConfig + 1));

    ClothInstance->setSolverFrequency(60.0f);

    // Tether Constraints: 정점이 초기 위치에서 너무 멀리 떨어지지 못하게 제한
    ClothInstance->setTetherConstraintScale(1.0f);      // Tether 제약의 스케일
    ClothInstance->setTetherConstraintStiffness(0.5f);  // Tether 제약의 강성도

    // ===== 5. DynamicMesh 초기화 =====
    int numVertices = (int)ClothVertices.size();
    int numIndices = (int)ClothIndices.size();

    URenderer* renderer = GEngine.GetRenderer();

    UE_LOG("[ClothComponent::InitializeCloth] Renderer: %p, DynamicMesh: %p", renderer, DynamicMesh);

    if (renderer && DynamicMesh)
    {
        ID3D11Device* device = renderer->GetRHIDevice()->GetDevice();
        UE_LOG("[ClothComponent::InitializeCloth] D3D11Device: %p", device);

        DynamicMesh->Initialize(
            numVertices,
            numIndices,
            device,
            EVertexLayoutType::PositionColorTexturNormal
        );

        UE_LOG("[ClothComponent::InitializeCloth] DynamicMesh initialized successfully (V:%d, I:%d)", numVertices, numIndices);
    }
    else
    {
        UE_LOG("[ClothComponent::InitializeCloth] ERROR: Cannot initialize DynamicMesh - Renderer:%p, DynamicMesh:%p", renderer, DynamicMesh);
    }

    // ===== 6. 초기 MeshData 설정 =====
    if (MeshData)
    {
        MeshData->Vertices = ClothVertices;
        MeshData->Indices.clear();
        for (uint32_t idx : ClothIndices)
        {
            MeshData->Indices.push_back(idx);
        }

        // 기본 색상 (흰색)
        MeshData->Color.clear();
        for (size_t i = 0; i < ClothVertices.size(); i++)
        {
            MeshData->Color.push_back(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
        }

        // 기본 UV (단순 매핑)
        MeshData->UV.clear();
        int numVerticesX = ClothGridSizeX + 1;
        int numVerticesY = ClothGridSizeY + 1;
        for (size_t i = 0; i < ClothVertices.size(); i++)
        {
            int x = i % numVerticesX;
            int y = i / numVerticesX;
            float u = (float)x / (numVerticesX - 1);
            float v = (float)y / (numVerticesY - 1);
            MeshData->UV.push_back(FVector2D(u, v));
        }

        // 노말은 UpdateDynamicMesh에서 계산
        MeshData->Normal.clear();
        for (size_t i = 0; i < ClothVertices.size(); i++)
        {
            MeshData->Normal.push_back(FVector(0.0f, 0.0f, 1.0f));
        }

        // ===== 7. 초기 데이터를 GPU에 업로드 =====
        UpdateDynamicMesh();
        UE_LOG("[InitializeCloth] Complete - SUCCESS!");
    }
    else
    {
        UE_LOG("[InitializeCloth] ERROR: MeshData is null");
    }

    UE_LOG("[InitializeCloth] EXIT - ClothInstance=%p, ClothFabric=%p", ClothInstance, ClothFabric);
}

// @brief Cloth 리소스를 해제하고 Solver에서 제거합니다
// @note 컴포넌트 파괴 시 또는 재초기화 시 호출됩니다
void UClothComponent::ReleaseCloth()
{
    UE_LOG("[ClothComponent::ReleaseCloth] Called - ClothInstance:%p, ClothFabric:%p", ClothInstance, ClothFabric);

    if (ClothInstance)
    {
        // ===== Solver::removeCloth() 호출 =====
        // @brief Cloth를 Solver에서 제거하여 시뮬레이션 대상에서 제외합니다
        // @param cloth: 제거할 Cloth 객체
        // @note 제거 후에는 Solver::simulate()에서 이 Cloth가 업데이트되지 않습니다
        nv::cloth::Solver* solver = FClothSystem::GetInstance().GetSolver();
        UE_LOG("[ClothComponent::ReleaseCloth] Solver: %p", solver);

        if (solver)
        {
            solver->removeCloth(ClothInstance);
            UE_LOG("[ClothComponent::ReleaseCloth] Cloth removed from Solver");
        }
        else
        {
            UE_LOG("[ClothComponent::ReleaseCloth] WARNING: Solver is null!");
        }

        // Cloth 인스턴스 삭제
        delete ClothInstance;
        ClothInstance = nullptr;
        UE_LOG("[ClothComponent::ReleaseCloth] Cloth instance deleted");
    }

    if (ClothFabric)
    {
        // ===== Fabric::decRefCount() 호출 =====
        // @brief Fabric의 참조 카운트를 감소시킵니다
        // @note 참조 카운트가 0이 되면 Fabric이 자동으로 삭제됩니다
        //       여러 Cloth가 같은 Fabric을 공유할 수 있으므로 참조 카운트로 관리
        ClothFabric->decRefCount();
        ClothFabric = nullptr;
        UE_LOG("[ClothComponent::ReleaseCloth] Fabric reference released");
    }

    UE_LOG("[ClothComponent::ReleaseCloth] Complete");
}

// @brief 시뮬레이션 결과(정점 위치)를 Cloth에서 가져와 CPU 측 데이터를 업데이트합니다
// @note 매 프레임 TickComponent()에서 호출됩니다
//
// === 시뮬레이션 플로우 ===
// 1. World::Tick() -> ClothSystem::Update()에서 Solver::simulate() 호출 (GPU/CPU에서 시뮬레이션)
// 2. UClothComponent::TickComponent() -> UpdateSimulationResult() 호출 (결과 가져오기)
// 3. UpdateDynamicMesh() 호출하여 GPU 버퍼에 업로드
// 4. RenderClothPass()에서 렌더링
void UClothComponent::UpdateSimulationResult()
{
    if (!ClothInstance)
    {
        return;
    }

    // ===== Cloth::getCurrentParticles() 호출 =====
    // @brief 시뮬레이션된 정점 위치를 가져옵니다 (읽기 전용)
    // @return MappedRange<const PxVec4>: 정점 배열 (x, y, z, inverseMass)
    // @note const 버전이므로 수정 불가, 읽기만 가능합니다
    //       정점을 직접 수정하려면 setParticles()를 사용해야 합니다
    auto particles = ClothInstance->getCurrentParticles();

    // ===== CPU 측 정점 위치 업데이트 =====
    // 시뮬레이션 결과를 ClothVertices에 복사하여 렌더링에 사용
    static int updateCount = 0;
    static FVector lastPos;

    for (size_t i = 0; i < ClothVertices.size(); i++)
    {
        const physx::PxVec4& p = particles[i];
        ClothVertices[i] = FVector(p.x, p.y, p.z);
        // inverseMass(p.w)는 변하지 않으므로 무시
    }

    // 첫 번째 정점 위치 변화 확인 (60프레임마다 로그)
    if (updateCount++ % 60 == 0 && ClothVertices.size() > 0)
    {
        FVector currentPos = ClothVertices[0];
        float distance = (currentPos - lastPos).Size();
        UE_LOG("[UpdateSimulationResult] Frame %d: Vertex[0] = (%.2f, %.2f, %.2f), Moved: %.4fcm",
            updateCount, currentPos.X, currentPos.Y, currentPos.Z, distance);
        lastPos = currentPos;
    }
}

// @brief 시뮬레이션 파라미터(중력, 댐핑, 공기저항 등)를 Cloth에 적용합니다
// @note InitializeCloth() 후 또는 파라미터 변경 시 호출됩니다
void UClothComponent::ApplySimulationParameters()
{
    if (!ClothInstance)
    {
        return;
    }

    // ===== Cloth::setGravity() 호출 =====
    // @brief 중력 벡터를 설정합니다 (월드 공간)
    // @param gravity: 중력 가속도 (cm/s^2 단위, 예: (0, 0, -980))
    // @note Z-up 좌표계에서 일반적으로 (0, 0, -980)을 사용 (지구 중력)
    ClothInstance->setGravity(physx::PxVec3(Gravity.X, Gravity.Y, Gravity.Z));

    // ===== Cloth::setDamping() 호출 =====
    // @brief 속도 감쇠(댐핑) 계수를 설정합니다
    // @param damping: 각 축별 감쇠 계수 (0~1, 0=감쇠 없음, 1=완전 감쇠)
    // @note 댐핑은 천의 진동을 빠르게 안정화시키는 역할을 합니다
    //       값이 클수록 천이 빠르게 멈추지만 너무 크면 부자연스러움
    ClothInstance->setDamping(physx::PxVec3(Damping, Damping, Damping));

    // ===== Cloth::setDragCoefficient() 호출 =====
    // @brief 공기 저항 계수를 설정합니다
    // @param drag: 공기 저항 계수 (0~1, 0=저항 없음)
    // @note 천이 공기 중에서 움직일 때 받는 저항을 시뮬레이션
    ClothInstance->setDragCoefficient(0.05f);

    // ===== Cloth::setLiftCoefficient() 호출 =====
    // @brief 양력 계수를 설정합니다
    // @param lift: 양력 계수 (0~1)
    // @note 천이 바람에 의해 들어올려지는 효과를 시뮬레이션
    ClothInstance->setLiftCoefficient(0.05f);
}

void UClothComponent::UpdateDynamicMesh()
{
    if (!DynamicMesh || !MeshData)
    {
        return;
    }

    if (!DynamicMesh->IsInitialized())
    {
        return;
    }

    // MeshData 정점 위치 업데이트
    MeshData->Vertices = ClothVertices;

    // 노말 벡터 계산
    MeshData->Normal.clear();
    MeshData->Normal.resize(ClothVertices.size(), FVector(0.0f, 0.0f, 0.0f));

    // 각 삼각형의 노말을 계산하고 정점에 누적
    for (size_t i = 0; i < ClothIndices.size(); i += 3)
    {
        uint32_t idx0 = ClothIndices[i];
        uint32_t idx1 = ClothIndices[i + 1];
        uint32_t idx2 = ClothIndices[i + 2];

        const FVector& v0 = ClothVertices[idx0];
        const FVector& v1 = ClothVertices[idx1];
        const FVector& v2 = ClothVertices[idx2];

        // 삼각형 에지
        FVector edge1 = v1 - v0;
        FVector edge2 = v2 - v0;

        // 면 노말 (외적)
        FVector faceNormal = FVector::Cross(edge1, edge2);

        // 각 정점에 면 노말 누적
        MeshData->Normal[idx0] = MeshData->Normal[idx0] + faceNormal;
        MeshData->Normal[idx1] = MeshData->Normal[idx1] + faceNormal;
        MeshData->Normal[idx2] = MeshData->Normal[idx2] + faceNormal;
    }

    // 노말 정규화
    for (size_t i = 0; i < MeshData->Normal.size(); i++)
    {
        MeshData->Normal[i].Normalize();
    }

    // DynamicMesh에 반영
    URenderer* renderer = GEngine.GetRenderer();
    if (renderer)
    {
        DynamicMesh->UpdateData(MeshData, GEngine.GetRHIDevice()->GetDeviceContext());
    }
}

void UClothComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
    if (!DynamicMesh || !DynamicMesh->IsInitialized())
    {
        UE_LOG("[ClothComponent::CollectMeshBatches] FAILED - DynamicMesh not initialized");
        return;
    }

    if (DynamicMesh->GetCurrentIndexCount() == 0 || DynamicMesh->GetCurrentVertexCount() == 0)
    {
        UE_LOG("[ClothComponent::CollectMeshBatches] FAILED - No vertices/indices");
        return;
    }

    // Get default material
    UMaterial* Material = UResourceManager::GetInstance().GetDefaultMaterial();
    if (!Material)
    {
        UE_LOG("[ClothComponent::CollectMeshBatches] FAILED - Material is null");
        return;
    }

    UShader* Shader = Material->GetShader();
    if (!Shader)
    {
        UE_LOG("[ClothComponent::CollectMeshBatches] FAILED - Shader is null");
        return;
    }

    // Combine View shader macros with Material shader macros
    TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;
    if (Material->GetShaderMacros().Num() > 0)
    {
        ShaderMacros.Append(Material->GetShaderMacros());
    }

    // Compile shader variant
    FShaderVariant* ShaderVariant = Shader->GetOrCompileShaderVariant(ShaderMacros);
    if (!ShaderVariant)
    {
        UE_LOG("[ClothComponent::CollectMeshBatches] FAILED - ShaderVariant is null");
        return;
    }

    UE_LOG("[ClothComponent::CollectMeshBatches] SUCCESS - Adding batch");

    // Create batch element
    FMeshBatchElement BatchElement;
    BatchElement.VertexShader = ShaderVariant->VertexShader;
    BatchElement.PixelShader = ShaderVariant->PixelShader;
    BatchElement.InputLayout = ShaderVariant->InputLayout;
    BatchElement.Material = Material;
    BatchElement.VertexBuffer = DynamicMesh->GetVertexBuffer();
    BatchElement.IndexBuffer = DynamicMesh->GetIndexBuffer();
    BatchElement.VertexStride = sizeof(FVertexDynamic);  // PositionColorTexturNormal format
    BatchElement.IndexCount = DynamicMesh->GetCurrentIndexCount();
    BatchElement.StartIndex = 0;
    BatchElement.BaseVertexIndex = 0;
    BatchElement.WorldMatrix = GetWorldMatrix();
    BatchElement.ObjectID = InternalIndex;
    BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    OutMeshBatchElements.Add(BatchElement);
}
