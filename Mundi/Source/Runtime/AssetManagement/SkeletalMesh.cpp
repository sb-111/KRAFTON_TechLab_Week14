#include "pch.h"
#include "SkeletalMesh.h"

#include "PhysicsAsset.h"
#include "BodySetup.h"
#include "FKSphylElem.h"
#include "ConstraintInstance.h"
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

    // ===== Phase 1: Bodies 생성 =====
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
        Capsule.Length = 0.1f;   // 테스트용 기본값

        BodySetup->AggGeom.SphylElems.Add(Capsule);

        // 기본 물리 속성
        BodySetup->MassInKg = 1.0f;
        BodySetup->Friction = 0.5f;
        BodySetup->Restitution = 0.3f;
        BodySetup->LinearDamping = 0.1f;
        BodySetup->AngularDamping = 0.1f;

        PhysicsAsset->Bodies.Add(BodySetup);
    }

    // ===== Phase 2: Constraints 생성 (언리얼 방식) =====
    // 각 자식 본과 부모 본 사이에 Constraint 생성
    for (int32 i = 0; i < Skeleton.Bones.Num(); ++i)
    {
        const FBone& ChildBone = Skeleton.Bones[i];
        int32 ParentIndex = ChildBone.ParentIndex;

        // 루트 본은 부모가 없으므로 스킵
        if (ParentIndex < 0 || ParentIndex >= Skeleton.Bones.Num())
            continue;

        const FBone& ParentBone = Skeleton.Bones[ParentIndex];

        FConstraintInstance CI;
        CI.ConstraintBone1 = FName(ParentBone.Name);  // 부모
        CI.ConstraintBone2 = FName(ChildBone.Name);   // 자식

        // Joint 위치: 자식 본의 위치 (부모 본의 로컬 좌표계 기준)
        // 언리얼에서는 보통 자식 본의 원점을 Joint 위치로 사용
        CI.Position1 = FVector::Zero();  // 부모 로컬 좌표계에서의 위치 (나중에 런타임에 계산)
        CI.Position2 = FVector::Zero();  // 자식 로컬 좌표계에서의 위치 (원점)

        // Joint Frame 회전: 기본값 (나중에 런타임에 본 방향에 맞춰 계산)
        CI.Rotation1 = FVector::Zero();
        CI.Rotation2 = FVector::Zero();

        // 기본 각도 제한 (언리얼 기본값: 45도)
        CI.Swing1LimitAngle = 45.0f;
        CI.Swing2LimitAngle = 45.0f;
        CI.TwistLimitAngle = 45.0f;

        PhysicsAsset->Constraints.Add(CI);
    }

    UE_LOG("AutoGeneratePhysicsAsset: Generated %d bodies, %d constraints for skeleton '%s'",
           PhysicsAsset->Bodies.Num(), PhysicsAsset->Constraints.Num(), Skeleton.Name.c_str());
}
