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

    // ===== 흰색 텍스처 생성 (1x1 픽셀) - 한 번만 생성 =====
    // Vertex Color를 유지하기 위해 흰색 텍스처 사용
    // 셰이더: finalColor = VertexColor × TextureColor = VertexColor × (1,1,1) = VertexColor
    if (GEngine.GetRenderer())
    {
        ID3D11Device* device = GEngine.GetRenderer()->GetRHIDevice()->GetDevice();

        // 1x1 흰색 텍스처 데이터 (RGBA)
        uint32_t whitePixel = 0xFFFFFFFF;  // (255, 255, 255, 255)

        // 텍스처 생성
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_IMMUTABLE;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = &whitePixel;
        initData.SysMemPitch = 4;  // 4 bytes per pixel

        ID3D11Texture2D* whiteTexture = nullptr;
        HRESULT hr = device->CreateTexture2D(&texDesc, &initData, &whiteTexture);

        if (SUCCEEDED(hr) && whiteTexture)
        {
            // Shader Resource View 생성
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = texDesc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            hr = device->CreateShaderResourceView(whiteTexture, &srvDesc, &WhiteTextureSRV);
            whiteTexture->Release();  // SRV가 참조를 가지므로 Release

            if (SUCCEEDED(hr))
            {
                UE_LOG("[ClothComponent] White texture created once in constructor");
            }
        }
    }

    CreateClothFromPlane(MeshGridSizeX, MeshGridSizeY, MeshQuadSize);
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
    Super::BeginPlay();
}

void UClothComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (ClothInstance)
    {
        // ===== DeltaTime 안전 제한 (프레임 드랍 대비) =====
        // 최대 0.01초 (100 FPS 이하로 떨어지면 슬로우모션)
        // 이렇게 하지 않으면 프레임이 튈 때 거대한 힘이 가해져 정점이 날아감
        float ClampedDeltaTime = (DeltaTime > 0.01f) ? 0.01f : DeltaTime;

        ApplySimulationParameters();
        UpdateWind(ClampedDeltaTime);

        // ===== 시뮬레이션 결과 업데이트 및 렌더링 =====
        UpdateSimulationResult();
        UpdateDynamicMesh();
    }
}

void UClothComponent::EndPlay()
{
    Super::EndPlay();
}

void UClothComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);
}

void UClothComponent::OnUnregister()
{
    Super::OnUnregister();
    ReleaseCloth();
}

void UClothComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    // ===== MeshData 복제 =====
    MeshData = new FMeshData();

    // ===== DynamicMesh 복제 =====
    DynamicMesh = NewObject<UDynamicMesh>();

    // ===== 흰색 텍스처는 재생성하지 않음 (Constructor에서 한 번만 생성) =====
    // WhiteTextureSRV는 그대로 유지

    // ===== Cloth 시뮬레이션 리소스 재생성 =====
    ClothInstance = nullptr;
    ClothFabric = nullptr;

    // 원본의 그리드 크기와 QuadSize를 사용하여 복제
    int gridX = (ClothGridSizeX > 0) ? ClothGridSizeX : 1;
    int gridY = (ClothGridSizeY > 0) ? ClothGridSizeY : 1;
    float quadSize = (ClothQuadSize > 0.0f) ? ClothQuadSize : 10.0f;

    CreateClothFromPlane(gridX, gridY, quadSize);
    InitializeCloth();
}

// ===== Cloth 메시 생성 =====
// @brief 평면 그리드 형태의 Cloth 메시 데이터를 생성합니다
// @param GridSizeX 가로 방향 쿼드 개수 (정점은 GridSizeX+1개)
// @param GridSizeY 세로 방향 쿼드 개수 (정점은 GridSizeY+1개)
// @param QuadSize 각 쿼드의 크기 (cm 단위)
// @note 이 함수는 단순히 CPU 측 데이터만 생성하며, InitializeCloth()를 호출해야 실제 시뮬레이션이 시작됩니다
void UClothComponent::CreateClothFromPlane(int GridSizeX, int GridSizeY, float QuadSize)
{
    ClothVertices.clear();
    InitialLocalPositions.clear();
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
            FVector localPos(0.0f, x * QuadSize, -y * QuadSize);
            ClothVertices.push_back(localPos);
            InitialLocalPositions.push_back(localPos);  // 로컬 위치 저장 (Transform 추적용)

            // ===== InverseMass (질량의 역수) 설정 =====
            // inverseMass = 0 -> 무한대 질량 = 고정된 정점 (Fixed/Pinned)
            // inverseMass = 1 -> 질량 1.0 = 시뮬레이션되는 정점 (Dynamic)
            // 최상단 행은 고정하되, Motion Constraints로 위치를 제어
            // 고정된 정점도 Motion Constraints로 이동 가능
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
    if (ClothVertices.empty() || ClothIndices.empty())
    {
        return;  // CreateClothFromPlane()을 먼저 호출해야 함
    }
    
    ReleaseCloth();  // 기존 Cloth 정리

    // ===== NvCloth Factory 가져오기 =====
    // Factory: NvCloth 객체(Fabric, Cloth, Solver)를 생성하는 팩토리 클래스
    // CPU 또는 GPU 기반 Factory가 있으며, 현재는 CPU Factory 사용
    nv::cloth::Factory* factory = FClothSystem::GetInstance().GetFactory();
    if (!factory) return;

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
    // 수평/수직/대각선 인접 정점 간 거리 제약을 생성하여 천의 구조를 유지합니다
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

            // ===== 대각선 제약 추가 (Shear Resistance) =====
            // 대각선 제약은 천의 전단(shear) 변형을 방지합니다
            // - 수평/수직 제약만으로는 천이 마름모꼴로 찌그러질 수 있음
            // - 대각선 제약을 추가하면 천이 사각형 형태를 유지
            // - Stiffness는 수평/수직보다 약간 낮게 설정 (자연스러운 주름 형성)

            // 대각선 제약 (오른쪽 아래)
            if (x < numVerticesX - 1 && y < numVerticesY - 1)
            {
                int neighbor = i + numVerticesX + 1;
                indices.push_back((uint32_t)i);
                indices.push_back((uint32_t)neighbor);

                float dist = (ClothVertices[i] - ClothVertices[neighbor]).Size();
                restvalues.push_back(dist);
                stiffnessValues.push_back(Stiffness * DiagonalStiffnessMultiplier);
                constraintCount++;
            }

            // 대각선 제약 (왼쪽 아래)
            if (x > 0 && y < numVerticesY - 1)
            {
                int neighbor = i + numVerticesX - 1;
                indices.push_back((uint32_t)i);
                indices.push_back((uint32_t)neighbor);

                float dist = (ClothVertices[i] - ClothVertices[neighbor]).Size();
                restvalues.push_back(dist);
                stiffnessValues.push_back(Stiffness * DiagonalStiffnessMultiplier);
                constraintCount++;
            }

            // ===== Bending Constraints 추가 (부드러운 굽힘) =====
            // 2-hop 거리의 정점들을 연결하여 천이 뾰족하게 접히는 것을 방지
            // - 1-hop: 인접 정점 (Distance Constraints)
            // - 2-hop: 2칸 떨어진 정점 (Bending Constraints)
            // - 매우 낮은 Stiffness로 설정하여 부드러운 곡선 형성

            // 수평 Bending (2칸 오른쪽)
            if (x < numVerticesX - 2)
            {
                int neighbor = i + 2;
                indices.push_back((uint32_t)i);
                indices.push_back((uint32_t)neighbor);

                float dist = (ClothVertices[i] - ClothVertices[neighbor]).Size();
                restvalues.push_back(dist);
                stiffnessValues.push_back(Stiffness * Bending2HopMultiplier);
                constraintCount++;
            }

            // 수직 Bending (2칸 아래)
            if (y < numVerticesY - 2)
            {
                int neighbor = i + numVerticesX * 2;
                indices.push_back((uint32_t)i);
                indices.push_back((uint32_t)neighbor);

                float dist = (ClothVertices[i] - ClothVertices[neighbor]).Size();
                restvalues.push_back(dist);
                stiffnessValues.push_back(Stiffness * Bending2HopMultiplier);
                constraintCount++;
            }

            // ===== 3-hop Bending Constraints (추가 부드러움) =====
            // 3칸 떨어진 정점을 연결하여 더욱 부드러운 곡선 생성

            // 수평 3-hop Bending
            if (x < numVerticesX - 3)
            {
                int neighbor = i + 3;
                indices.push_back((uint32_t)i);
                indices.push_back((uint32_t)neighbor);

                float dist = (ClothVertices[i] - ClothVertices[neighbor]).Size();
                restvalues.push_back(dist);
                stiffnessValues.push_back(Stiffness * Bending3HopMultiplier);
                constraintCount++;
            }

            // 수직 3-hop Bending
            if (y < numVerticesY - 3)
            {
                int neighbor = i + numVerticesX * 3;
                indices.push_back((uint32_t)i);
                indices.push_back((uint32_t)neighbor);

                float dist = (ClothVertices[i] - ClothVertices[neighbor]).Size();
                restvalues.push_back(dist);
                stiffnessValues.push_back(Stiffness * Bending3HopMultiplier);
                constraintCount++;
            }
        }
    }

    phaseIndices.push_back(constraintCount);

    sets.push_back(1);

    // ===== Tether Constraints 생성 =====
    // Tether: 각 정점이 고정된 앵커로부터 너무 멀리 떨어지지 못하게 제한
    // - 천이 무한정 늘어나는 것을 방지하는 핵심 제약
    // - 각 정점에서 가장 가까운 고정 앵커(최상단 행)까지의 최대 거리를 설정
    // - NvCloth는 이 거리를 초과하면 정점을 앵커 방향으로 당김
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

            // ===== Tether 최대 거리 계산 =====
            // - 초기 거리: 정점과 앵커 사이의 초기 거리
            // - 허용 배율: 바람으로 날아가는 것을 방지하기 위해 엄격하게 제한
            FVector pos = ClothVertices[i];
            FVector anchorPos = ClothVertices[anchorIdx];
            float initialDist = (pos - anchorPos).Size();
            float maxDist = initialDist * TetherMaxDistanceMultiplier;
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

    // ===== 삼각형 인덱스 준비 (바람 효과에 필수!) =====
    // NvCloth는 삼각형 정보를 사용하여 천의 표면 법선을 계산하고,
    // 이를 통해 바람이 천에 가하는 힘의 방향과 크기를 결정합니다
    nv::cloth::Range<const uint32_t> trianglesRange(ClothIndices.data(), ClothIndices.data() + ClothIndices.size());

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
    // @param triangles: 삼각형 인덱스 (바람 효과와 충돌 계산에 필요)
    ClothFabric = factory->createFabric(
        (uint32_t)ClothVertices.size(),
        phaseRange,
        setsRange,
        restRange,
        stiffRange,
        indicesRange,
        tetherAnchorsRange,    // Tether 앵커 인덱스
        tetherLengthsRange,    // Tether 최대 거리
        trianglesRange         // 삼각형 인덱스 (바람 효과에 필수!)
    );

    if (!ClothFabric) return;

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

    if (!ClothInstance)
    {
        ClothFabric->decRefCount();  // Fabric 참조 카운트 감소
        ClothFabric = nullptr;
        return;
    }

    // ===== 3. Solver에 추가 =====
    // Solver: 물리 시뮬레이션 엔진으로, 여러 Cloth를 관리하고 업데이트합니다
    //   - 매 프레임 Solver::simulate()를 호출하면 등록된 모든 Cloth가 시뮬레이션됨
    //   - 병렬 처리, 충돌 감지 등을 담당
    nv::cloth::Solver* solver = FClothSystem::GetInstance().GetSolver();
    if (solver)
    {
        // ===== Solver::addCloth() 호출 =====
        // @brief Cloth를 Solver에 등록하여 시뮬레이션 대상으로 만듭니다
        // @param cloth: 등록할 Cloth 객체
        // @note 등록 후 Solver::simulate()가 호출될 때마다 이 Cloth가 업데이트됩니다
        solver->addCloth(ClothInstance);
    }

    // ===== 4. 시뮬레이션 파라미터 적용 =====
    ApplySimulationParameters();

    // ===== PhaseConfig 설정 (제약 조건 해결 설정) =====
    // PhaseConfig: Distance Constraints의 해결(Solve) 방식을 제어
    // - Stiffness: 제약의 강성도
    // - StretchLimit: 늘어남 제한
    // - CompressionLimit: 압축 제한
    nv::cloth::PhaseConfig phaseConfig;
    phaseConfig.mPhaseIndex = 0;
    phaseConfig.mStiffness = Stiffness;
    phaseConfig.mStiffnessMultiplier = PhaseStiffnessMultiplier;
    phaseConfig.mCompressionLimit = CompressionLimit;
    phaseConfig.mStretchLimit = StretchLimit;
    ClothInstance->setPhaseConfig(nv::cloth::Range<nv::cloth::PhaseConfig>(&phaseConfig, &phaseConfig + 1));

    // ===== Solver Frequency 설정 =====
    // Solver Frequency: 한 프레임 동안 제약 조건을 몇 번 해결할지 결정
    // - 낮추면 부드럽게 접히지만, Tether로 안정성 보장
    ClothInstance->setSolverFrequency(SolverFrequency);

    // ===== Tether Constraints 설정 =====
    // Tether: 각 정점이 고정된 앵커로부터 너무 멀리 떨어지지 못하게 제한
    // - TetherConstraintScale: Fabric에 정의된 Tether 길이의 배율
    // - TetherConstraintStiffness: 강성도 (바람으로 날아가는 것 방지)
    ClothInstance->setTetherConstraintScale(TetherConstraintScale);
    ClothInstance->setTetherConstraintStiffness(TetherConstraintStiffness);

    // ===== Friction 설정 =====
    // 마찰 계수 설정 (천이 접힐 때 부드럽게)
    ClothInstance->setFriction(Friction);

    // ===== 5. Cloth 전용 Material 생성 =====
    // Vertex Color를 제대로 표현하기 위해 DiffuseColor를 (1,1,1)로 설정
    UMaterial* DefaultMaterial = UResourceManager::GetInstance().GetDefaultMaterial();
    if (DefaultMaterial)
    {
        ClothMaterial = NewObject<UMaterial>();
        // 기본 Material 설정 복사
        ClothMaterial->SetShader(DefaultMaterial->GetShader());
        // ShaderMacros는 비워둠 -> View의 조명 설정(ViewShaderMacros)이 적용되도록
        ClothMaterial->SetShaderMacros(TArray<FShaderMacro>());

        // Material Info 복사 후 DiffuseColor를 흰색으로 설정
        FMaterialInfo MatInfo = DefaultMaterial->GetMaterialInfo();
        MatInfo.DiffuseColor = FVector(1.0f, 1.0f, 1.0f);  // Vertex Color가 그대로 표현되도록
        MatInfo.AmbientColor = FVector(1.0f, 1.0f, 1.0f);  // Ambient도 흰색으로
        ClothMaterial->SetMaterialInfo(MatInfo);

        UE_LOG("[ClothComponent] ClothMaterial created with white DiffuseColor and empty ShaderMacros for lighting");
    }

    // ===== 6. DynamicMesh 초기화 =====
    int numVertices = (int)ClothVertices.size();
    int numIndices = (int)ClothIndices.size();

    URenderer* renderer = GEngine.GetRenderer();

    if (renderer && DynamicMesh)
    {
        ID3D11Device* device = renderer->GetRHIDevice()->GetDevice();

        DynamicMesh->Initialize(
            numVertices,
            numIndices,
            device,
            EVertexLayoutType::PositionColorTexturNormal
        );
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

        // 천 색상 설정 (알파는 1.0 고정)
        MeshData->Color.clear();
        for (size_t i = 0; i < ClothVertices.size(); i++)
        {
            MeshData->Color.push_back(FVector4(ClothColor.X, ClothColor.Y, ClothColor.Z, 1.0f));
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
    }

    // ===== 8. Transform 추적 초기화 =====
    PreviousWorldTransform = GetWorldTransform();
}

// @brief Cloth 리소스를 해제하고 Solver에서 제거합니다
// @note 컴포넌트 파괴 시 또는 재초기화 시 호출됩니다
void UClothComponent::ReleaseCloth()
{
    if (ClothInstance)
    {
        // ===== Solver::removeCloth() 호출 =====
        // @brief Cloth를 Solver에서 제거하여 시뮬레이션 대상에서 제외합니다
        // @param cloth: 제거할 Cloth 객체
        // @note 제거 후에는 Solver::simulate()에서 이 Cloth가 업데이트되지 않습니다
        nv::cloth::Solver* solver = FClothSystem::GetInstance().GetSolver();

        if (solver)
        {
            solver->removeCloth(ClothInstance);
        }

        // Cloth 인스턴스 삭제
        delete ClothInstance;
        ClothInstance = nullptr;
    }

    if (ClothFabric)
    {
        // ===== Fabric::decRefCount() 호출 =====
        // @brief Fabric의 참조 카운트를 감소시킵니다
        // @note 참조 카운트가 0이 되면 Fabric이 자동으로 삭제됩니다
        //       여러 Cloth가 같은 Fabric을 공유할 수 있으므로 참조 카운트로 관리
        ClothFabric->decRefCount();
        ClothFabric = nullptr;
    }

    // 흰색 텍스처 정리
    if (WhiteTextureSRV)
    {
        WhiteTextureSRV->Release();
        WhiteTextureSRV = nullptr;
    }
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

    // ===== Cloth::getCurrentParticles() 호출 (쓰기 가능 버전) =====
    ClothInstance->lockParticles();
    auto particles = ClothInstance->getCurrentParticles();

    // ===== CPU 측 정점 위치 업데이트 (NaN/Inf/거리 제한 방어) =====
    // MaxDistance를 QuadSize에 비례하게 설정 (QuadSize * 3.0)
    const float MaxDistanceFromInitial = ClothQuadSize * std::max(MeshGridSizeX, MeshGridSizeY) * 2;
    bool bInstabilityDetected = false;

    for (size_t i = 0; i < ClothVertices.size(); i++)
    {
        physx::PxVec4& p = particles[i];

        // NaN/Inf 체크
        if (isnan(p.x) || isnan(p.y) || isnan(p.z) ||
            isinf(p.x) || isinf(p.y) || isinf(p.z))
        {
            // 초기 위치로 복구 (시뮬레이션에도 반영)
            FVector initialPos = InitialLocalPositions[i];
            p.x = initialPos.X;
            p.y = initialPos.Y;
            p.z = initialPos.Z;
            ClothVertices[i] = initialPos;
            bInstabilityDetected = true;
            continue;
        }

        FVector newPos(p.x, p.y, p.z);

        // 초기 위치로부터 최대 거리 제한 (Tether 실패 대비)
        FVector initialPos = InitialLocalPositions[i];
        FVector delta = newPos - initialPos;
        float distance = delta.Size();

        if (distance > MaxDistanceFromInitial)
        {
            // 클램핑된 위치로 수정 (시뮬레이션에도 반영)
            newPos = initialPos + delta * (MaxDistanceFromInitial / distance);
            p.x = newPos.X;
            p.y = newPos.Y;
            p.z = newPos.Z;
            bInstabilityDetected = true;
        }

        ClothVertices[i] = newPos;
    }

    ClothInstance->unlockParticles();

    // ===== 불안정 감지 시 관성 제거 (속도/가속도 초기화) =====
    if (bInstabilityDetected)
    {
        ClothInstance->clearInertia();
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
    // @brief 공기 저항 계수를 설정합니다 (Drag Force: Fd = ½ρv²cdragA)
    // @param drag: cdrag 계수 (0~1, 0=저항 없음)
    // @note 천이 공기 중에서 움직일 때 받는 저항을 시뮬레이션
    //       바람 효과가 작동하려면 이 값이 충분히 커야 함 (권장: 0.5~1.0)
    ClothInstance->setDragCoefficient(DragCoefficient);

    // ===== Cloth::setLiftCoefficient() 호출 =====
    // @brief 양력 계수를 설정합니다 (Lift Force: Fl = clift½ρv²A)
    // @param lift: clift 계수 (0~1)
    // @note 천이 바람에 의해 들어올려지는 효과를 시뮬레이션
    //       값이 클수록 바람에 의해 더 많이 들려 올라감 (권장: 0.6~1.0)
    ClothInstance->setLiftCoefficient(LiftCoefficient);

    // ===== Cloth::setFluidDensity() 호출 =====
    // @brief 유체 밀도를 설정합니다 (ρ, Drag/Lift Force 계산에 사용)
    // @param density: 유체 밀도 (기본값 1.0)
    // @note 바람의 힘은 ρ에 비례하므로, 이 값을 조절하여 전체적인 바람 효과 강도 조절 가능
    ClothInstance->setFluidDensity(FluidDensity);
} 

// @brief 바람 효과를 업데이트합니다
// @note NvCloth Wind Simulation 수식:
//       - Drag Force: Fd = ½ρv²cdragA
//       - Lift Force: Fl = clift½ρv²A
//       - 각 삼각형에 대해 위치 차이 d = ct - pt + wind 계산 후 힘 적용
//       - |d|² > epsilon일 때만 힘 적용 (수치 안정성)
void UClothComponent::UpdateWind(float DeltaTime)
{
    if (!ClothInstance || !bEnableWind)
    {
        // 바람 비활성화 시 바람 속도 0으로
        if (ClothInstance)
        {
            ClothInstance->setWindVelocity(physx::PxVec3(0.0f, 0.0f, 0.0f));
        }
        return;
    }

    // 시간 누적
    WindTime += DeltaTime;

    // ===== 자연스러운 바람 시뮬레이션 (Perlin-like 패턴) =====
    // 기본 방향 정규화
    FVector baseDirection = WindDirection;
    baseDirection.Normalize();

    // 주 바람 파동 (저주파) - 천천히 변화하는 기본 강도
    // 0.0 ~ 1.0 범위 (주기적으로 0이 되어 천이 자연스럽게 내려옴)
    float mainWave = 0.5f + 0.5f * sin(WindTime * WindFrequency * 0.5f);  // 0~1

    // 중간 바람 파동 (중주파) - 일반적인 바람 변화
    float midWave = sin(WindTime * WindFrequency * 1.0f);  // -1~1

    // 돌풍 (고주파) - 빠른 난류 효과
    float gustWave = sin(WindTime * WindFrequency * 3.7f);  // -1~1
    float gustWave2 = cos(WindTime * WindFrequency * 5.3f);  // -1~1 (다른 주파수)

    // ===== 최종 바람 강도 계산 =====
    // mainWave: 기본 세기 (0~1)
    // midWave: 중간 변동 추가 (±Turbulence * 0.3)
    // gustWave: 돌풍 효과 (±Turbulence * 0.2)
    float windIntensity = mainWave
                        + midWave * WindTurbulence * 0.3f
                        + gustWave * WindTurbulence * 0.2f
                        + gustWave2 * WindTurbulence * 0.1f;

    // 0 이상으로 클램핑 (음수 바람 방지)
    windIntensity = (windIntensity < 0.0f) ? 0.0f : windIntensity;
    // 1.5 이하로 클램핑 (너무 강한 바람 방지)
    windIntensity = (windIntensity > 1.5f) ? 1.5f : windIntensity;

    float finalStrength = WindStrength * windIntensity;

    // ===== 바람 방향 변화 (난류 효과) =====
    // 방향 변화는 Turbulence에 비례하되, 너무 크지 않게 제한
    float dirVariationX = sin(WindTime * WindFrequency * 0.7f) * WindTurbulence * 0.15f;
    float dirVariationY = cos(WindTime * WindFrequency * 0.9f) * WindTurbulence * 0.15f;
    float dirVariationZ = sin(WindTime * WindFrequency * 1.3f) * WindTurbulence * 0.08f;

    FVector finalDirection = baseDirection + FVector(dirVariationX, dirVariationY, dirVariationZ);
    finalDirection.Normalize();

    // ===== 최종 바람 속도 벡터 (v) =====
    // NvCloth는 이 속도를 사용하여 각 삼각형에 대해 Drag/Lift Force 계산
    FVector windVelocity = finalDirection * finalStrength;

    // ===== NvCloth에 바람 속도 설정 =====
    // setWindVelocity()는 내부적으로 다음을 수행:
    // - 각 삼각형의 중심 위치 계산 (ct, pt)
    // - 위치 차이: d = ct - pt + wind
    // - |d|² > epsilon일 때만:
    //   - Lift impulse: ilift = clift · cos(θ) · sin(θ) · ldir · |d| · Δt⁻¹
    //   - Drag impulse: idrag = cdrag · |cos(θ)| · d · |d| · Δt⁻¹
    //   - 합성: i = (ilift + idrag) · ρ · area

    float finalVelocityX = isnan(windVelocity.X) || isinf(windVelocity.X) ? 0.0f : windVelocity.X;
    float finalVelocityY = isnan(windVelocity.Y) || isinf(windVelocity.Y) ? 0.0f : windVelocity.Y;
    float finalVelocityZ = isnan(windVelocity.Z) || isinf(windVelocity.Z) ? 0.0f : windVelocity.Z;
    
    ClothInstance->setWindVelocity(physx::PxVec3(finalVelocityX, finalVelocityY, finalVelocityZ));
    // ClothInstance->setWindVelocity(physx::PxVec3(WindDirection.X, WindDirection.Y, WindDirection.Z));
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

    // 색상 업데이트 (에디터에서 변경 가능, 알파는 1.0 고정)
    MeshData->Color.clear();
    for (size_t i = 0; i < ClothVertices.size(); i++)
    {
        MeshData->Color.push_back(FVector4(ClothColor.X, ClothColor.Y, ClothColor.Z, 1.0f));
    }

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

        // 면 노말 (외적) - 반전시켜서 카메라를 향하도록
        FVector faceNormal = -FVector::Cross(edge1, edge2);

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

    // Cloth 전용 Material 사용 및 DiffuseColor를 ClothColor로 업데이트
    UMaterial* Material = ClothMaterial;
    if (!Material)
    {
        // Fallback: ClothMaterial이 없으면 기본 Material 사용
        Material = UResourceManager::GetInstance().GetDefaultMaterial();
        if (!Material)
        {
            UE_LOG("[ClothComponent::CollectMeshBatches] FAILED - Material is null");
            return;
        }
    }

    // Material의 DiffuseColor를 ClothColor로 동적 업데이트 (매 프레임)
    if (ClothMaterial)
    {
        FMaterialInfo MatInfo = ClothMaterial->GetMaterialInfo();
        MatInfo.DiffuseColor = ClothColor;  // ClothColor를 DiffuseColor로 설정
        MatInfo.AmbientColor = ClothColor;  // Ambient도 동일하게
        ClothMaterial->SetMaterialInfo(MatInfo);
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
    
    // Create batch element
    FMeshBatchElement BatchElement;
    BatchElement.VertexShader = ShaderVariant->VertexShader;
    BatchElement.PixelShader = ShaderVariant->PixelShader;
    BatchElement.InputLayout = ShaderVariant->InputLayout;
    BatchElement.Material = Material;  // ClothMaterial (DiffuseColor=흰색, Vertex Color 곱셈)
    BatchElement.InstanceShaderResourceView = nullptr;  // 텍스처 없음 (Material.DiffuseColor 사용)
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
