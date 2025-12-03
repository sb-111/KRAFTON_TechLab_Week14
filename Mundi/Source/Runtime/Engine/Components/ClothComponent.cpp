#include "pch.h"
#include "ClothComponent.h"
#include "ClothSystem.h"
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
}

UClothComponent::~UClothComponent()
{
    ReleaseCloth();
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
        UpdateSimulationResult();
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

// ===== Cloth 메시 생성 =====
void UClothComponent::CreateClothFromPlane(int GridSizeX, int GridSizeY, float QuadSize)
{
    ClothVertices.clear();
    InverseMasses.clear();
    ClothIndices.clear();

    // 정점 생성 (GridSize 10x10 -> 정점 11x11 = 121개)
    for (int y = 0; y <= GridSizeY; y++)
    {
        for (int x = 0; x <= GridSizeX; x++)
        {
            // Z-up 왼손 좌표계: X-앞, Y-오른쪽, Z-위
            // 커튼은 YZ 평면에 위치 (X=0)
            FVector pos(0.0f, x * QuadSize, -y * QuadSize);
            ClothVertices.push_back(pos);

            // 최상단 행(y==0)만 고정 (InverseMass = 0)
            float invMass = (y == 0) ? 0.0f : 1.0f;
            InverseMasses.push_back(invMass);
        }
    }

    // 삼각형 인덱스 생성 (각 사각형 -> 2개 삼각형)
    for (int y = 0; y < GridSizeY; y++)
    {
        for (int x = 0; x < GridSizeX; x++)
        {
            int i0 = y * (GridSizeX + 1) + x;
            int i1 = i0 + 1;
            int i2 = i0 + (GridSizeX + 1);
            int i3 = i2 + 1;

            // 삼각형 1 (CCW for left-handed)
            ClothIndices.push_back(i0);
            ClothIndices.push_back(i2);
            ClothIndices.push_back(i1);

            // 삼각형 2
            ClothIndices.push_back(i1);
            ClothIndices.push_back(i2);
            ClothIndices.push_back(i3);
        }
    }
}

void UClothComponent::InitializeCloth()
{
    if (ClothVertices.empty() || ClothIndices.empty())
    {
        return;  // CreateClothFromPlane()을 먼저 호출해야 함
    }

    ReleaseCloth();  // 기존 Cloth 정리

    nv::cloth::Factory* factory = FClothSystem::GetInstance().GetFactory();
    if (!factory)
    {
        return;
    }

    // ===== 1. Fabric 생성 (간단한 거리 제약) =====
    TArray<uint32_t> phaseIndices;
    TArray<uint32_t> sets;
    TArray<float> restvalues;
    TArray<float> stiffnessValues;
    TArray<uint32_t> indices;

    // 간단한 구조적 제약: 인접 정점 간 거리
    phaseIndices.push_back(0);  // Phase 0 시작
    uint32_t constraintCount = 0;

    // 수평/수직 인접 정점 간 제약 생성
    int gridWidth = (int)sqrt(ClothVertices.size());
    for (size_t i = 0; i < ClothVertices.size(); i++)
    {
        int x = i % gridWidth;
        int y = i / gridWidth;

        // 오른쪽 정점 연결
        if (x < gridWidth - 1)
        {
            int neighbor = i + 1;
            indices.push_back((uint32_t)i);
            indices.push_back((uint32_t)neighbor);

            float dist = (ClothVertices[i] - ClothVertices[neighbor]).Size();
            restvalues.push_back(dist);
            stiffnessValues.push_back(Stiffness);
            constraintCount++;
        }

        // 아래 정점 연결
        if (y < gridWidth - 1)
        {
            int neighbor = i + gridWidth;
            indices.push_back((uint32_t)i);
            indices.push_back((uint32_t)neighbor);

            float dist = (ClothVertices[i] - ClothVertices[neighbor]).Size();
            restvalues.push_back(dist);
            stiffnessValues.push_back(Stiffness);
            constraintCount++;
        }
    }

    sets.push_back(constraintCount);  // Set 0에 모든 제약 포함

    // Fabric 생성
    nv::cloth::Range<const uint32_t> phaseRange(phaseIndices.data(), phaseIndices.data() + phaseIndices.size());
    nv::cloth::Range<const uint32_t> setsRange(sets.data(), sets.data() + sets.size());
    nv::cloth::Range<const float> restRange(restvalues.data(), restvalues.data() + restvalues.size());
    nv::cloth::Range<const float> stiffRange(stiffnessValues.data(), stiffnessValues.data() + stiffnessValues.size());
    nv::cloth::Range<const uint32_t> indicesRange(indices.data(), indices.data() + indices.size());

    ClothFabric = factory->createFabric(
        (uint32_t)ClothVertices.size(),
        phaseRange,
        setsRange,
        restRange,
        stiffRange,
        indicesRange,
        nv::cloth::Range<const uint32_t>(),  // anchors (사용 안 함)
        nv::cloth::Range<const float>(),     // tether lengths
        nv::cloth::Range<const uint32_t>()   // triangles
    );

    if (!ClothFabric)
    {
        return;
    }

    // ===== 2. Cloth 인스턴스 생성 =====
    // PxVec4로 변환 (x, y, z, inverseMass)
    TArray<physx::PxVec4> particles;
    for (size_t i = 0; i < ClothVertices.size(); i++)
    {
        const FVector& v = ClothVertices[i];
        float invMass = InverseMasses[i];
        particles.push_back(physx::PxVec4(v.X, v.Y, v.Z, invMass));
    }

    nv::cloth::Range<const physx::PxVec4> particleRange(particles.data(), particles.data() + particles.size());
    ClothInstance = factory->createCloth(particleRange, *ClothFabric);

    if (!ClothInstance)
    {
        ClothFabric->decRefCount();
        ClothFabric = nullptr;
        return;
    }

    // ===== 3. Solver에 추가 =====
    nv::cloth::Solver* solver = FClothSystem::GetInstance().GetSolver();
    if (solver)
    {
        solver->addCloth(ClothInstance);
    }

    // ===== 4. 파라미터 적용 =====
    ApplySimulationParameters();
}

void UClothComponent::ReleaseCloth()
{
    if (ClothInstance)
    {
        // Solver에서 제거
        nv::cloth::Solver* solver = FClothSystem::GetInstance().GetSolver();
        if (solver)
        {
            solver->removeCloth(ClothInstance);
        }

        delete ClothInstance;
        ClothInstance = nullptr;
    }

    if (ClothFabric)
    {
        ClothFabric->decRefCount();
        ClothFabric = nullptr;
    }
}

void UClothComponent::UpdateSimulationResult()
{
    if (!ClothInstance)
    {
        return;
    }

    // Cloth에서 시뮬레이션 결과 가져오기 (const 버전)
    auto particles = ClothInstance->getCurrentParticles();

    // ClothVertices 업데이트
    for (size_t i = 0; i < ClothVertices.size(); i++)
    {
        const physx::PxVec4& p = particles[i];
        ClothVertices[i] = FVector(p.x, p.y, p.z);
    }
}

void UClothComponent::ApplySimulationParameters()
{
    if (!ClothInstance)
    {
        return;
    }

    // 중력 설정
    ClothInstance->setGravity(physx::PxVec3(Gravity.X, Gravity.Y, Gravity.Z));

    // 댐핑 설정 (속도 감쇠)
    ClothInstance->setDamping(physx::PxVec3(Damping, Damping, Damping));

    // 공기 저항 (Drag, Lift)
    ClothInstance->setDragCoefficient(0.1f);
    ClothInstance->setLiftCoefficient(0.1f);
}
