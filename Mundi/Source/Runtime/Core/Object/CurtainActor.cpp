#include "pch.h"
#include "CurtainActor.h"
#include "ClothComponent.h"
#include "DynamicMesh.h"
#include "VertexData.h"
#include "Renderer.h"

IMPLEMENT_CLASS(ACurtainActor)

ACurtainActor::ACurtainActor()
{
    // ClothComponent 생성
    ClothComp = CreateDefaultSubobject<UClothComponent>("ClothComponent");

    // DynamicMesh 생성
    DynamicMesh = NewObject<UDynamicMesh>();

    // MeshData 생성
    MeshData = new FMeshData();

    bCanEverTick = true;
}

ACurtainActor::~ACurtainActor()
{
    if (MeshData)
    {
        delete MeshData;
        MeshData = nullptr;
    }
}

void ACurtainActor::BeginPlay()
{
    Super::BeginPlay();

    if (!ClothComp)
    {
        return;
    }

    // ===== Cloth 메시 생성 =====
    ClothComp->CreateClothFromPlane(GridSizeX, GridSizeY, QuadSize);
    ClothComp->InitializeCloth();

    // ===== DynamicMesh 초기화 =====
    int numVertices = (GridSizeX + 1) * (GridSizeY + 1);
    int numIndices = GridSizeX * GridSizeY * 6;  // 각 사각형 = 2개 삼각형 = 6 인덱스

    URenderer* renderer = GEngine.GetRenderer();
    if (renderer && DynamicMesh)
    {
        DynamicMesh->Initialize(
            numVertices,
            numIndices,
            renderer->GetRHIDevice()->GetDevice(),
            EVertexLayoutType::PositionColor
        );
    }

    // ===== 초기 메시 데이터 설정 =====
    if (MeshData)
    {
        const TArray<FVector>& vertices = ClothComp->GetClothVertices();
        const TArray<uint32_t>& indices = ClothComp->GetClothIndices();

        MeshData->Vertices = vertices;
        MeshData->Indices.clear();
        for (uint32_t idx : indices)
        {
            MeshData->Indices.push_back(idx);
        }

        // 기본 색상 (흰색)
        MeshData->Color.clear();
        for (size_t i = 0; i < vertices.size(); i++)
        {
            MeshData->Color.push_back(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }
}

void ACurtainActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateMeshFromCloth();
}

void ACurtainActor::UpdateMeshFromCloth()
{
    if (!ClothComp || !DynamicMesh || !MeshData)
    {
        return;
    }

    // Cloth 정점 가져오기
    const TArray<FVector>& clothVertices = ClothComp->GetClothVertices();

    // MeshData 업데이트
    MeshData->Vertices = clothVertices;

    // DynamicMesh에 반영
    URenderer* renderer = GEngine.GetRenderer();
    if (renderer)
    {
        DynamicMesh->UpdateData(MeshData, GEngine.GetRHIDevice()->GetDeviceContext());
    }
}
