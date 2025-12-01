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

// 대소문자 무시 문자열 포함 검사 (범용 스켈레톤 지원)
static bool ContainsIgnoreCase(const FString& Str, const FString& Pattern)
{
    FString LowerStr = Str;
    FString LowerPattern = Pattern;
    std::transform(LowerStr.begin(), LowerStr.end(), LowerStr.begin(),
        [](unsigned char c) { return std::tolower(c); });
    std::transform(LowerPattern.begin(), LowerPattern.end(), LowerPattern.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return LowerStr.find(LowerPattern) != FString::npos;
}

// 손가락/발가락/말단 본 필터링 (대소문자 무시, 범용)
static bool ShouldSkipBone(const FString& BoneName)
{
    // 손가락 관련 (다양한 스켈레톤 형식 지원)
    if (ContainsIgnoreCase(BoneName, "thumb")) return true;
    if (ContainsIgnoreCase(BoneName, "index")) return true;
    if (ContainsIgnoreCase(BoneName, "middle")) return true;
    if (ContainsIgnoreCase(BoneName, "ring")) return true;
    if (ContainsIgnoreCase(BoneName, "pinky")) return true;
    if (ContainsIgnoreCase(BoneName, "finger")) return true;
    if (ContainsIgnoreCase(BoneName, "digit")) return true;

    // 발가락 관련
    if (ContainsIgnoreCase(BoneName, "toe")) return true;

    // 말단 본 마커 (다양한 형식)
    if (ContainsIgnoreCase(BoneName, "_end")) return true;
    if (ContainsIgnoreCase(BoneName, ".end")) return true;
    if (ContainsIgnoreCase(BoneName, "_tip")) return true;
    if (ContainsIgnoreCase(BoneName, "nub")) return true;

    return false;
}

// Hand 본 감지 (대소문자 무시, 범용)
// Hand 본은 자식(손가락)까지 거리가 짧아서 고정 크기 캡슐 필요
static bool IsHandBone(const FString& BoneName)
{
    // "Hand"가 포함되어 있고 손가락 본이 아닌 경우
    // 예: LeftHand, RightHand, hand_l, hand_r, Hand.L, Hand.R
    if (ContainsIgnoreCase(BoneName, "hand"))
    {
        // 손가락 본은 제외 (예: LeftHandThumb1)
        if (ShouldSkipBone(BoneName)) return false;
        return true;
    }
    return false;
}

// Head 본 감지 (대소문자 무시, 범용)
// Head 본은 구형에 가깝게 처리 (길이 제한)
static bool IsHeadBone(const FString& BoneName)
{
    // "Head"가 포함되어 있고 말단 본이 아닌 경우
    // 예: Head, head, HEAD
    if (ContainsIgnoreCase(BoneName, "head"))
    {
        // HeadTop_End 같은 말단 본은 제외
        if (ShouldSkipBone(BoneName)) return false;
        return true;
    }
    return false;
}

// Spine/Pelvis 본 감지 (대소문자 무시, 범용)
// 가로 캡슐로 회전해야 하는 본들
static bool IsSpineOrPelvisBone(const FString& BoneName)
{
    if (ContainsIgnoreCase(BoneName, "spine")) return true;
    if (ContainsIgnoreCase(BoneName, "pelvis")) return true;
    if (ContainsIgnoreCase(BoneName, "hips")) return true;
    return false;
}

// Neck 본 감지 (대소문자 무시, 범용)
static bool IsNeckBone(const FString& BoneName)
{
    if (ContainsIgnoreCase(BoneName, "neck")) return true;
    return false;
}

// Shoulder/Clavicle 본 감지 (대소문자 무시, 범용)
// 가슴과 팔을 연결하는 본 - 충돌 방지를 위해 반지름 키움
static bool IsShoulderBone(const FString& BoneName)
{
    if (ContainsIgnoreCase(BoneName, "shoulder")) return true;
    if (ContainsIgnoreCase(BoneName, "clavicle")) return true;
    return false;
}

// Foot 본 감지 (대소문자 무시, 범용)
static bool IsFootBone(const FString& BoneName)
{
    if (ContainsIgnoreCase(BoneName, "foot")) return true;
    if (ContainsIgnoreCase(BoneName, "ankle")) return true;
    return false;
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
                OutBoneLocalDir = FVector(0, 0, 1);  // 기본 방향
            }

            return BoneLength;
        }
    }

    // 자식이 없으면 (말단 본) 기본값
    OutBoneLocalDir = FVector(0, 0, 1);
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

    // ===== 본 크기 기반 캡슐 파라미터 (범용) =====
    const float MinBoneSize = 0.03f;    // 최소 본 크기 (3cm 이하는 무시)
    const float MaxBoneSize = 0.50f;    // 최대 본 크기 (50cm 초과는 클램프)
    const float RadiusRatio = 0.25f;    // 반지름 = 길이의 25% (팔다리)
    const float LengthRatio = 0.8f;     // 캡슐 길이 = 본 길이의 80%

    // Hand 본 고정 크기 (손가락까지 거리가 짧아서 별도 처리)
    const float HandBoneSize = 0.15f;   // 15cm 고정 크기
    const float HandRadiusRatio = 0.4f; // Hand 반지름 = 길이의 40%

    // Head 본 특별 처리 (구형에 가깝게, 위로 올림)
    const float HeadMaxLength = 0.07f;  // 머리 길이 최대 7cm
    const float HeadRadiusRatio = 1.2f; // Head 반지름 = 길이의 120% (더 키움)
    const float HeadCenterOffset = 0.03f; // 머리 캡슐 위로 3cm 올림

    // Foot 본 특별 처리 (발은 좀 더 두껍게)
    const float FootRadiusRatio = 0.4f; // Foot 반지름 = 길이의 40%

    // Spine/Pelvis 본 특별 처리 (가로로 넓게)
    const float SpineRadiusRatio = 0.5f;  // Spine 반지름 = 길이의 50%
    const float SpineLengthRatio = 1.2f;  // Spine 캡슐 길이 = 본 길이의 120%

    // Neck 본 특별 처리
    const float NeckRadiusRatio = 0.7f;   // Neck 반지름 = 길이의 70%
    const float NeckLengthRatio = 1.2f;   // Neck 캡슐 길이 = 본 길이의 120%

    // Shoulder/Clavicle 본 특별 처리 (가슴-팔 연결부)
    const float ShoulderRadiusRatio = 0.3f;  // Shoulder 반지름 = 길이의 30% (줄임)

    // ===== Phase 1: Bodies 생성 =====
    for (int32 i = 0; i < Skeleton.Bones.Num(); ++i)
    {
        const FBone& Bone = Skeleton.Bones[i];

        // 0. 손가락/발가락/말단 본은 스킵 (대소문자 무시, 범용)
        if (ShouldSkipBone(Bone.Name))
        {
            UE_LOG("Skipping bone '%s': filtered by name", Bone.Name.c_str());
            continue;
        }

        // 1. 본 길이 및 본의 로컬 방향 계산
        FVector BoneLocalDir;
        float BoneLength = CalculateBoneLength(Skeleton, i, BoneLocalDir);

        // 1.5. Hand 본 특별 처리 (손가락까지 거리가 짧아서 고정 크기 사용)
        bool bIsHandBone = IsHandBone(Bone.Name);
        if (bIsHandBone)
        {
            BoneLength = HandBoneSize;  // 고정 크기 사용
            UE_LOG("Hand bone '%s': using fixed size %.3f", Bone.Name.c_str(), HandBoneSize);
        }

        // 1.6. Head 본 특별 처리 (길이 제한, 구형에 가깝게)
        bool bIsHeadBone = IsHeadBone(Bone.Name);
        if (bIsHeadBone && BoneLength > HeadMaxLength)
        {
            UE_LOG("Head bone '%s': clamping length %.3f -> %.3f", Bone.Name.c_str(), BoneLength, HeadMaxLength);
            BoneLength = HeadMaxLength;
        }

        // 2. 너무 작은 본이나 말단 본은 건너뛰기 (Hand 본은 제외)
        if (BoneLength < MinBoneSize && !bIsHandBone)
        {
            UE_LOG("Skipping bone '%s': length %.3f < min %.3f", Bone.Name.c_str(), BoneLength, MinBoneSize);
            continue;
        }

        // 3. 비정상적으로 큰 본 길이는 클램프
        float ClampedLength = std::min(BoneLength, MaxBoneSize);
        if (BoneLength > MaxBoneSize)
        {
            UE_LOG("Clamping bone '%s': length %.2f -> %.2f", Bone.Name.c_str(), BoneLength, ClampedLength);
        }

        // 4. 캡슐 크기 계산
        FKSphylElem Capsule;
        Capsule.Name = FName(Bone.Name);

        // 본 타입별 비율 적용
        bool bIsSpineBone = IsSpineOrPelvisBone(Bone.Name);
        bool bIsNeckBone = IsNeckBone(Bone.Name);
        bool bIsShoulderBone = IsShoulderBone(Bone.Name);
        bool bIsFootBone = IsFootBone(Bone.Name);

        float EffectiveRadiusRatio = RadiusRatio;
        float EffectiveLengthRatio = LengthRatio;

        if (bIsHandBone)
        {
            EffectiveRadiusRatio = HandRadiusRatio;
        }
        else if (bIsHeadBone)
        {
            EffectiveRadiusRatio = HeadRadiusRatio;
        }
        else if (bIsSpineBone)
        {
            EffectiveRadiusRatio = SpineRadiusRatio;
            EffectiveLengthRatio = SpineLengthRatio;
        }
        else if (bIsNeckBone)
        {
            EffectiveRadiusRatio = NeckRadiusRatio;
            EffectiveLengthRatio = NeckLengthRatio;
        }
        else if (bIsShoulderBone)
        {
            EffectiveRadiusRatio = ShoulderRadiusRatio;
        }
        else if (bIsFootBone)
        {
            EffectiveRadiusRatio = FootRadiusRatio;
        }

        Capsule.Radius = ClampedLength * EffectiveRadiusRatio;
        Capsule.Length = ClampedLength * EffectiveLengthRatio;

        // 5. 캡슐 위치: 본 세그먼트 중앙 (로컬 Z축 방향)
        // ClampedLength 사용 (BoneLength가 클램프된 경우 대비)
        Capsule.Center = FVector(0, 0, ClampedLength * 0.5f);

        // Head는 위로 올려서 가슴과 간섭 방지
        if (bIsHeadBone)
        {
            Capsule.Center.Z += HeadCenterOffset;
        }

        // 6. 캡슐 회전
        if (bIsSpineBone)
        {
            // Spine/Pelvis: 가로 캡슐
            Capsule.Rotation = FVector(180.0f, 90.0f, 90.0f);
        }
        else
        {
            // 기본: 세로 캡슐
            Capsule.Rotation = FVector(180.0f, 0.0f, 0.0f);
        }

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
