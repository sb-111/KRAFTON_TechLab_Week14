#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "PlatformTime.h"
#include "AnimSequence.h"
#include "FbxLoader.h"
#include "AnimNodeBase.h"
#include "AnimSingleNodeInstance.h"
#include "AnimStateMachineInstance.h"
#include "AnimBlendSpaceInstance.h"

// 래그돌 관련
#include "BodyInstance.h"
#include "PhysicsAsset.h"
#include "BodySetup.h"
#include "ConstraintInstance.h"
#include "PhysicsSystem.h"
#include "PhysxConverter.h"
#include "PhysicsScene.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.h"

using namespace physx;

USkeletalMeshComponent::USkeletalMeshComponent()
{
    // Keep constructor lightweight for editor/viewer usage.
    // Load a simple default test mesh if available; viewer UI can override.
    SetSkeletalMesh(GDataDir + "/DancingRacer.fbx");
    // TODO - 애니메이션 나중에 써먹으세요
    /*
	UAnimationAsset* AnimationAsset = UResourceManager::GetInstance().Get<UAnimSequence>("Data/DancingRacer_mixamo.com");
    PlayAnimation(AnimationAsset, true, 1.f);
    */
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    // 래그돌 Bodies/Constraints 정리 (메모리 누수 방지)
    DestroyPhysicsState();
}

void USkeletalMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    // 래그돌 관련 데이터 초기화 (원본과 공유 방지)
    // 얕은 복사된 Bodies/Constraints 포인터들은 원본의 PhysX 객체를 가리키므로
    // 복제된 컴포넌트는 자신만의 Bodies를 새로 생성해야 함
    Bodies.Empty();
    Constraints.Empty();
    BodyBoneIndices.Empty();
    BodyParentIndices.Empty();
    PhysicsAsset = nullptr;
    bIsRagdoll = false;
}

void USkeletalMeshComponent::BeginPlay()
{
    Super::BeginPlay();

    // PIE 모드에서 PhysicsAsset이 있으면 래그돌 자동 활성화
    UWorld* World = GetWorld();
    if (World && World->bPie)
    {
        UPhysicsAsset* Asset = GetEffectivePhysicsAsset();
        if (Asset)
        {
            // 중요: InitArticulated 전에 bSimulatePhysics를 true로 설정
            // FBodyInstance::InitBody에서 이 값을 참조하여 키네마틱 여부 결정
            bSimulatePhysics = true;

            // Bodies가 비어있으면 초기화
            if (Bodies.IsEmpty())
            {
                InitArticulated(Asset);
            }

            // Bodies가 있으면 시뮬레이션 시작
            if (!Bodies.IsEmpty())
            {
                SetSimulatePhysics(true);
                SetRagdollState(true);
            }
        }
    }

    // Physics Asset이 있고 movable이면 PrePhysics에 등록
    // (키네마틱이든 래그돌이든 시뮬레이션 전에 계산해야 함)
    UPhysicsAsset* EffectiveAsset = GetEffectivePhysicsAsset();
    if (EffectiveAsset && MobilityType == EMobilityType::Movable)
    {
        GWorld->GetPhysicsScene()->RegisterPrePhysics(this);
    }
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    UpdateAnimation(DeltaTime);
}

void USkeletalMeshComponent::PrePhysicsUpdate(float DeltaTime)
{
    Super::PrePhysicsUpdate(DeltaTime);
    UpdateAnimation(DeltaTime);
}

void USkeletalMeshComponent::SetSkeletalMesh(const FString& PathFileName)
{
    Super::SetSkeletalMesh(PathFileName);

    if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshData())
    {
        const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
        const int32 NumBones = Skeleton.Bones.Num();

        CurrentLocalSpacePose.SetNum(NumBones);
        CurrentComponentSpacePose.SetNum(NumBones);
        TempFinalSkinningMatrices.SetNum(NumBones);

        for (int32 i = 0; i < NumBones; ++i)
        {
            const FBone& ThisBone = Skeleton.Bones[i];
            const int32 ParentIndex = ThisBone.ParentIndex;
            FMatrix LocalBindMatrix;

            if (ParentIndex == -1) // 루트 본
            {
                LocalBindMatrix = ThisBone.BindPose;
            }
            else // 자식 본
            {
                const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
                LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
            }
            // 계산된 로컬 행렬을 로컬 트랜스폼으로 변환
            CurrentLocalSpacePose[i] = FTransform(LocalBindMatrix); 
        }
        RefPose = CurrentLocalSpacePose;
        ForceRecomputePose();

        // Rebind anim instance to new skeleton
        if (AnimInstance)
        {
            AnimInstance->InitializeAnimation(this);
        }
    }
    else
    {
        // 메시 로드 실패 시 버퍼 비우기
        CurrentLocalSpacePose.Empty();
        CurrentComponentSpacePose.Empty();
        TempFinalSkinningMatrices.Empty();
    }
}

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InInstance)
{
    AnimInstance = InInstance;
    if (AnimInstance)
    {
        AnimInstance->InitializeAnimation(this);
    }
}

void USkeletalMeshComponent::PlayAnimation(UAnimationAsset* Asset, bool bLooping, float InPlayRate)
{
    UAnimSingleNodeInstance* Single = nullptr;
    if (!AnimInstance)
    {
        Single = NewObject<UAnimSingleNodeInstance>();
        SetAnimInstance(Single);
    }
    else
    {
        Single = Cast<UAnimSingleNodeInstance>(AnimInstance);
        if (!Single)
        {
            // Replace with a single-node instance for simple playback
            // 기존 AnimInstance를 먼저 삭제
            ObjectFactory::DeleteObject(AnimInstance);
            AnimInstance = nullptr;

            Single = NewObject<UAnimSingleNodeInstance>();
            SetAnimInstance(Single);
        }
    }

    if (Single)
    {
        Single->SetAnimationAsset(Asset, bLooping);
        Single->SetPlayRate(InPlayRate);
        Single->Play(true);
    }
}

// ==== Lua-friendly State Machine helper: switch this component to a state machine anim instance ====
void USkeletalMeshComponent::UseStateMachine()
{
    UAnimStateMachineInstance* StateMachine = Cast<UAnimStateMachineInstance>(AnimInstance);
    if (!StateMachine)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimStateMachineInstance\n");
        StateMachine = NewObject<UAnimStateMachineInstance>();
        SetAnimInstance(StateMachine);
    }
    else
    {
        UE_LOG("[SkeletalMeshComponent] AnimStateMachineInstance already exists\n");
    }
}

UAnimStateMachineInstance* USkeletalMeshComponent::GetOrCreateStateMachine()
{
    UAnimStateMachineInstance* StateMachine = Cast<UAnimStateMachineInstance>(AnimInstance);
    if (!StateMachine)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimStateMachineInstance\n");
        StateMachine = NewObject<UAnimStateMachineInstance>();
        SetAnimInstance(StateMachine);
    }
    return StateMachine;
}

// ==== Lua-friendly Blend Space helper: switch this component to a blend space 2D anim instance ====
void USkeletalMeshComponent::UseBlendSpace2D()
{
    UAnimBlendSpaceInstance* BS = Cast<UAnimBlendSpaceInstance>(AnimInstance);
    if (!BS)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimBlendSpaceInstance\n");
        BS = NewObject<UAnimBlendSpaceInstance>();
        SetAnimInstance(BS);
    }
    else
    {
        UE_LOG("[SkeletalMeshComponent] AnimBlendSpaceInstance already exists\n");
    }
}

UAnimBlendSpaceInstance* USkeletalMeshComponent::GetOrCreateBlendSpace2D()
{
    UAnimBlendSpaceInstance* BS = Cast<UAnimBlendSpaceInstance>(AnimInstance);
    if (!BS)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimBlendSpaceInstance\n");
        BS = NewObject<UAnimBlendSpaceInstance>();
        SetAnimInstance(BS);
    }
    return BS;
}

void USkeletalMeshComponent::StopAnimation()
{
    if (UAnimSingleNodeInstance* Single = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        Single->Stop();
    }
}

void USkeletalMeshComponent::SetAnimationPosition(float InSeconds)
{
    if (UAnimSingleNodeInstance* Single = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        Single->SetPosition(InSeconds, false);
    }
}

bool USkeletalMeshComponent::IsPlayingAnimation() const
{
    return AnimInstance ? AnimInstance->IsPlaying() : false;
}

void USkeletalMeshComponent::UpdateAnimation(float DeltaTime)
{
    // PrePhysicsUpdate에서 이미 계산했는데 다시 계산하는 경우 예방
    uint64 CurrentFrameCounter = GEngine.GetFrameCounter();
    if (LastFrameCount == CurrentFrameCounter || bIsRagdoll)
    {
        return;
    }
    LastFrameCount = CurrentFrameCounter;

    if (!SkeletalMesh) { return; }
    // Drive animation instance if present
    if (bUseAnimation && AnimInstance && SkeletalMesh && SkeletalMesh->GetSkeleton())
    {
        AnimInstance->NativeUpdateAnimation(DeltaTime);

        FPoseContext OutputPose;
        OutputPose.Initialize(this, SkeletalMesh->GetSkeleton(), DeltaTime);
        AnimInstance->EvaluateAnimation(OutputPose);

        // Apply local-space pose to component and rebuild skinning
        // 애니메이션 포즈를 BaseAnimationPose에 저장 (additive 적용 전 리셋용)
        BaseAnimationPose = OutputPose.LocalSpacePose;
        CurrentLocalSpacePose = OutputPose.LocalSpacePose;
        if (bSimulatePhysics)
        {
            // TODO: 피지컬 애니메이션
        }
        else
        {
            ForceRecomputePose();

            // TODO: 바디 인스턴스 애니메이션에 맞게 Physx에 업데이트
        }
    }

}

void USkeletalMeshComponent::SetRagdollState(bool InState)
{
    bSimulatePhysics = true;
    bIsRagdoll = InState;
}

void USkeletalMeshComponent::ApplyPhysicsResult()
{
    Super::ApplyPhysicsResult();
    if (bIsRagdoll && bSimulatePhysics)
    {
        SyncBodiesToBones();
    }
}

float USkeletalMeshComponent::GetAnimationPosition()
{
    if (UAnimSingleNodeInstance* Single = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        return Single->GetPosition();
    }
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        CurrentLocalSpacePose[BoneIndex] = NewLocalTransform;
        ForceRecomputePose();
    }
}

void USkeletalMeshComponent::SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform)
{
    if (BoneIndex < 0 || BoneIndex >= CurrentLocalSpacePose.Num())
        return;

    const int32 ParentIndex = SkeletalMesh->GetSkeleton()->Bones[BoneIndex].ParentIndex;

    const FTransform& ParentWorldTransform = GetBoneWorldTransform(ParentIndex);
    FTransform DesiredLocal = ParentWorldTransform.GetRelativeTransform(NewWorldTransform);

    SetBoneLocalTransform(BoneIndex, DesiredLocal);
}


FTransform USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        return CurrentLocalSpacePose[BoneIndex];
    }
    return FTransform();
}

FTransform USkeletalMeshComponent::GetBoneWorldTransform(int32 BoneIndex)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex && BoneIndex >= 0)
    {
        // 뼈의 컴포넌트 공간 트랜스폼 * 컴포넌트의 월드 트랜스폼
        return GetWorldTransform().GetWorldTransform(CurrentComponentSpacePose[BoneIndex]);
    }
    return GetWorldTransform(); // 실패 시 컴포넌트 위치 반환
}

void USkeletalMeshComponent::ForceRecomputePose()
{
    if (!SkeletalMesh) { return; } 

    // LocalSpace -> ComponentSpace 계산
    UpdateComponentSpaceTransforms();

    // ComponentSpace -> Final Skinning Matrices 계산
    // UpdateFinalSkinningMatrices()에서 UpdateSkinningMatrices()를 호출하여 본 행렬 계산 시간과 함께 전달
    UpdateFinalSkinningMatrices();
    // PerformSkinning은 CollectMeshBatches에서 전역 모드에 따라 수행됨
}

void USkeletalMeshComponent::UpdateComponentSpaceTransforms()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& LocalTransform = CurrentLocalSpacePose[BoneIndex];
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

        if (ParentIndex == -1) // 루트 본
        {
            CurrentComponentSpacePose[BoneIndex] = LocalTransform;
        }
        else // 자식 본
        {
            const FTransform& ParentComponentTransform = CurrentComponentSpacePose[ParentIndex];
            CurrentComponentSpacePose[BoneIndex] = ParentComponentTransform.GetWorldTransform(LocalTransform);
        }
    }
}

void USkeletalMeshComponent::UpdateFinalSkinningMatrices()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    // 본 행렬 계산 시간 측정 시작
    uint64 BoneMatrixCalcStart = FWindowsPlatformTime::Cycles64();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FMatrix& InvBindPose = Skeleton.Bones[BoneIndex].InverseBindPose;
        const FMatrix ComponentPoseMatrix = CurrentComponentSpacePose[BoneIndex].ToMatrix();

        TempFinalSkinningMatrices[BoneIndex] = InvBindPose * ComponentPoseMatrix;
    }

    // 본 행렬 계산 시간 측정 종료
    uint64 BoneMatrixCalcEnd = FWindowsPlatformTime::Cycles64();
    double BoneMatrixCalcTimeMS = FWindowsPlatformTime::ToMilliseconds(BoneMatrixCalcEnd - BoneMatrixCalcStart);

    // 본 행렬 계산 시간을 부모 USkinnedMeshComponent로 전달
    // 부모에서 실제 스키닝 모드(CPU/GPU)에 따라 통계에 추가됨
    UpdateSkinningMatrices(TempFinalSkinningMatrices, BoneMatrixCalcTimeMS);
}

void USkeletalMeshComponent::ApplyAdditiveTransforms(const TMap<int32, FTransform>& AdditiveTransforms)
{
    if (AdditiveTransforms.IsEmpty()) return;

    // CurrentLocalSpacePose는 TickComponent에서 이미 기본 애니메이션 포즈를 포함하고 있어야 함
    for (auto const& [BoneIndex, AdditiveTransform] : AdditiveTransforms)
    {
        if (BoneIndex >= 0 && BoneIndex < CurrentLocalSpacePose.Num())
        {
            // 회전이 위치에 영향을 주지 않도록 각 성분을 개별적으로 적용
            FTransform& BasePose = CurrentLocalSpacePose[BoneIndex];

            // 위치: 단순 덧셈
            FVector FinalLocation = BasePose.Translation + AdditiveTransform.Translation;

            // 회전: 쿼터니언 곱셈
            FQuat FinalRotation = AdditiveTransform.Rotation * BasePose.Rotation;

            // 스케일: 성분별 곱셈
            FVector FinalScale = FVector(
                BasePose.Scale3D.X * AdditiveTransform.Scale3D.X,
                BasePose.Scale3D.Y * AdditiveTransform.Scale3D.Y,
                BasePose.Scale3D.Z * AdditiveTransform.Scale3D.Z
            );

            CurrentLocalSpacePose[BoneIndex] = FTransform(FinalLocation, FinalRotation, FinalScale);
        }
    }

    // 모든 additive 적용 후 최종 포즈 재계산
    ForceRecomputePose();
}

void USkeletalMeshComponent::ResetToRefPose()
{
    CurrentLocalSpacePose = RefPose;
    ForceRecomputePose();
}

void USkeletalMeshComponent::ResetToAnimationPose()
{
    // BaseAnimationPose가 비어있으면 RefPose 사용
    if (BaseAnimationPose.IsEmpty())
    {
        CurrentLocalSpacePose = RefPose;
    }
    else
    {
        CurrentLocalSpacePose = BaseAnimationPose;
    }
    ForceRecomputePose();
}

void USkeletalMeshComponent::TriggerAnimNotify(const FAnimNotifyEvent& NotifyEvent)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Owner->HandleAnimNotify(NotifyEvent);
    }
}

// ===== 래그돌 시스템 구현 (언리얼 방식) =====

// 과제 요구사항: UActorComponent::CreatePhysicsState 오버라이드
// 물리 상태 생성 (PhysicsAsset이 설정되어 있으면 래그돌 초기화)
void USkeletalMeshComponent::CreatePhysicsState()
{
    // PhysicsAsset이 이미 설정되어 있으면 Bodies/Constraints 생성
    if (!PhysicsAsset)
    {
        PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
    }
    InitArticulated(PhysicsAsset);
}

void USkeletalMeshComponent::InitArticulated(UPhysicsAsset* PhysAsset)
{
    if (!PhysAsset || !SkeletalMesh) return;

    // 기존 래그돌 정리 (과제 요구사항: DestroyPhysicsState 사용)
    DestroyPhysicsState();

    PhysicsAsset = PhysAsset;

    // 각 본의 월드 Transform 수집
    TArray<FTransform> BoneWorldTransforms;
    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    if (Skeleton)
    {
        for (int32 i = 0; i < PhysAsset->Bodies.Num(); ++i)
        {
            UBodySetup* Setup = PhysAsset->Bodies[i];
            if (!Setup) continue;

            // BodySetup의 BoneName으로 스켈레톤에서 본 인덱스 찾기
            const auto* BoneIndexPtr = Skeleton->BoneNameToIndex.Find(Setup->BoneName.ToString());
            if (BoneIndexPtr)
            {
                BoneWorldTransforms.Add(GetBoneWorldTransform(*BoneIndexPtr));
            }
            else
            {
                BoneWorldTransforms.Add(GetWorldTransform());
            }
        }
    }

    // Bodies 생성 (과제 요구사항: InstantiatePhysicsAssetBodies_Internal 사용)
    InstantiatePhysicsAssetBodies_Internal(PhysAsset, BoneWorldTransforms);

    // 부모-자식 관계 설정
    SetupBoneHierarchy();

    // Constraints (Joint) 생성
    CreateConstraints(PhysAsset);

    UE_LOG("[Ragdoll] InitArticulated: Created %d bodies, %d constraints", Bodies.Num(), Constraints.Num());
}

// 과제 요구사항: UActorComponent::DestroyPhysicsState 오버라이드
// 래그돌 물리 상태 해제 (Joint → Body 순서로 정리)
void USkeletalMeshComponent::DestroyPhysicsState()
{
    // Joint 먼저 해제 (과제 요구사항: FConstraintInstance로 래핑)
    for (FConstraintInstance* Constraint : Constraints)
    {
        if (Constraint)
        {
            delete Constraint;  // 소멸자에서 TermConstraint 호출
        }
    }
    Constraints.Empty();

    // Body 해제
    for (FBodyInstance* Body : Bodies)
    {
        if (Body)
        {
            delete Body;
        }
    }
    Bodies.Empty();

    BodyBoneIndices.Empty();
    BodyParentIndices.Empty();
    PhysicsAsset = nullptr;
    // bSimulatePhysics는 건드리지 않음 - 물리 상태 정리와 시뮬레이션 의도는 별개
}

void USkeletalMeshComponent::SetSimulatePhysics(bool bEnable)
{
    if (bEnable && Bodies.IsEmpty())
    {
        UE_LOG("[Ragdoll] Warning: SetSimulatePhysics(true) called but no bodies exist. Call InitArticulated first.");
        return;
    }

    bSimulatePhysics = bEnable;

    // Bodies 깨우기/재우기
    for (FBodyInstance* Body : Bodies)
    {
        if (Body && Body->IsValidBodyInstance())
        {
            PxRigidDynamic* RigidBody = Body->GetPxRigidDynamic();
            if (RigidBody)
            {
                if (bEnable)
                {
                    RigidBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
                    RigidBody->wakeUp();
                }
                else
                {
                    RigidBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
                    RigidBody->putToSleep();
                }
            }
        }
    }
}

// 과제 요구사항: USkeletalMeshComponent::InstantiatePhysicsAssetBodies_Internal
// PhysicsAsset의 BodySetup들을 기반으로 FBodyInstance 배열 생성
void USkeletalMeshComponent::InstantiatePhysicsAssetBodies_Internal(UPhysicsAsset* PhysAsset, const TArray<FTransform>& BoneWorldTransforms)
{
    const FSkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;

    // 래그돌 자체 충돌 방지를 위한 고유 Owner ID 생성
    // 같은 래그돌 내 모든 Body는 동일한 ID를 공유
    static uint32 NextRagdollOwnerID = 1;
    uint32 RagdollOwnerID = NextRagdollOwnerID++;

    for (int32 i = 0; i < PhysAsset->Bodies.Num(); ++i)
    {
        UBodySetup* Setup = PhysAsset->Bodies[i];
        if (!Setup) continue;

        FTransform BodyTransform = (i < BoneWorldTransforms.Num()) ? BoneWorldTransforms[i] : FTransform();

        // 스켈레톤에서 본 인덱스 찾기
        int32 BoneIndex = -1;
        if (Skeleton)
        {
            const auto* BoneIndexPtr = Skeleton->BoneNameToIndex.Find(Setup->BoneName.ToString());
            if (BoneIndexPtr)
            {
                BoneIndex = *BoneIndexPtr;
            }
        }

        // FBodyInstance 생성 및 초기화 (RagdollOwnerID 전달)
        FBodyInstance* NewBody = new FBodyInstance();
        NewBody->InitBody(Setup, this, BodyTransform, BoneIndex, RagdollOwnerID);

        NewBody->OwnerComponent = this;
        Bodies.Add(NewBody);
        BodyBoneIndices.Add(BoneIndex);
    }
}

void USkeletalMeshComponent::SetupBoneHierarchy()
{
    const FSkeleton* Skeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;
    if (!Skeleton || !PhysicsAsset)
    {
        // 선형 계층으로 폴백
        BodyParentIndices.SetNum(Bodies.Num());
        for (int32 i = 0; i < Bodies.Num(); ++i)
        {
            BodyParentIndices[i] = (i > 0) ? i - 1 : -1;
        }
        return;
    }

    // BodySetup의 BoneName을 기반으로 Bodies 인덱스 매핑
    TMap<FName, int32> BoneNameToBodyIndex;
    for (int32 i = 0; i < PhysicsAsset->Bodies.Num(); ++i)
    {
        if (PhysicsAsset->Bodies[i])
        {
            BoneNameToBodyIndex.Add(PhysicsAsset->Bodies[i]->BoneName, i);
        }
    }

    BodyParentIndices.SetNum(Bodies.Num());

    for (int32 i = 0; i < Bodies.Num(); ++i)
    {
        BodyParentIndices[i] = -1;  // 기본값: 루트

        int32 BoneIndex = BodyBoneIndices[i];
        if (BoneIndex < 0) continue;

        // 스켈레톤 체인을 따라 올라가며 Bodies에 있는 부모 찾기
        int32 CurrentParentBoneIndex = Skeleton->Bones[BoneIndex].ParentIndex;
        while (CurrentParentBoneIndex >= 0)
        {
            const FString& ParentBoneName = Skeleton->Bones[CurrentParentBoneIndex].Name;
            const auto* BodyIndexPtr = BoneNameToBodyIndex.Find(FName(ParentBoneName));

            if (BodyIndexPtr)
            {
                BodyParentIndices[i] = *BodyIndexPtr;
                break;
            }

            CurrentParentBoneIndex = Skeleton->Bones[CurrentParentBoneIndex].ParentIndex;
        }
    }
}

void USkeletalMeshComponent::CreateConstraints(UPhysicsAsset* PhysAsset)
{
    // 과제 요구사항: PxJoint = FConstraintInstance
    // FConstraintInstance::InitConstraint()에서 Joint 생성 로직 처리

    // PhysicsAsset의 Constraints 사용 (있으면)
    if (PhysAsset->Constraints.Num() > 0)
    {
        for (const FConstraintInstance& AssetCI : PhysAsset->Constraints)
        {
            int32 ParentIdx = FindBodyIndex(AssetCI.ConstraintBone1);
            int32 ChildIdx = FindBodyIndex(AssetCI.ConstraintBone2);

            if (ParentIdx < 0 || ChildIdx < 0) continue;
            if (!Bodies[ParentIdx] || !Bodies[ChildIdx]) continue;

            // 새 FConstraintInstance 생성 (에셋 데이터 복사)
            FConstraintInstance* CI = new FConstraintInstance(AssetCI);

            // Joint 초기화 (FConstraintInstance가 PhysX Joint를 래핑)
            CI->InitConstraint(Bodies[ParentIdx], Bodies[ChildIdx], this);

            if (CI->IsValidConstraintInstance())
            {
                Constraints.Add(CI);
            }
            else
            {
                delete CI;
            }
        }
    }
    else
    {
        // Constraints가 없으면 SetupBoneHierarchy 결과 사용 (기본 45도 제한)
        for (int32 i = 0; i < Bodies.Num(); ++i)
        {
            int32 ParentIdx = BodyParentIndices[i];
            if (ParentIdx < 0) continue;

            if (!Bodies[i] || !Bodies[ParentIdx]) continue;

            // 새 FConstraintInstance 생성 (기본값 45도)
            FConstraintInstance* CI = new FConstraintInstance();
            CI->TwistLimitAngle = 45.0f;
            CI->Swing1LimitAngle = 45.0f;
            CI->Swing2LimitAngle = 45.0f;

            // Joint 초기화
            CI->InitConstraint(Bodies[ParentIdx], Bodies[i], this);

            if (CI->IsValidConstraintInstance())
            {
                Constraints.Add(CI);
            }
            else
            {
                delete CI;
            }
        }
    }
}

void USkeletalMeshComponent::SyncBodiesToBones()
{
    if (Bodies.Num() != BodyBoneIndices.Num()) return;  // 안전 검사
    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton()) return;

    const FTransform& ComponentWorldTransform = GetWorldTransform();
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    // Body가 있는 본 인덱스를 빠르게 찾기 위한 맵
    TMap<int32, int32> BoneToBodyMap;  // BoneIndex → Bodies 배열 인덱스
    for (int32 i = 0; i < Bodies.Num(); ++i)
    {
        int32 BoneIndex = BodyBoneIndices[i];
        if (BoneIndex >= 0)
        {
            BoneToBodyMap.Add(BoneIndex, i);
        }
    }

    // 본 계층 순서대로 (루트부터) ComponentSpace 계산
    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

        // 이 본에 Body가 있는지 확인
        const int32* BodyIndexPtr = BoneToBodyMap.Find(BoneIndex);

        if (BodyIndexPtr)
        {
            // Body가 있는 본: Body의 월드 Transform을 ComponentSpace로 변환
            FBodyInstance* Body = Bodies[*BodyIndexPtr];
            if (Body && Body->IsValidBodyInstance())
            {
                FTransform BodyWorldTransform = Body->GetWorldTransform();
                // GetRelativeTransform: 부모(Component)의 월드 기준으로 자식(Body)의 상대 Transform 계산
                FTransform NewComponentSpace = ComponentWorldTransform.GetRelativeTransform(BodyWorldTransform);

                // PhysX는 스케일을 지원하지 않으므로, 원래 본의 스케일 유지
                NewComponentSpace.Scale3D = CurrentComponentSpacePose[BoneIndex].Scale3D;

                CurrentComponentSpacePose[BoneIndex] = NewComponentSpace;
            }
        }
        else
        {
            // Body가 없는 본: 부모의 ComponentSpace + 원래 로컬 오프셋으로 계산
            if (ParentIndex >= 0)
            {
                const FTransform& ParentCS = CurrentComponentSpacePose[ParentIndex];
                const FTransform& LocalPose = CurrentLocalSpacePose[BoneIndex];
                CurrentComponentSpacePose[BoneIndex] = ParentCS.GetWorldTransform(LocalPose);
            }
            // 루트인데 Body가 없으면 현재 값 유지
        }
    }

    // ComponentSpace → LocalSpace 역계산
    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
        if (ParentIndex == -1)
        {
            CurrentLocalSpacePose[BoneIndex] = CurrentComponentSpacePose[BoneIndex];
        }
        else
        {
            const FTransform& ParentCS = CurrentComponentSpacePose[ParentIndex];
            CurrentLocalSpacePose[BoneIndex] = ParentCS.GetRelativeTransform(CurrentComponentSpacePose[BoneIndex]);
        }
    }

    // 최종 스키닝 매트릭스 업데이트
    UpdateFinalSkinningMatrices();
}

int32 USkeletalMeshComponent::FindBodyIndex(const FName& BoneName) const
{
    if (!PhysicsAsset) return -1;

    for (int32 i = 0; i < PhysicsAsset->Bodies.Num(); ++i)
    {
        if (PhysicsAsset->Bodies[i] && PhysicsAsset->Bodies[i]->BoneName == BoneName)
        {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// Physics Asset 경로 오버라이드 시스템
// ============================================================================

FString USkeletalMeshComponent::GetEffectivePhysicsAssetPath() const
{
    // 1. 컴포넌트 오버라이드가 있으면 그것 사용
    if (!PhysicsAssetPathOverride.empty())
    {
        return PhysicsAssetPathOverride;
    }

    // 2. SkeletalMesh의 기본 경로 사용
    if (SkeletalMesh)
    {
        return SkeletalMesh->GetPhysicsAssetPath();
    }

    return "";
}

UPhysicsAsset* USkeletalMeshComponent::GetEffectivePhysicsAsset()
{
    // 이미 캐시된 PhysicsAsset이 있으면 반환
    if (PhysicsAsset)
    {
        return PhysicsAsset;
    }

    // 1. 오버라이드 경로가 있으면 해당 경로에서 로드 (한 번만)
    if (!PhysicsAssetPathOverride.empty())
    {
        PhysicsAsset = PhysicsAssetEditorBootstrap::LoadPhysicsAsset(PhysicsAssetPathOverride, nullptr);
        return PhysicsAsset;
    }

    // 2. SkeletalMesh의 기본 PhysicsAsset 사용
    if (SkeletalMesh)
    {
        return SkeletalMesh->GetPhysicsAsset();
    }

    return nullptr;
}

const TArray<FBodyInstance*>& USkeletalMeshComponent::GetBodies()
{
    UWorld* MyWorld = GetWorld();

    // 에디터 모드 + PIE 비활성일 때만 lazy 초기화
    // (PIE 중에는 재생성 안 함 - StartPIE에서 정리했고, PIE Scene에 잘못 생성되면 안됨)
    if (MyWorld && !MyWorld->bPie && !GEngine.IsPIEActive())
    {
        if (Bodies.IsEmpty())
        {
            UPhysicsAsset* Asset = GetEffectivePhysicsAsset();
            if (Asset)
            {
                bool bPrevSimulate = bSimulatePhysics;
                bSimulatePhysics = false;
                InitArticulated(Asset);
                bSimulatePhysics = bPrevSimulate;
            }
        }
    }

    return Bodies;
}