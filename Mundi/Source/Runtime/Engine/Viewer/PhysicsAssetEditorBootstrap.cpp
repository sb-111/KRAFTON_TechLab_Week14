#include "pch.h"
#include "PhysicsAssetEditorBootstrap.h"
#include "ViewerState.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/Physics/PhysicsAsset.h"
#include "Source/Runtime/Engine/Physics/BodySetup.h"
#include "Source/Runtime/Engine/Physics/ConstraintInstance.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/Engine/Components/ShapeAnchorComponent.h"
#include "Source/Runtime/Engine/Components/ConstraintAnchorComponent.h"
#include "EditorAssetPreviewContext.h"

ViewerState* PhysicsAssetEditorBootstrap::CreateViewerState(const char* Name, UWorld* InWorld,
	ID3D11Device* InDevice, UEditorAssetPreviewContext* Context)
{
	if (!InDevice) return nullptr;

	// PhysicsAssetEditorState 생성
	PhysicsAssetEditorState* State = new PhysicsAssetEditorState();
	State->Name = Name ? Name : "Physics Asset Editor";

	// Preview world 생성
	State->World = NewObject<UWorld>();
	State->World->SetWorldType(EWorldType::PreviewMinimal);
	State->World->Initialize();
	State->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);

	// Viewport 생성
	State->Viewport = new FViewport();
	State->Viewport->Initialize(0.f, 0.f, 1.f, 1.f, InDevice);
	State->Viewport->SetUseRenderTarget(true);

	// ViewportClient 생성
	FViewportClient* Client = new FViewportClient();
	Client->SetWorld(State->World);
	Client->SetViewportType(EViewportType::Perspective);
	Client->SetViewMode(EViewMode::VMI_Lit_Phong);

	// 카메라 설정 - 정면에서 가까이 바라보게
	Client->GetCamera()->SetActorLocation(FVector(3.f, 0.f, 1.f));
	Client->GetCamera()->SetRotationFromEulerAngles(FVector(0.f, 10.f, 180.f));

	State->Client = Client;
	State->Viewport->SetViewportClient(Client);
	State->World->SetEditorCameraActor(Client->GetCamera());

	// 프리뷰 액터 생성 (스켈레탈 메시를 담을 액터)
	if (State->World)
	{
		ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
		State->PreviewActor = Preview;
		State->CurrentMesh = Preview && Preview->GetSkeletalMeshComponent()
			? Preview->GetSkeletalMeshComponent()->GetSkeletalMesh() : nullptr;
	}

	// Physics Asset 생성
	State->EditingAsset = NewObject<UPhysicsAsset>();

	// Shape 와이어프레임용 LineComponent 생성 및 연결
	State->ShapeLineComponent = NewObject<ULineComponent>();
	State->ShapeLineComponent->SetAlwaysOnTop(true);
	if (State->PreviewActor)
	{
		State->PreviewActor->AddOwnedComponent(State->ShapeLineComponent);
		State->ShapeLineComponent->RegisterComponent(State->World);
	}

	// Shape 기즈모용 앵커 컴포넌트 생성
	State->ShapeGizmoAnchor = NewObject<UShapeAnchorComponent>();
	State->ShapeGizmoAnchor->SetVisibility(false);
	State->ShapeGizmoAnchor->SetEditability(false);
	if (State->PreviewActor)
	{
		State->PreviewActor->AddOwnedComponent(State->ShapeGizmoAnchor);
		State->ShapeGizmoAnchor->RegisterComponent(State->World);
	}

	// Constraint 와이어프레임용 LineComponent 생성 및 연결
	State->ConstraintLineComponent = NewObject<ULineComponent>();
	State->ConstraintLineComponent->SetAlwaysOnTop(true);
	if (State->PreviewActor)
	{
		State->PreviewActor->AddOwnedComponent(State->ConstraintLineComponent);
		State->ConstraintLineComponent->RegisterComponent(State->World);
	}

	// Constraint 기즈모용 앵커 컴포넌트 생성
	State->ConstraintGizmoAnchor = NewObject<UConstraintAnchorComponent>();
	State->ConstraintGizmoAnchor->SetVisibility(false);
	State->ConstraintGizmoAnchor->SetEditability(false);
	if (State->PreviewActor)
	{
		State->PreviewActor->AddOwnedComponent(State->ConstraintGizmoAnchor);
		State->ConstraintGizmoAnchor->RegisterComponent(State->World);
	}

	return State;
}

void PhysicsAssetEditorBootstrap::DestroyViewerState(ViewerState*& State)
{
	if (!State) return;

	// PhysicsAssetEditorState로 캐스팅
	PhysicsAssetEditorState* PhysState = static_cast<PhysicsAssetEditorState*>(State);

	// 리소스 정리
	if (PhysState->Viewport)
	{
		delete PhysState->Viewport;
		PhysState->Viewport = nullptr;
	}

	if (PhysState->Client)
	{
		delete PhysState->Client;
		PhysState->Client = nullptr;
	}

	if (PhysState->World)
	{
		ObjectFactory::DeleteObject(PhysState->World);
		PhysState->World = nullptr;
	}

	// Physics Asset은 NewObject로 생성되었으므로 ObjectFactory::DeleteObject로 삭제
	// ObjectFactory::DeleteAll에서 자동으로 삭제되므로 여기서는 포인터만 nullptr로 설정
	// (World 삭제 시 관련 액터들도 함께 정리됨)
	PhysState->EditingAsset = nullptr;

	// ShapeLineComponent는 PreviewActor에 AddOwnedComponent로 추가했으므로 Actor 삭제 시 함께 정리됨
	PhysState->ShapeLineComponent = nullptr;
	PhysState->ShapeGizmoAnchor = nullptr;
	PhysState->ConstraintLineComponent = nullptr;
	PhysState->ConstraintGizmoAnchor = nullptr;

	delete State;
	State = nullptr;
}

bool PhysicsAssetEditorBootstrap::SavePhysicsAsset(UPhysicsAsset* Asset, const FString& FilePath)
{
	// TODO: JSON 직렬화 구현
	if (!Asset || FilePath.empty())
		return false;

	UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: %s", FilePath.c_str());
	return true;
}

UPhysicsAsset* PhysicsAssetEditorBootstrap::LoadPhysicsAsset(const FString& FilePath)
{
	// TODO: JSON 역직렬화 구현
	if (FilePath.empty())
		return nullptr;

	UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: %s", FilePath.c_str());
	return nullptr;
}
