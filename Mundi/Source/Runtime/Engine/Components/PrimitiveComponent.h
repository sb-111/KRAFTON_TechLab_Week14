#pragma once

#include "SceneComponent.h"
#include "Material.h"
#include "UPrimitiveComponent.generated.h"
#include "BodyInstance.h"
#include "PrePhysics.h"

// 전방 선언
struct FSceneCompData;

class URenderer;
struct FMeshBatchElement;
class FSceneView;

struct FOverlapInfo
{
    AActor* OtherActor = nullptr;
    UPrimitiveComponent* Other = nullptr;
};

UCLASS(DisplayName="프리미티브 컴포넌트", Description="렌더링 가능한 기본 컴포넌트입니다")
class UPrimitiveComponent :public USceneComponent, public IPrePhysics
{
public:
    GENERATED_REFLECTION_BODY()

public:

    // ===== Lua-Bindable Properties (Auto-moved from protected/private) =====

    UPROPERTY(EditAnywhere, Category="Shape")
    bool bGenerateOverlapEvents;

    UPROPERTY(EditAnywhere, Category="Shape")
    bool bBlockComponent;

    // ───── 충돌 관련 ────────────────────────────

    // 부모가 Movable인데 자식 Static 불가능
    // 부모가 Static인데 자식 Movable 가능
    // 둘다 static, 둘다 movable 가능.
    UPROPERTY(EditAnywhere, Category = "Physics")
    EMobilityType MobilityType = EMobilityType::Static;

    UPROPERTY(EditAnywhere, Category = "Physics")
    ECollisionEnabled CollisionType = ECollisionEnabled::None;

    // 독립적인 물리 액터인지 결정
    // 키네마틱 여부(false인 경우 정해진 동작만 수행함)
    // 기본값 false: 래그돌은 SkeletalMeshComponent의 BeginPlay에서 처리
    UPROPERTY(EditAnywhere, Category = "Physics")
    bool bSimulatePhysics = false;

    // 아래의 물리량은 지금 BodyInstance를 프로퍼티에 등록할 방법이 없어서 컴포넌트에서 처리함.
    // TODO: BodyInstance 구조체를 프로퍼티에 등록하고 물리량 Instance로 옮기기
    UPROPERTY(EditAnywhere, Category = "Physics")
    bool bOverrideMass = false;

    UPROPERTY(EditAnywhere, Category = "Physics")
    float Mass = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Physics")
    float Density = 10.0f;

    FBodyInstance BodyInstance;

    // 유령, 이벤트 처리만 하고 충돌 처리는 안 함, 위의 CollisionType보다 우선적으로 적용됨.
    // 충돌시에 이벤트 처리 하는 것은 Hit Event라고 따로 있음
    UPROPERTY(EditAnywhere, Category = "Physics")
    bool bIsTrigger = false;

    virtual void ApplyPhysicsResult();

    void CreatePhysicsState() override;

    bool ShouldWelding();

    UPrimitiveComponent* GetPrimitiveParent();

    UPrimitiveComponent* FindPhysicsParent();

    void PrePhysicsUpdate(float DeltaTime) override;

    void AddForce(const FVector& InForce);

    void AddTorque(const FVector& InTorque);

    virtual physx::PxGeometryHolder GetGeometry();

    UPrimitiveComponent();
    virtual ~UPrimitiveComponent() = default;

    void OnRegister(UWorld* InWorld) override;
    void OnUnregister() override;

    void OnTransformUpdated() override;

    void BeginPlay() override;

    virtual void EndPlay() override;

    virtual FAABB GetWorldAABB() const { return FAABB(); }

    // 이 프리미티브를 렌더링하는 데 필요한 FMeshBatchElement를 수집합니다.
    virtual void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) {}

    virtual UMaterialInterface* GetMaterial(uint32 InElementIndex) const
    {
        // 기본 구현: UPrimitiveComponent 자체는 머티리얼을 소유하지 않으므로 nullptr 반환
        return nullptr;
    }
    virtual void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial)
    {
        // 기본 구현: 아무것도 하지 않음 (머티리얼을 지원하지 않거나 설정 불가)
    }

    // 내부적으로 ResourceManager를 통해 UMaterial*를 찾아 SetMaterial을 호출합니다.
    void SetMaterialByName(uint32 InElementIndex, const FString& MaterialName);

    void SetCulled(bool InCulled)
    {
        bIsCulled = InCulled;
    }

    bool GetCulled() const
    {
        return bIsCulled;
    }

    // ───── 충돌 관련 ──────────────────────────── 
    bool IsOverlappingActor(const AActor* Other) const;
    virtual const TArray<FOverlapInfo>& GetOverlapInfos() const { static TArray<FOverlapInfo> Empty; return Empty; }
    
    //Delegate 
    
    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;

    // Overlap event generation toggle API
    void SetGenerateOverlapEvents(bool bEnable) { bGenerateOverlapEvents = bEnable; }
    bool GetGenerateOverlapEvents() const { return bGenerateOverlapEvents; }

    // ───── 직렬화 ────────────────────────────
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    bool bIsCulled = false;
     
    // 이미 PrePhysicsTemporalList에 등록된 객체인지 확인
   // bool bPrePhysicsTemporal = false;

    bool bIsSyncingPhysics = false;
  
};
