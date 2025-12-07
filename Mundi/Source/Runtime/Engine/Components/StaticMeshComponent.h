#pragma once

#include "MeshComponent.h"
#include "AABB.h"
#include "UStaticMeshComponent.generated.h"

struct ID3D11Buffer;
class UStaticMesh;
class UShader;
class UTexture;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FSceneCompData;

// GPU 인스턴싱용 인스턴스 데이터 (ParticleMesh.hlsl 패턴과 동일한 3x4 행렬)
struct FStaticMeshInstanceData
{
	FVector4 Transform0;  // 3x4 행렬 row 0 (x축 + translation.x)
	FVector4 Transform1;  // 3x4 행렬 row 1 (y축 + translation.y)
	FVector4 Transform2;  // 3x4 행렬 row 2 (z축 + translation.z)
};  // 48 bytes per instance

UCLASS(DisplayName="스태틱 메시 컴포넌트", Description="정적 메시를 렌더링하는 컴포넌트입니다")
class UStaticMeshComponent : public UMeshComponent
{
public:

	GENERATED_REFLECTION_BODY()

	UStaticMeshComponent();

protected:
	~UStaticMeshComponent() override;

public:

    // ===== Lua-Bindable Properties (Auto-moved from protected/private) =====

	UPROPERTY(EditAnywhere, Category="Static Mesh", Tooltip="Static mesh asset to render")
	UStaticMesh* StaticMesh = nullptr;
	void OnStaticMeshReleased(UStaticMesh* ReleasedMesh);

	void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	void SetStaticMesh(const FString& PathFileName);

	UStaticMesh* GetStaticMesh() const { return StaticMesh; }

	FAABB GetWorldAABB() const override;

	void DuplicateSubObjects() override;

	// ===== GPU 인스턴싱 =====

	// 인스턴스 추가 (월드 트랜스폼)
	UFUNCTION(LuaBind, DisplayName="AddInstance", Tooltip="Add an instance with world transform")
	int32 AddInstance(const FTransform& WorldTransform);

	// 인스턴스 제거
	UFUNCTION(LuaBind, DisplayName="RemoveInstance", Tooltip="Remove instance at index")
	void RemoveInstance(int32 Index);

	// 모든 인스턴스 제거
	UFUNCTION(LuaBind, DisplayName="ClearInstances", Tooltip="Remove all instances")
	void ClearInstances();

	// 인스턴스 트랜스폼 업데이트
	UFUNCTION(LuaBind, DisplayName="UpdateInstanceTransform", Tooltip="Update instance transform at index")
	void UpdateInstanceTransform(int32 Index, const FTransform& NewTransform);

	// 인스턴스 개수 반환
	UFUNCTION(LuaBind, DisplayName="GetInstanceCount", Tooltip="Get number of instances")
	int32 GetInstanceCount() const { return static_cast<int32>(InstanceTransforms.size()); }

	// 인스턴싱 모드 활성화 여부
	bool IsInstanced() const { return bUseInstancing && !InstanceTransforms.IsEmpty(); }

protected:
	void OnTransformUpdated() override;

	// ===== 인스턴싱 내부 데이터 =====
	TArray<FTransform> InstanceTransforms;       // 인스턴스별 월드 트랜스폼
	ID3D11Buffer* InstanceBuffer = nullptr;      // GPU 인스턴스 버퍼
	uint32 AllocatedInstanceCount = 0;           // 할당된 버퍼 크기
	bool bUseInstancing = false;                 // 인스턴싱 모드 플래그
	bool bInstanceBufferDirty = false;           // 버퍼 업데이트 필요 플래그

	// 인스턴스 버퍼 업데이트 (CPU -> GPU)
	void UpdateInstanceBuffer();

	// 인스턴스 버퍼 해제
	void ReleaseInstanceBuffer();
};
