#pragma once
#include "SkinnedMeshComponent.h"
#include "PrePhysics.h"
#include "USkeletalMeshComponent.generated.h"

class UAnimInstance;
class UAnimationAsset;
class UAnimSequence;
class UAnimStateMachineInstance;
class UAnimBlendSpaceInstance;
class UPhysicsAsset;
struct FBodyInstance;
struct FConstraintInstance;

UCLASS(DisplayName="스켈레탈 메시 컴포넌트", Description="스켈레탈 메시를 렌더링하는 컴포넌트입니다")
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    GENERATED_REFLECTION_BODY()
    
    USkeletalMeshComponent();
    ~USkeletalMeshComponent() override;

    // 복제 시 래그돌 Bodies 정리 (원본과 공유 방지)
    void DuplicateSubObjects() override;

    void BeginPlay() override;

    void TickComponent(float DeltaTime) override;

    void PrePhysicsUpdate(float DeltaTime) override;

    void SetSkeletalMesh(const FString& PathFileName) override;

    FTransform GetSocketTransform(FName InSocketName) const override;

    // Animation Integration
public:
    void SetAnimInstance(class UAnimInstance* InInstance);
    UAnimInstance* GetAnimInstance() const { return AnimInstance; }

    // Convenience single-clip controls (optional)
    void PlayAnimation(class UAnimationAsset* Asset, bool bLooping = true, float InPlayRate = 1.f);
    void StopAnimation();
    void SetAnimationPosition(float InSeconds);
    float GetAnimationPosition();
    bool IsPlayingAnimation() const;

    void UpdateAnimation(float DeltaTime);

    void SetRagdollState(bool InState);

    void ApplyPhysicsResult() override;

    //==== Minimal Lua-friendly helper to switch to a state machine anim instance ====
    UFUNCTION(LuaBind, DisplayName="UseStateMachine")
    void UseStateMachine();

    UFUNCTION(LuaBind, DisplayName="GetOrCreateStateMachine")
    UAnimStateMachineInstance* GetOrCreateStateMachine();

    //==== Minimal Lua-friendly helper to switch to a blend space 2D anim instance ====
    UFUNCTION(LuaBind, DisplayName="UseBlendSpace2D")
    void UseBlendSpace2D();

    UFUNCTION(LuaBind, DisplayName="GetOrCreateBlendSpace2D")
    UAnimBlendSpaceInstance* GetOrCreateBlendSpace2D();
    
    UPROPERTY()
    bool bIsCar = false;
// Editor Section
public:
    /**
     * @brief 특정 뼈의 부모 기준 로컬 트랜스폼을 설정
     * @param BoneIndex 수정할 뼈의 인덱스
     * @param NewLocalTransform 새로운 부모 기준 로컬 FTransform
     */
    void SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform);

    void SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform);
    
    /**
     * @brief 특정 뼈의 현재 로컬 트랜스폼을 반환
     */
    FTransform GetBoneLocalTransform(int32 BoneIndex) const;
    
    /**
     * @brief 기즈모를 렌더링하기 위해 특정 뼈의 월드 트랜스폼을 계산
     */
    FTransform GetBoneWorldTransform(int32 BoneIndex);

    /**
     * @brief 애니메이션 포즈 위에 추가적인(additive) 트랜스폼을 적용 (by 사용자 조작)
     * @param AdditiveTransforms BoneIndex -> Additive FTransform 맵
     */
    void ApplyAdditiveTransforms(const TMap<int32, FTransform>& AdditiveTransforms);

    /**
     * @brief CurrentLocalSpacePose를 RefPose로 리셋하고 포즈를 재계산
     * 애니메이션이 없는 뷰어에서 additive transform 적용 전에 호출
     */
    void ResetToRefPose();

    /**
     * @brief CurrentLocalSpacePose를 BaseAnimationPose로 리셋
     * 애니메이션이 있는 뷰어에서 additive transform 적용 전에 호출
     */
    void ResetToAnimationPose();

    TArray<FTransform> RefPose;
    TArray<FTransform> BaseAnimationPose;

    // Notify
    void TriggerAnimNotify(const FAnimNotifyEvent& NotifyEvent);

protected:
    /**
     * @brief CurrentLocalSpacePose의 변경사항을 ComponentSpace -> FinalMatrices 계산까지 모두 수행
     */
    void ForceRecomputePose();

    /**
     * @brief CurrentLocalSpacePose를 기반으로 CurrentComponentSpacePose 채우기
     */
    void UpdateComponentSpaceTransforms();

    /**
     * @brief CurrentComponentSpacePose를 기반으로 TempFinalSkinningMatrices 채우기
     */
    void UpdateFinalSkinningMatrices();

protected:
    /**
     * @brief 각 뼈의 부모 기준 로컬 트랜스폼
     */
    TArray<FTransform> CurrentLocalSpacePose;

    /**
     * @brief LocalSpacePose로부터 계산된 컴포넌트 기준 트랜스폼
     */
    TArray<FTransform> CurrentComponentSpacePose;

    /**
     * @brief 부모에게 보낼 최종 스키닝 행렬 (임시 계산용)
     */
    TArray<FMatrix> TempFinalSkinningMatrices;

// FOR TEST!!!
private:
    float TestTime = 0;
    bool bIsInitialized = false;
    FTransform TestBoneBasePose;

    uint64 LastFrameCount = 0;

    // Animation state
    UAnimInstance* AnimInstance = nullptr;
    bool bUseAnimation = true;

// ===== 래그돌 시스템 (언리얼 방식) =====
public:
    // 과제 요구사항: UActorComponent::CreatePhysicsState 오버라이드
    void CreatePhysicsState() override;
    void DestroyPhysicsState() override;

    // 래그돌 초기화 (PhysicsAsset 지정)
    void InitArticulated(UPhysicsAsset* PhysAsset);

    // ===== Physics Asset 경로 오버라이드 시스템 =====
    // 에디터에서 수정 가능한 PhysicsAsset 경로
    // 비어있으면 SkeletalMesh의 기본값 사용
    UPROPERTY(EditAnywhere, Category = "Physics", DisplayName = "Physics Asset 경로")
    FString PhysicsAssetPathOverride;

    // 실제 사용할 PhysicsAsset 경로 반환 (오버라이드 > 기본값)
    FString GetEffectivePhysicsAssetPath() const;

    // PhysicsAsset 오버라이드 경로 설정 (캐시 무효화 포함)
    void SetPhysicsAssetPathOverride(const FString& NewPath);

    // 실제 사용할 PhysicsAsset 반환 (오버라이드 경로 > SkeletalMesh 기본값)
    UPhysicsAsset* GetEffectivePhysicsAsset();

    // PhysicsAsset 직접 설정 (에디터 미리보기/디버그 렌더링용)
    void SetPhysicsAssetPreview(UPhysicsAsset* InAsset) { PhysicsAsset = InAsset; }

    // 래그돌 활성화/비활성화 (부모 UPrimitiveComponent의 bSimulatePhysics 사용)
    void SetSimulatePhysics(bool bEnable);
    bool IsSimulatingPhysics() const { return bIsRagdoll; }

    // Bodies/Constraints 접근자 (디버그 렌더링용, 에디터 모드에서 lazy 초기화)
    const TArray<FBodyInstance*>& GetBodies();
    const TArray<FConstraintInstance*>& GetConstraints() const { return Constraints; }
    const TArray<int32>& GetBodyParentIndices() const { return BodyParentIndices; }
    const TArray<int32>& GetBodyBoneIndices() const { return BodyBoneIndices; }

private:
    // 과제 요구사항: USkeletalMeshComponent::InstantiatePhysicsAssetBodies_Internal
    void InstantiatePhysicsAssetBodies_Internal(UPhysicsAsset* PhysAsset, const TArray<FTransform>& BoneWorldTransforms);
    void CreateConstraints(UPhysicsAsset* PhysAsset);
    void SetupBoneHierarchy();

    // 물리 -> 렌더링 동기화
    void SyncBodiesToBones();

    // 본 이름으로 Bodies 인덱스 찾기
    int32 FindBodyIndex(const FName& BoneName) const;

private:
    // 래그돌 물리 데이터 (과제 요구사항: PxShape/PxRigidActor = FBodyInstance, PxJoint = FConstraintInstance)
    TArray<FBodyInstance*> Bodies;
    TArray<FConstraintInstance*> Constraints;
    TArray<int32> BodyBoneIndices;		// Bodies[i]에 대응하는 스켈레톤 본 인덱스
    TArray<int32> BodyParentIndices;	// Bodies[i]의 부모 Body 인덱스 (-1이면 루트)

    // 래그돌 상태 (bSimulatePhysics는 부모 UPrimitiveComponent에서 상속)
    UPhysicsAsset* PhysicsAsset = nullptr;
    bool bIsRagdoll = false;

    // PIE 중 bSimulatePhysics 프로퍼티 변경 감지용
    bool bPrevSimulatePhysics = false;
};
