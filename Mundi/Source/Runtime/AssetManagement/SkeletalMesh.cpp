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
#include <cmath>

// ===== 본 크기 기반 캡슐 자동 생성 헬퍼 함수들 =====

// BindPose 매트릭스에서 위치 추출
static FVector GetBonePosition(const FMatrix& BindPose)
{
    return FVector(BindPose.M[3][0], BindPose.M[3][1], BindPose.M[3][2]);
}

// 방향 벡터를 매트릭스의 회전 부분으로 변환 (위치 무시)
static FVector TransformDirection(const FMatrix& M, const FVector& Dir)
{
    return FVector(
        Dir.X * M.M[0][0] + Dir.Y * M.M[1][0] + Dir.Z * M.M[2][0],
        Dir.X * M.M[0][1] + Dir.Y * M.M[1][1] + Dir.Z * M.M[2][1],
        Dir.X * M.M[0][2] + Dir.Y * M.M[1][2] + Dir.Z * M.M[2][2]
    );
}

// 본 길이 계산 및 본의 로컬 방향 반환
static float CalculateBoneLength(const FSkeleton& Skeleton, int32 BoneIndex, FVector& OutBoneLocalDir)
{
    const FBone& CurrentBone = Skeleton.Bones[BoneIndex];
    FVector CurrentPos = GetBonePosition(CurrentBone.BindPose);

    // 자식 본 찾기
    for (int32 i = 0; i < Skeleton.Bones.Num(); ++i)
    {
        if (Skeleton.Bones[i].ParentIndex == BoneIndex)
        {
            const FBone& ChildBone = Skeleton.Bones[i];
            FVector ChildGlobalPos = GetBonePosition(ChildBone.BindPose);

            // 글로벌 방향 계산
            FVector BoneDirGlobal = ChildGlobalPos - CurrentPos;
            float BoneLength = BoneDirGlobal.Size();

            if (BoneLength > KINDA_SMALL_NUMBER)
            {
                BoneDirGlobal = BoneDirGlobal / BoneLength;  // 정규화

                // 글로벌 방향을 로컬 공간으로 변환 (회전만 적용)
                OutBoneLocalDir = TransformDirection(CurrentBone.InverseBindPose, BoneDirGlobal);
                OutBoneLocalDir = OutBoneLocalDir.GetNormalized();
            }
            else
            {
                OutBoneLocalDir = FVector(0, 1, 0);  // 기본 방향
            }

            return BoneLength;
        }
    }

    // 자식이 없으면 (말단 본) 기본값
    OutBoneLocalDir = FVector::Zero();
    return 0.0f;  // 말단 본은 캡슐 생성하지 않음
}

// 캡슐 회전 계산: 본 방향으로 정렬하는 오일러 각도 (도 단위)
// 주의: 디버그 렌더러/PhysX가 이미 BaseRotation(-90, 0, 0)을 적용하므로 Y축 기준으로 계산
static FVector CalculateCapsuleRotation(const FVector& BoneLocalDir)
{
    // 캡슐 기본 축: Y축 (BaseRotation 적용 후)
    FVector DefaultAxis(0, 1, 0);

    FQuat Rotation = FQuat::FindBetweenVectors(DefaultAxis, BoneLocalDir);

    // 쿼터니언 → 오일러 각도 (ZYX 순서, 도 단위)
    return Rotation.ToEulerZYXDeg();
}

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

    // ===== 본 크기 기반 캡슐 파라미터 (언리얼 방식) =====
    const float MinBoneSize = 0.03f;    // 최소 본 크기 (3cm 이하는 무시) - Neck 포함하도록
    const float MaxBoneSize = 0.50f;    // 최대 본 크기 (50cm 초과는 클램프)
    const float RadiusRatio = 0.3f;     // 반지름 = 길이의 30%
    const float LengthRatio = 0.8f;     // 캡슐 길이 = 본 길이의 80%

    // 제외할 본 키워드 (손가락, 발가락 등 세부 본)
    auto ShouldSkipBone = [](const FString& BoneName) -> bool {
        // 손가락 관련
        if (BoneName.find("Thumb") != FString::npos) return true;
        if (BoneName.find("Index") != FString::npos) return true;
        if (BoneName.find("Middle") != FString::npos) return true;
        if (BoneName.find("Ring") != FString::npos) return true;
        if (BoneName.find("Pinky") != FString::npos) return true;
        // 발가락 관련
        if (BoneName.find("Toe") != FString::npos) return true;
        // 말단 본
        if (BoneName.find("_End") != FString::npos) return true;
        if (BoneName.find("Top_End") != FString::npos) return true;
        return false;
    };

    // ===== Phase 1: Bodies 생성 =====
    for (int32 i = 0; i < Skeleton.Bones.Num(); ++i)
    {
        const FBone& Bone = Skeleton.Bones[i];

        // 0. 손가락/발가락 등 세부 본은 스킵
        if (ShouldSkipBone(Bone.Name))
        {
            UE_LOG("Skipping bone '%s': filtered by name", Bone.Name.c_str());
            continue;
        }

        // 1. 본 길이 및 본의 로컬 방향 계산
        FVector BoneLocalDir;
        float BoneLength = CalculateBoneLength(Skeleton, i, BoneLocalDir);

        // 2. 너무 작은 본이나 말단 본은 건너뛰기
        if (BoneLength < MinBoneSize)
        {
            UE_LOG("Skipping bone '%s': length %.2f < min %.2f", Bone.Name.c_str(), BoneLength, MinBoneSize);
            continue;
        }

        // 3. 비정상적으로 큰 본 길이는 클램프
        // Head 본은 _End 마커까지의 거리가 비정상적으로 크므로 특별히 작은 크기 사용
        float EffectiveMaxSize = MaxBoneSize;
        if (Bone.Name.find("Head") != FString::npos && Bone.Name.find("Top") == FString::npos)
        {
            EffectiveMaxSize = 0.12f;  // 머리는 최대 12cm
        }

        float ClampedLength = std::min(BoneLength, EffectiveMaxSize);
        if (BoneLength > EffectiveMaxSize)
        {
            UE_LOG("Clamping bone '%s': length %.2f -> %.2f", Bone.Name.c_str(), BoneLength, ClampedLength);
        }

        // 4. 캡슐 크기 계산
        FKSphylElem Capsule;
        Capsule.Name = FName(Bone.Name);
        Capsule.Radius = ClampedLength * RadiusRatio;
        Capsule.Length = ClampedLength * LengthRatio;

        // 5. 캡슐 위치: 본 세그먼트 중앙 (로컬 Z축 방향)
        // ClampedLength 사용 (BoneLength가 클램프된 경우 대비)
        Capsule.Center = FVector(0, 0, ClampedLength * 0.5f);

        // 6. 캡슐 회전: -Z축(BaseRotation 후) → +Z축(본 방향)
        // BaseRotation(-90)이 Y→-Z로 만들므로, 180도 추가 회전으로 +Z로 뒤집음
        Capsule.Rotation = FVector(180.0f, 0.0f, 0.0f);

        UE_LOG("Bone '%s': len=%.2f, dir=(%.2f,%.2f,%.2f), center=(%.2f,%.2f,%.2f)",
               Bone.Name.c_str(), BoneLength,
               BoneLocalDir.X, BoneLocalDir.Y, BoneLocalDir.Z,
               Capsule.Center.X, Capsule.Center.Y, Capsule.Center.Z);

        UBodySetup* BodySetup = new UBodySetup();
        BodySetup->BoneName = FName(Bone.Name);
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
    // BodySetup이 있는 본들의 이름 집합 생성
    TSet<FString> BonesWithBodies;
    for (const UBodySetup* Body : PhysicsAsset->Bodies)
    {
        if (Body)
        {
            BonesWithBodies.insert(Body->BoneName.ToString());
        }
    }

    // 각 자식 본과 부모 본 사이에 Constraint 생성
    for (int32 i = 0; i < Skeleton.Bones.Num(); ++i)
    {
        const FBone& ChildBone = Skeleton.Bones[i];
        int32 ParentIndex = ChildBone.ParentIndex;

        // 루트 본은 부모가 없으므로 스킵
        if (ParentIndex < 0 || ParentIndex >= Skeleton.Bones.Num())
            continue;

        const FBone& ParentBone = Skeleton.Bones[ParentIndex];

        // 두 본 모두 BodySetup이 있어야 Constraint 생성
        if (BonesWithBodies.find(ChildBone.Name) == BonesWithBodies.end() ||
            BonesWithBodies.find(ParentBone.Name) == BonesWithBodies.end())
        {
            continue;
        }

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
