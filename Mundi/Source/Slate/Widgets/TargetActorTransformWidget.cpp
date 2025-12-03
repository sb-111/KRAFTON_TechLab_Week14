#include "pch.h"
#include <string>
#include "TargetActorTransformWidget.h"
#include "UIManager.h"
#include "ImGui/imgui.h"
#include "Vector.h"
#include "World.h"
#include "ResourceManager.h"
#include "SelectionManager.h"
#include "WorldPartitionManager.h"
#include "PropertyRenderer.h"
#include "Source/Slate/Windows/SParticleEditorWindow.h"
#include "Source/Slate/Windows/SPhysicsAssetEditorWindow.h"

#include "Actor.h"
#include "Grid/GridActor.h"
#include "Gizmo/GizmoActor.h"
#include "StaticMeshActor.h"
#include "FakeSpotLightActor.h"

#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "BodySetup.h"
#include "CollisionShapes.h"
#include "PhysicsSystem.h"
#include "VertexData.h"
#include "RagdollDebugRenderer.h"
#include "PhysxConverter.h"
#include "Renderer.h"
#include "RenderManager.h"
#include "TextRenderComponent.h"
#include "CameraComponent.h"
#include "BillboardComponent.h"
#include "DecalComponent.h"
#include "PerspectiveDecalComponent.h"
#include "HeightFogComponent.h"
#include "DirectionalLightComponent.h"
#include "AmbientLightComponent.h"
#include "PointLightComponent.h"
#include "SpotLightComponent.h"
#include "SceneComponent.h"
#include "Color.h"
#include "PlatformProcess.h"
#include "JsonSerializer.h"

using namespace std;

IMPLEMENT_CLASS(UTargetActorTransformWidget)

namespace
{
	struct FAddableComponentDescriptor
	{
		const char* Label;
		UClass* Class;
		const char* Description;
	};

	const TArray<FAddableComponentDescriptor>& GetAddableComponentDescriptors()
	{
		static TArray<FAddableComponentDescriptor> Options = []()
			{
				TArray<FAddableComponentDescriptor> Result;

				// 리플렉션 시스템을 통해 자동으로 컴포넌트 목록 가져오기
				TArray<UClass*> ComponentClasses = UClass::GetAllComponents();

				for (UClass* Class : ComponentClasses)
				{
					if (Class && Class->bIsComponent && Class->DisplayName)
					{
						Result.push_back({
							Class->DisplayName,
							Class,
							Class->Description ? Class->Description : ""
						});
					}
				}

				return Result;
			}();
		return Options;
	}

	bool TryAttachComponentToActor(AActor& Actor, UClass* ComponentClass, UActorComponent*& SelectedComponent)
	{
		// 부모 컴포넌트 결정 (만약 ParentToAttach가 null이면, AddNewComponent가 알아서 Root에 붙여줄 것임)
		USceneComponent* ParentToAttach = Cast<USceneComponent>(SelectedComponent);

		// AActor에 새로운 컴포넌트 추가
		UActorComponent* NewComp = Actor.AddNewComponent(ComponentClass, ParentToAttach);
		if (!NewComp)
		{
			return false;
		}

		// 새로 생긴 컴포넌트 선택
		SelectedComponent = NewComp;
		GWorld->GetSelectionManager()->SelectComponent(SelectedComponent);

		return true;
	}

	void MarkComponentSubtreeVisited(USceneComponent* Component, TSet<USceneComponent*>& Visited)
	{
		if (!Component || Visited.count(Component))
		{
			return;
		}

		Visited.insert(Component);
		for (USceneComponent* Child : Component->GetAttachChildren())
		{
			MarkComponentSubtreeVisited(Child, Visited);
		}
	}

	void RenderActorComponent(
		AActor* SelectedActor,
		UActorComponent* SelectedComponent,
		UActorComponent* ComponentPendingRemoval
	)
	{
		for (UActorComponent* Component : SelectedActor->GetOwnedComponents())
		{
			if (Cast<USceneComponent>(Component))
			{
				continue;
			}

			ImGuiTreeNodeFlags NodeFlags =
				ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_Leaf | 
				ImGuiTreeNodeFlags_NoTreePushOnOpen;

			// 선택 하이라이트: 현재 선택된 컴포넌트와 같으면 Selected 플래그
			if (Component == SelectedComponent && !GWorld->GetSelectionManager()->IsActorMode())
			{
				NodeFlags |= ImGuiTreeNodeFlags_Selected;
			}

			FString Label = Component->GetClass() ? Component->GetName() : "Unknown Component";

			ImGui::PushID(Component);
			const bool bNodeOpen = ImGui::TreeNodeEx(Component, NodeFlags, "%s", Label.c_str());
			// 좌클릭 시 컴포넌트 선택으로 전환(액터 Row 선택 해제)
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				GWorld->GetSelectionManager()->SelectComponent(Component);
			}

			if (ImGui::BeginPopupContextItem("ComponentContext"))
			{
				const bool bCanRemove = !Component->IsNative();
				if (ImGui::MenuItem("삭제", "Delete", false, bCanRemove))
				{
					ComponentPendingRemoval = Component;
				}
				ImGui::EndPopup();
			}
			
			ImGui::PopID();
		}
	}
	void RenderSceneComponentTree(
		USceneComponent* Component,
		AActor& Actor,
		UActorComponent*& SelectedComponent,
		UActorComponent*& ComponentPendingRemoval,
		TSet<USceneComponent*>& Visited)
	{
		if (!Component)
			return;

		// 에디터 전용 컴포넌트는 계층구조에 표시하지 않음
		// (CREATE_EDITOR_COMPONENT로 생성된 DirectionGizmo, SpriteComponent 등)
		if (!Component->IsEditable())
		{
			return;
		}

		Visited.insert(Component);
		const TArray<USceneComponent*>& Children = Component->GetAttachChildren();
		const bool bHasChildren = !Children.IsEmpty();

		ImGuiTreeNodeFlags NodeFlags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_DefaultOpen;

		if (!bHasChildren)
		{
			NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		// 선택 하이라이트: 현재 선택된 컴포넌트와 같으면 Selected 플래그
		if (Component == SelectedComponent && !GWorld->GetSelectionManager()->IsActorMode())
		{
			NodeFlags |= ImGuiTreeNodeFlags_Selected;
		}

		FString Label = Component->GetClass() ? Component->GetName() : "Unknown Component";
		if (Component == Actor.GetRootComponent())
		{
			Label += " (Root)";
		}

		// 트리 노드 그리기 직전에 ID 푸시
		ImGui::PushID(Component);
		const bool bNodeOpen = ImGui::TreeNodeEx(Component, NodeFlags, "%s", Label.c_str());
		// 좌클릭 시 컴포넌트 선택으로 전환(액터 Row 선택 해제)
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			GWorld->GetSelectionManager()->SelectComponent(Component);
		}

		if (ImGui::BeginPopupContextItem("ComponentContext"))
		{
			const bool bCanRemove = !Component->IsNative();
			if (ImGui::MenuItem("삭제", "Delete", false, bCanRemove))
			{
				ComponentPendingRemoval = Component;
			}
			ImGui::EndPopup();
		}

		if (bNodeOpen && bHasChildren)
		{
			for (USceneComponent* Child : Children)
			{
				RenderSceneComponentTree(Child, Actor, SelectedComponent, ComponentPendingRemoval, Visited);
			}
			ImGui::TreePop();
		}
		else if (!bNodeOpen && bHasChildren)
		{
			for (USceneComponent* Child : Children)
			{
				MarkComponentSubtreeVisited(Child, Visited);
			}
		}
		// 항목 처리가 끝나면 반드시 PopID
		ImGui::PopID();
	}
}

// 파일명 스템(Cube 등) 추출 + .obj 확장자 제거
static inline FString GetBaseNameNoExt(const FString& Path)
{
	const size_t sep = Path.find_last_of("/\\");
	const size_t start = (sep == FString::npos) ? 0 : sep + 1;

	const FString ext = ".obj";
	size_t end = Path.size();

	if (end >= ext.size())
	{
		FString PathExt = Path.substr(end - ext.size());

		std::transform(PathExt.begin(), PathExt.end(), PathExt.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (PathExt == ext)
		{
			end -= ext.size();
		}
	}

	if (start <= end)
	{
		return Path.substr(start, end - start);
	}

	return Path.substr(start);
}

UTargetActorTransformWidget::UTargetActorTransformWidget()
	: UWidget("Target Actor Transform Widget")
	, UIManager(&UUIManager::GetInstance())
{

}

UTargetActorTransformWidget::~UTargetActorTransformWidget() = default;

void UTargetActorTransformWidget::OnSelectedActorCleared()
{
	ResetChangeFlags();
}

void UTargetActorTransformWidget::Initialize()
{
	// UIManager 참조 확보
	UIManager = &UUIManager::GetInstance();

	// Transform 위젯을 UIManager에 등록하여 선택 해제 브로드캐스트를 받을 수 있게 함
	if (UIManager)
	{
		UIManager->RegisterTargetTransformWidget(this);
	}
}


void UTargetActorTransformWidget::Update()
{
	USceneComponent* SelectedComponent = GWorld->GetSelectionManager()->GetSelectedComponent();
	if (SelectedComponent)
	{
		if (!bRotationEditing)
		{
			UpdateTransformFromComponent(SelectedComponent);
		}
	}
}

void UTargetActorTransformWidget::RenderWidget()
{
	AActor* SelectedActor = GWorld->GetSelectionManager()->GetSelectedActor();
	UActorComponent* SelectedComponent = GWorld->GetSelectionManager()->GetSelectedActorComponent();
	if (!SelectedActor)
	{
		return;
	}

	// 1. 헤더 (액터 이름, "+추가" 버튼) 렌더링
	RenderHeader(SelectedActor, SelectedComponent);

	// 2. 컴포넌트 계층 구조 렌더링
	RenderComponentHierarchy(SelectedActor, SelectedComponent);
	// 위 함수에서 SelectedComponent를 Delete하는데 아래 함수에서 SelectedComponent를 그대로 인자로 사용하고 있었음.
	// 댕글링 포인터 참조를 막기 위해 다시 한번 SelectionManager에서 Component를 얻어옴
	// 기존에는 DestroyComponent에서 DeleteObject를 호출하지도 않았음. Delete를 실제로 진행하면서 발견된 버그.
	
	// 3. 선택된 컴포넌트, 엑터의 상세 정보 렌더링 (Transform 포함)
	if (GWorld->GetSelectionManager()->IsActorMode())
	{
		RenderSelectedActorDetails(GWorld->GetSelectionManager()->GetSelectedActor());
	}
	else
	{
		RenderSelectedComponentDetails(GWorld->GetSelectionManager()->GetSelectedActorComponent());
	}
}

void UTargetActorTransformWidget::RenderHeader(AActor* SelectedActor, UActorComponent* SelectedComponent)
{
	ImGui::Text(SelectedActor->GetName().c_str());
	ImGui::SameLine();

	if (ImGui::Button("to Prefab", ImVec2(80, 25)))
	{
		std::filesystem::path PrefabPath = FPlatformProcess::OpenSaveFileDialog(UTF8ToWide(GDataDir) + L"/Prefabs", L"prefab", L"Prefab Files", UTF8ToWide(SelectedActor->ObjectName.ToString()));
		JSON ActorJson;
		ActorJson["Type"] = SelectedActor->GetClass()->Name;
		SelectedActor->Serialize(false, ActorJson);
		bool bSuccess = FJsonSerializer::SaveJsonToFile(ActorJson, PrefabPath);
	}

	ImGui::SameLine();
	const float ButtonWidth = 60.0f;
	const float ButtonHeight = 25.0f;
	float Avail = ImGui::GetContentRegionAvail().x;
	float NewX = ImGui::GetCursorPosX() + std::max(0.0f, Avail - ButtonWidth);
	ImGui::SetCursorPosX(NewX);

	if (ImGui::Button("+ 추가", ImVec2(ButtonWidth, ButtonHeight)))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		ImGui::TextUnformatted("Add Component");
		ImGui::Separator();

		for (const FAddableComponentDescriptor& Descriptor : GetAddableComponentDescriptors())
		{
			ImGui::PushID(Descriptor.Label);
			if (ImGui::Selectable(Descriptor.Label))
			{
				if (TryAttachComponentToActor(*SelectedActor, Descriptor.Class, SelectedComponent))
				{
					ImGui::CloseCurrentPopup();
				}
			}
			if (Descriptor.Description && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", Descriptor.Description);
			}
			ImGui::PopID();
		}
		ImGui::EndPopup();
	}
	ImGui::Spacing();
}

void UTargetActorTransformWidget::RenderComponentHierarchy(AActor* SelectedActor, UActorComponent* SelectedComponent)
{
	AActor* ActorPendingRemoval = nullptr;
	UActorComponent* ComponentPendingRemoval = nullptr;

	// 컴포넌트 트리 박스 크기 관련
	static float PaneHeight = 120.0f;
	const float SplitterThickness = 4.0f;
	const float MinTop = 1.0f;
	const float MinBottom = 0.0f;
	const float WindowAvailY = ImGui::GetContentRegionAvail().y;
	PaneHeight = std::clamp(PaneHeight, MinTop, std::max(MinTop, WindowAvailY - (MinBottom + SplitterThickness)));

	ImGui::BeginChild("ComponentBox", ImVec2(0, PaneHeight), true);

	// 액터 행 렌더링
	ImGui::PushID("ActorDisplay");
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));

	const bool bActorSelected = GWorld->GetSelectionManager()->IsActorMode();
	if (ImGui::Selectable(SelectedActor->GetName().c_str(), bActorSelected, ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_SpanAvailWidth))
	{
		GWorld->GetSelectionManager()->SelectActor(SelectedActor);
	}
	ImGui::PopStyleColor();
	if (ImGui::BeginPopupContextItem("ActorContextMenu"))
	{
		if (ImGui::MenuItem("삭제", "Delete", false, true))
		{
			ActorPendingRemoval = SelectedActor;
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();

	// 컴포넌트 트리 렌더링
	USceneComponent* RootComponent = SelectedActor->GetRootComponent();
	if (!RootComponent)
	{
		ImGui::BulletText("Root component not found.");
	}
	else
	{
		TSet<USceneComponent*> VisitedComponents;
		RenderSceneComponentTree(RootComponent, *SelectedActor, SelectedComponent, ComponentPendingRemoval, VisitedComponents);
		// ... (루트에 붙지 않은 씬 컴포넌트 렌더링 로직은 생략 가능성 있음, 엔진 설계에 따라)
		ImGui::Separator();
		RenderActorComponent(SelectedActor, SelectedComponent, ComponentPendingRemoval);
	}

	// 삭제 입력 처리 (파티클 에디터 또는 피직스 에셋 에디터가 포커스되어 있으면 스킵)
	const bool bDeletePressed = ImGui::IsKeyPressed(ImGuiKey_Delete);
	if (bDeletePressed && !SParticleEditorWindow::bIsAnyParticleEditorFocused && !SPhysicsAssetEditorWindow::bIsAnyPhysicsAssetEditorFocused)
	{
		if (bActorSelected) ActorPendingRemoval = SelectedActor;
		else if (SelectedComponent && !SelectedComponent->IsNative()) ComponentPendingRemoval = SelectedComponent;
	}

	// 컴포넌트 삭제 실행
	if (ComponentPendingRemoval)
	{

		USceneComponent* NewSelection = SelectedActor->GetRootComponent();
		// SelectionManager를 통해 선택 해제
		GWorld->GetSelectionManager()->ClearSelection();

		// 컴포넌트 삭제
		SelectedActor->RemoveOwnedComponent(ComponentPendingRemoval);
		ComponentPendingRemoval = nullptr;

		// 삭제 후 새로운 컴포넌트 선택
		if (NewSelection)
		{
			GWorld->GetSelectionManager()->SelectComponent(NewSelection);
		}
	}

	// 액터 삭제 실행
	if (ActorPendingRemoval)
	{
		// 삭제 전에 선택 해제 (dangling pointer 방지)
		GWorld->GetSelectionManager()->ClearSelection();

		// 액터 삭제
		ActorPendingRemoval->Destroy();

		OnSelectedActorCleared();
	}

	ImGui::EndChild();

	// 스플리터 렌더링
	ImGui::InvisibleButton("VerticalSplitter", ImVec2(-FLT_MIN, SplitterThickness));
	if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	if (ImGui::IsItemActive()) PaneHeight += ImGui::GetIO().MouseDelta.y;
	ImVec2 Min = ImGui::GetItemRectMin(), Max = ImGui::GetItemRectMax();
	ImGui::GetWindowDrawList()->AddLine(ImVec2(Min.x, (Min.y + Max.y) * 0.5f), ImVec2(Max.x, (Min.y + Max.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
	ImGui::Spacing();
}

// 액터 와 루트 컴포넌트 프로퍼티 출력
void UTargetActorTransformWidget::RenderSelectedActorDetails(AActor* SelectedActor)
{
	if (!SelectedActor)
	{
		return;
	}

	// 액터 프로퍼티 표시
	UPropertyRenderer::RenderAllPropertiesWithInheritance(SelectedActor);

	ImGui::Spacing();
	ImGui::Separator();

	// 루트 컴포넌트의 프로퍼티를 이어서 표시
	USceneComponent* RootComponent = SelectedActor->GetRootComponent();
	if (RootComponent)
	{
		UPropertyRenderer::RenderAllPropertiesWithInheritance(RootComponent);
	}

	ImGui::Spacing();
	ImGui::Separator();
}

// 컴포넌트의 프로퍼티 출력
void UTargetActorTransformWidget::RenderSelectedComponentDetails(UActorComponent* SelectedComponent)
{
	if (!SelectedComponent) return;

	// 리플렉션이 적용된 컴포넌트는 자동으로 UI 생성
	if (SelectedComponent)
	{
		UPropertyRenderer::RenderAllPropertiesWithInheritance(SelectedComponent);
	}

	// StaticMeshComponent인 경우 Collision Shape 편집 UI 추가
	if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(SelectedComponent))
	{
		RenderStaticMeshCollisionUI(StaticMeshComp);
	}
}

void UTargetActorTransformWidget::UpdateTransformFromComponent(USceneComponent* SelectedComponent)
{
	if (SelectedComponent)
	{
		EditLocation = SelectedComponent->GetRelativeLocation();
		EditRotation = SelectedComponent->GetRelativeRotation().ToEulerZYXDeg();
		EditScale = SelectedComponent->GetRelativeScale();
	}

	ResetChangeFlags();
}

void UTargetActorTransformWidget::ResetChangeFlags()
{
	bPositionChanged = false;
	bRotationChanged = false;
	bScaleChanged = false;
}

// StaticMeshComponent용 Collision Shape 편집 UI
void UTargetActorTransformWidget::RenderStaticMeshCollisionUI(UStaticMeshComponent* StaticMeshComp)
{
	if (!StaticMeshComp)
		return;

	UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();
	if (!StaticMesh)
		return;

	ImGui::Spacing();
	ImGui::Separator();

	// 콜리전 섹션 헤더
	if (!ImGui::CollapsingHeader("콜리전", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	// BodySetup이 없으면 생성 버튼 표시
	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	if (!BodySetup)
	{
		ImGui::TextDisabled("콜리전 데이터가 없습니다.");
		if (ImGui::Button("BodySetup 생성", ImVec2(-1, 0)))
		{
			BodySetup = NewObject<UBodySetup>();
			StaticMesh->SetBodySetup(BodySetup);
		}
		return;
	}

	// 시각화 토글 - SelectionManager의 플래그를 사용하여 SceneRenderer에서 렌더링
	USelectionManager* SelectionMgr = GWorld->GetSelectionManager();
	bool bShowViz = SelectionMgr ? SelectionMgr->IsCollisionVisualizationEnabled() : false;
	if (ImGui::Checkbox("시각화", &bShowViz))
	{
		if (SelectionMgr)
		{
			SelectionMgr->SetCollisionVisualizationEnabled(bShowViz);
		}
	}

	ImGui::Separator();

	// Shape 추가 버튼
	ImGui::Text("프리미티브");
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
	if (ImGui::Button("+ 추가##Shape"))
	{
		ImGui::OpenPopup("AddShapePopup");
	}

	if (ImGui::BeginPopup("AddShapePopup"))
	{
		if (ImGui::MenuItem("스피어"))
		{
			FKSphereElem NewSphere;
			NewSphere.Name = FName("Sphere");
			NewSphere.Center = FVector::Zero();
			NewSphere.Radius = 50.0f;
			BodySetup->AggGeom.SphereElems.Add(NewSphere);
		}
		if (ImGui::MenuItem("박스"))
		{
			FKBoxElem NewBox;
			NewBox.Name = FName("Box");
			NewBox.Center = FVector::Zero();
			NewBox.X = 50.0f;
			NewBox.Y = 50.0f;
			NewBox.Z = 50.0f;
			BodySetup->AggGeom.BoxElems.Add(NewBox);
		}
		if (ImGui::MenuItem("캡슐"))
		{
			FKSphylElem NewCapsule;
			NewCapsule.Name = FName("Capsule");
			NewCapsule.Center = FVector::Zero();
			NewCapsule.Radius = 25.0f;
			NewCapsule.Length = 100.0f;
			BodySetup->AggGeom.SphylElems.Add(NewCapsule);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("컨벡스 (메시로부터)"))
		{
			// PhysX Quickhull을 사용하여 컨벡스 헐 생성
			FStaticMesh* MeshAsset = StaticMesh->GetStaticMeshAsset();
			if (MeshAsset && MeshAsset->Vertices.Num() > 0)
			{
				// 버텍스 데이터 추출
				TArray<physx::PxVec3> Points;
				Points.reserve(MeshAsset->Vertices.Num());
				for (const FNormalVertex& V : MeshAsset->Vertices)
				{
					Points.Add(physx::PxVec3(V.pos.X, V.pos.Y, V.pos.Z));
				}

				// PhysX Convex Mesh Description
				physx::PxConvexMeshDesc ConvexDesc;
				ConvexDesc.points.count = Points.Num();
				ConvexDesc.points.data = Points.GetData();
				ConvexDesc.points.stride = sizeof(physx::PxVec3);
				ConvexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;

				// Cook convex mesh
				physx::PxCooking* Cooking = FPhysicsSystem::GetInstance().GetCooking();
				if (Cooking)
				{
					physx::PxDefaultMemoryOutputStream WriteBuffer;
					physx::PxConvexMeshCookingResult::Enum Result;
					bool bSuccess = Cooking->cookConvexMesh(ConvexDesc, WriteBuffer, &Result);

					if (bSuccess)
					{
						// 쿠킹된 메시에서 버텍스/인덱스 추출
						physx::PxDefaultMemoryInputData ReadBuffer(WriteBuffer.getData(), WriteBuffer.getSize());
						physx::PxConvexMesh* ConvexMesh = FPhysicsSystem::GetInstance().GetPhysics()->createConvexMesh(ReadBuffer);

						if (ConvexMesh)
						{
							FKConvexElem NewConvex;
							NewConvex.Name = FName("ConvexHull");

							// 버텍스 복사
							uint32 NumVerts = ConvexMesh->getNbVertices();
							const physx::PxVec3* Verts = ConvexMesh->getVertices();
							for (uint32 i = 0; i < NumVerts; ++i)
							{
								NewConvex.VertexData.Add(FVector(Verts[i].x, Verts[i].y, Verts[i].z));
							}

							// 인덱스 복사 (폴리곤에서 삼각형으로 변환)
							uint32 NumPolygons = ConvexMesh->getNbPolygons();
							for (uint32 p = 0; p < NumPolygons; ++p)
							{
								physx::PxHullPolygon Polygon;
								ConvexMesh->getPolygonData(p, Polygon);
								const physx::PxU8* Indices = ConvexMesh->getIndexBuffer() + Polygon.mIndexBase;

								// 폴리곤을 삼각형 팬으로 변환
								for (uint32 t = 1; t + 1 < Polygon.mNbVerts; ++t)
								{
									NewConvex.IndexData.Add(Indices[0]);
									NewConvex.IndexData.Add(Indices[t]);
									NewConvex.IndexData.Add(Indices[t + 1]);
								}
							}

							BodySetup->AggGeom.ConvexElems.Add(NewConvex);
							ConvexMesh->release();
						}
					}
				}
			}
		}
		ImGui::EndPopup();
	}

	// CollisionEnabled 옵션 배열
	const char* collisionModes[] = { "No Collision", "Query Only", "Physics Only", "Collision Enabled" };

	// 삭제 대기 인덱스
	int32 SphereToRemove = -1;
	int32 BoxToRemove = -1;
	int32 CapsuleToRemove = -1;

	// ===== Sphere 섹션 =====
	int32 sphereCount = BodySetup->AggGeom.SphereElems.Num();
	ImGui::PushID("Spheres");
	if (ImGui::TreeNode("스피어"))
	{
		ImGui::SameLine(200);
		ImGui::TextDisabled("Elements: %d", sphereCount);

		for (int32 i = 0; i < sphereCount; ++i)
		{
			FKSphereElem& Sphere = BodySetup->AggGeom.SphereElems[i];
			ImGui::PushID(i);
			if (ImGui::TreeNode("##SphereElem", "인덱스 [%d]", i))
			{
				ImGui::SameLine(200);
				ImGui::TextDisabled("%s", Sphere.Name.ToString().c_str());

				// Center
				float center[3] = { Sphere.Center.X, Sphere.Center.Y, Sphere.Center.Z };
				ImGui::Text("Center");
				ImGui::SetNextItemWidth(-1);
				if (ImGui::DragFloat3("##Center", center, 0.1f))
				{
					Sphere.Center = FVector(center[0], center[1], center[2]);
				}

				// Radius
				ImGui::Text("Radius");
				ImGui::SetNextItemWidth(-1);
				ImGui::DragFloat("##Radius", &Sphere.Radius, 0.1f, 0.1f, 10000.0f, "%.2f");

				// Name
				char nameBuffer[256];
				strcpy_s(nameBuffer, Sphere.Name.ToString().c_str());
				ImGui::Text("Name");
				ImGui::SetNextItemWidth(-1);
				if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
				{
					Sphere.Name = FName(nameBuffer);
				}

				// CollisionEnabled
				int collisionMode = static_cast<int>(Sphere.CollisionEnabled);
				ImGui::Text("Collision Enabled");
				ImGui::SetNextItemWidth(-1);
				if (ImGui::Combo("##Collision", &collisionMode, collisionModes, 4))
				{
					Sphere.CollisionEnabled = static_cast<ECollisionEnabled>(collisionMode);
				}

				// 삭제 버튼
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
				if (ImGui::Button("삭제##Sphere"))
				{
					SphereToRemove = i;
				}
				ImGui::PopStyleColor();

				ImGui::TreePop();
			}
			else
			{
				ImGui::SameLine(200);
				ImGui::TextDisabled("%s", Sphere.Name.ToString().c_str());
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
	else
	{
		ImGui::SameLine(200);
		ImGui::TextDisabled("Elements: %d", sphereCount);
	}
	ImGui::PopID();

	// ===== Box 섹션 =====
	int32 boxCount = BodySetup->AggGeom.BoxElems.Num();
	ImGui::PushID("Boxes");
	if (ImGui::TreeNode("박스"))
	{
		ImGui::SameLine(200);
		ImGui::TextDisabled("Elements: %d", boxCount);

		for (int32 i = 0; i < boxCount; ++i)
		{
			FKBoxElem& Box = BodySetup->AggGeom.BoxElems[i];
			ImGui::PushID(i);
			if (ImGui::TreeNode("##BoxElem", "인덱스 [%d]", i))
			{
				ImGui::SameLine(200);
				ImGui::TextDisabled("%s", Box.Name.ToString().c_str());

				// Center
				float center[3] = { Box.Center.X, Box.Center.Y, Box.Center.Z };
				ImGui::Text("Center");
				ImGui::SetNextItemWidth(-1);
				if (ImGui::DragFloat3("##Center", center, 0.1f))
				{
					Box.Center = FVector(center[0], center[1], center[2]);
				}

				// Rotation
				float rotation[3] = { Box.Rotation.X, Box.Rotation.Y, Box.Rotation.Z };
				ImGui::Text("Rotation");
				ImGui::SetNextItemWidth(-1);
				if (ImGui::DragFloat3("##Rotation", rotation, 0.5f, -180.0f, 180.0f, "%.1f"))
				{
					Box.Rotation = FVector(rotation[0], rotation[1], rotation[2]);
				}

				// Size
				ImGui::Text("X (Half Extent)");
				ImGui::SetNextItemWidth(-1);
				ImGui::DragFloat("##X", &Box.X, 0.1f, 0.1f, 10000.0f, "%.2f");

				ImGui::Text("Y (Half Extent)");
				ImGui::SetNextItemWidth(-1);
				ImGui::DragFloat("##Y", &Box.Y, 0.1f, 0.1f, 10000.0f, "%.2f");

				ImGui::Text("Z (Half Extent)");
				ImGui::SetNextItemWidth(-1);
				ImGui::DragFloat("##Z", &Box.Z, 0.1f, 0.1f, 10000.0f, "%.2f");

				// Name
				char nameBuffer[256];
				strcpy_s(nameBuffer, Box.Name.ToString().c_str());
				ImGui::Text("Name");
				ImGui::SetNextItemWidth(-1);
				if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
				{
					Box.Name = FName(nameBuffer);
				}

				// CollisionEnabled
				int collisionMode = static_cast<int>(Box.CollisionEnabled);
				ImGui::Text("Collision Enabled");
				ImGui::SetNextItemWidth(-1);
				if (ImGui::Combo("##Collision", &collisionMode, collisionModes, 4))
				{
					Box.CollisionEnabled = static_cast<ECollisionEnabled>(collisionMode);
				}

				// 삭제 버튼
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
				if (ImGui::Button("삭제##Box"))
				{
					BoxToRemove = i;
				}
				ImGui::PopStyleColor();

				ImGui::TreePop();
			}
			else
			{
				ImGui::SameLine(200);
				ImGui::TextDisabled("%s", Box.Name.ToString().c_str());
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
	else
	{
		ImGui::SameLine(200);
		ImGui::TextDisabled("Elements: %d", boxCount);
	}
	ImGui::PopID();

	// ===== Capsule 섹션 =====
	int32 capsuleCount = BodySetup->AggGeom.SphylElems.Num();
	ImGui::PushID("Capsules");
	if (ImGui::TreeNode("캡슐"))
	{
		ImGui::SameLine(200);
		ImGui::TextDisabled("Elements: %d", capsuleCount);

		for (int32 i = 0; i < capsuleCount; ++i)
		{
			FKSphylElem& Capsule = BodySetup->AggGeom.SphylElems[i];
			ImGui::PushID(i);
			if (ImGui::TreeNode("##CapsuleElem", "인덱스 [%d]", i))
			{
				ImGui::SameLine(200);
				ImGui::TextDisabled("%s", Capsule.Name.ToString().c_str());

				// Center
				float center[3] = { Capsule.Center.X, Capsule.Center.Y, Capsule.Center.Z };
				ImGui::Text("Center");
				ImGui::SetNextItemWidth(-1);
				if (ImGui::DragFloat3("##Center", center, 0.1f))
				{
					Capsule.Center = FVector(center[0], center[1], center[2]);
				}

				// Rotation
				float rotation[3] = { Capsule.Rotation.X, Capsule.Rotation.Y, Capsule.Rotation.Z };
				ImGui::Text("Rotation");
				ImGui::SetNextItemWidth(-1);
				if (ImGui::DragFloat3("##Rotation", rotation, 0.5f, -180.0f, 180.0f, "%.1f"))
				{
					Capsule.Rotation = FVector(rotation[0], rotation[1], rotation[2]);
				}

				// Radius
				ImGui::Text("Radius");
				ImGui::SetNextItemWidth(-1);
				ImGui::DragFloat("##Radius", &Capsule.Radius, 0.1f, 0.1f, 10000.0f, "%.2f");

				// Length
				ImGui::Text("Length");
				ImGui::SetNextItemWidth(-1);
				ImGui::DragFloat("##Length", &Capsule.Length, 0.1f, 0.1f, 10000.0f, "%.2f");

				// Name
				char nameBuffer[256];
				strcpy_s(nameBuffer, Capsule.Name.ToString().c_str());
				ImGui::Text("Name");
				ImGui::SetNextItemWidth(-1);
				if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
				{
					Capsule.Name = FName(nameBuffer);
				}

				// CollisionEnabled
				int collisionMode = static_cast<int>(Capsule.CollisionEnabled);
				ImGui::Text("Collision Enabled");
				ImGui::SetNextItemWidth(-1);
				if (ImGui::Combo("##Collision", &collisionMode, collisionModes, 4))
				{
					Capsule.CollisionEnabled = static_cast<ECollisionEnabled>(collisionMode);
				}

				// 삭제 버튼
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
				if (ImGui::Button("삭제##Capsule"))
				{
					CapsuleToRemove = i;
				}
				ImGui::PopStyleColor();

				ImGui::TreePop();
			}
			else
			{
				ImGui::SameLine(200);
				ImGui::TextDisabled("%s", Capsule.Name.ToString().c_str());
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
	else
	{
		ImGui::SameLine(200);
		ImGui::TextDisabled("Elements: %d", capsuleCount);
	}
	ImGui::PopID();

	// ===== Convex 섹션 (읽기 전용) =====
	int32 convexCount = BodySetup->AggGeom.ConvexElems.Num();
	ImGui::PushID("Convex");
	if (ImGui::TreeNode("컨벡스"))
	{
		ImGui::SameLine(200);
		ImGui::TextDisabled("Elements: %d", convexCount);

		for (int32 i = 0; i < convexCount; ++i)
		{
			FKConvexElem& Convex = BodySetup->AggGeom.ConvexElems[i];
			ImGui::PushID(i);
			if (ImGui::TreeNode("##ConvexElem", "인덱스 [%d]", i))
			{
				ImGui::SameLine(200);
				ImGui::TextDisabled("%s", Convex.Name.ToString().c_str());

				ImGui::Text("Vertices: %d", (int32)Convex.VertexData.Num());
				ImGui::Text("Indices: %d", (int32)Convex.IndexData.Num());

				// Name
				char nameBuffer[256];
				strcpy_s(nameBuffer, Convex.Name.ToString().c_str());
				ImGui::Text("Name");
				ImGui::SetNextItemWidth(-1);
				if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
				{
					Convex.Name = FName(nameBuffer);
				}

				ImGui::TreePop();
			}
			else
			{
				ImGui::SameLine(200);
				ImGui::TextDisabled("%s", Convex.Name.ToString().c_str());
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
	else
	{
		ImGui::SameLine(200);
		ImGui::TextDisabled("Elements: %d", convexCount);
	}
	ImGui::PopID();

	// 삭제 실행
	if (SphereToRemove >= 0 && SphereToRemove < BodySetup->AggGeom.SphereElems.Num())
	{
		BodySetup->AggGeom.SphereElems.RemoveAt(SphereToRemove);
	}
	if (BoxToRemove >= 0 && BoxToRemove < BodySetup->AggGeom.BoxElems.Num())
	{
		BodySetup->AggGeom.BoxElems.RemoveAt(BoxToRemove);
	}
	if (CapsuleToRemove >= 0 && CapsuleToRemove < BodySetup->AggGeom.SphylElems.Num())
	{
		BodySetup->AggGeom.SphylElems.RemoveAt(CapsuleToRemove);
	}
}
