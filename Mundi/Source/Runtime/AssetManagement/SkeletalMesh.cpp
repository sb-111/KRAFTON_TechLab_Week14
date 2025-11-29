#include "pch.h"
#include "SkeletalMesh.h"

#include "PhysicsAsset.h"
#include "BodySetup.h"
#include "FKSphylElem.h"
#include "FbxLoader.h"
#include "WindowsBinReader.h"
#include "WindowsBinWriter.h"
#include "PathUtils.h"
#include <filesystem>

IMPLEMENT_CLASS(USkeletalMesh)

USkeletalMesh::USkeletalMesh()
{
}

USkeletalMesh::~USkeletalMesh()
{
    ReleaseResources();
}

void USkeletalMesh::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
    if (Data)
    {
        ReleaseResources();
    }

    // FBXLoader가 캐싱을 내부적으로 처리합니다
    Data = UFbxLoader::GetInstance().LoadFbxMeshAsset(InFilePath);

    if (!Data || Data->Vertices.empty() || Data->Indices.empty())
    {
        UE_LOG("ERROR: Failed to load FBX mesh from '%s'", InFilePath.c_str());
        return;
    }

    // GPU 버퍼 생성
    CreateIndexBuffer(Data, InDevice);
    VertexCount = static_cast<uint32>(Data->Vertices.size());
    IndexCount = static_cast<uint32>(Data->Indices.size());
    VertexStride = sizeof(FVertexDynamic);
}

void USkeletalMesh::ReleaseResources()
{
    if (IndexBuffer)
    {
        IndexBuffer->Release();
        IndexBuffer = nullptr;
    }

    if (Data)
    {
        delete Data;
        Data = nullptr;
    }
}

void USkeletalMesh::CreateVertexBuffer(ID3D11Buffer** InVertexBuffer)
{
    if (!Data) { return; }
    ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
    HRESULT hr = D3D11RHI::CreateVertexBuffer<FVertexDynamic>(Device, Data->Vertices, InVertexBuffer);
    assert(SUCCEEDED(hr));
}

void USkeletalMesh::UpdateVertexBuffer(const TArray<FNormalVertex>& SkinnedVertices, ID3D11Buffer* InVertexBuffer)
{
    if (!InVertexBuffer) { return; }

    GEngine.GetRHIDevice()->VertexBufferUpdate(InVertexBuffer, SkinnedVertices);
}

void USkeletalMesh::CreateGPUSkinnedVertexBuffer(ID3D11Buffer** InVertexBuffer)
{
    if (!Data) { return; }
    ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();

    // FSkinnedVertex를 그대로 사용하는 버텍스 버퍼 생성
    D3D11_BUFFER_DESC BufferDesc;
    ZeroMemory(&BufferDesc, sizeof(BufferDesc));
    BufferDesc.Usage = D3D11_USAGE_IMMUTABLE; // GPU 스키닝 모드에서는 버텍스 데이터 변경 없음
    BufferDesc.ByteWidth = static_cast<UINT>(sizeof(FSkinnedVertex) * Data->Vertices.size());
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BufferDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = Data->Vertices.data();

    HRESULT hr = Device->CreateBuffer(&BufferDesc, &InitData, InVertexBuffer);
    assert(SUCCEEDED(hr));
}

void USkeletalMesh::CreateIndexBuffer(FSkeletalMeshData* InSkeletalMesh, ID3D11Device* InDevice)
{
    HRESULT hr = D3D11RHI::CreateIndexBuffer(InDevice, InSkeletalMesh, &IndexBuffer);
    assert(SUCCEEDED(hr));
}

void USkeletalMesh::AutoGeneratePhysicsAsset()
{
    if (!Data || Data->Skeleton.Bones.empty())
    {
        UE_LOG("AutoGeneratePhysicsAsset: No skeleton data");
        return;
    }

    // 이미 PhysicsAsset이 있으면 스킵
    if (PhysicsAsset && PhysicsAsset->Bodies.Num() > 0)
    {
        UE_LOG("AutoGeneratePhysicsAsset: Already has PhysicsAsset");
        return;
    }

    // 새 PhysicsAsset 생성
    PhysicsAsset = new UPhysicsAsset();

    const FSkeleton& Skeleton = Data->Skeleton;

    // 각 본에 대해 BodySetup 생성
    for (int32 i = 0; i < Skeleton.Bones.Num(); ++i)
    {
        const FBone& Bone = Skeleton.Bones[i];

        UBodySetup* BodySetup = new UBodySetup();
        BodySetup->BoneName = FName(Bone.Name);

        // 기본 캡슐 Shape 추가
        FKSphylElem Capsule;
        Capsule.Name = FName(Bone.Name);
        Capsule.Center = FVector::Zero();
        Capsule.Rotation = FVector::Zero();
        Capsule.Radius = 0.1f;   // 테스트용 기본값
        Capsule.Length = 0.1f;  // 테스트용 기본값

        BodySetup->AggGeom.SphylElems.Add(Capsule);

        // 기본 물리 속성
        BodySetup->MassInKg = 1.0f;
        BodySetup->Friction = 0.5f;
        BodySetup->Restitution = 0.3f;
        BodySetup->LinearDamping = 0.1f;
        BodySetup->AngularDamping = 0.1f;

        PhysicsAsset->Bodies.Add(BodySetup);
    }

    UE_LOG("AutoGeneratePhysicsAsset: Generated %d bodies for skeleton '%s'",
           PhysicsAsset->Bodies.Num(), Skeleton.Name.c_str());
}
