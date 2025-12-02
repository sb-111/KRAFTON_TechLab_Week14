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
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "EditorAssetPreviewContext.h"
#include "PhysicsSystem.h"
#include "PhysicsScene.h"

ViewerState* PhysicsAssetEditorBootstrap::CreateViewerState(const char* Name, UWorld* InWorld,
	ID3D11Device* InDevice, UEditorAssetPreviewContext* Context)
{
	if (!InDevice) return nullptr;

	// PhysicsAssetEditorState 생성
	PhysicsAssetEditorState* State = new PhysicsAssetEditorState();
	State->Name = Name ? Name : "Physics Asset Editor";

	// Preview world 생성
	// Initialize()에서 PhysicsScene도 자동으로 생성됨
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

// ===== Helper 함수들: Shape 직렬화 =====
static JSON SerializeSphereElem(const FKSphereElem& Elem)
{
	JSON Obj = JSON::Make(JSON::Class::Object);
	Obj["Name"] = Elem.Name.ToString();
	Obj["Center"] = FJsonSerializer::VectorToJson(Elem.Center);
	Obj["Radius"] = Elem.Radius;
	return Obj;
}

static JSON SerializeBoxElem(const FKBoxElem& Elem)
{
	JSON Obj = JSON::Make(JSON::Class::Object);
	Obj["Name"] = Elem.Name.ToString();
	Obj["Center"] = FJsonSerializer::VectorToJson(Elem.Center);
	Obj["Rotation"] = FJsonSerializer::VectorToJson(Elem.Rotation);
	Obj["X"] = Elem.X;
	Obj["Y"] = Elem.Y;
	Obj["Z"] = Elem.Z;
	return Obj;
}

static JSON SerializeSphylElem(const FKSphylElem& Elem)
{
	JSON Obj = JSON::Make(JSON::Class::Object);
	Obj["Name"] = Elem.Name.ToString();
	Obj["Center"] = FJsonSerializer::VectorToJson(Elem.Center);
	Obj["Rotation"] = FJsonSerializer::VectorToJson(Elem.Rotation);
	Obj["Radius"] = Elem.Radius;
	Obj["Length"] = Elem.Length;
	return Obj;
}

static JSON SerializeAggGeom(const FKAggregateGeom& AggGeom)
{
	JSON Obj = JSON::Make(JSON::Class::Object);

	// Sphere Elements
	JSON SphereArray = JSON::Make(JSON::Class::Array);
	for (const FKSphereElem& Sphere : AggGeom.SphereElems)
	{
		SphereArray.append(SerializeSphereElem(Sphere));
	}
	Obj["SphereElems"] = SphereArray;

	// Box Elements
	JSON BoxArray = JSON::Make(JSON::Class::Array);
	for (const FKBoxElem& Box : AggGeom.BoxElems)
	{
		BoxArray.append(SerializeBoxElem(Box));
	}
	Obj["BoxElems"] = BoxArray;

	// Sphyl (Capsule) Elements
	JSON SphylArray = JSON::Make(JSON::Class::Array);
	for (const FKSphylElem& Sphyl : AggGeom.SphylElems)
	{
		SphylArray.append(SerializeSphylElem(Sphyl));
	}
	Obj["SphylElems"] = SphylArray;

	return Obj;
}

static JSON SerializeBodySetup(UBodySetup* Body)
{
	JSON Obj = JSON::Make(JSON::Class::Object);
	Obj["BoneName"] = Body->BoneName.ToString();
	Obj["bSimulatePhysics"] = Body->bSimulatePhysics;
	Obj["CollisionEnabled"] = static_cast<int32>(Body->CollisionEnabled);
	Obj["MassInKg"] = Body->MassInKg;
	Obj["LinearDamping"] = Body->LinearDamping;
	Obj["AngularDamping"] = Body->AngularDamping;
	Obj["Friction"] = Body->Friction;
	Obj["Restitution"] = Body->Restitution;
	Obj["AggGeom"] = SerializeAggGeom(Body->AggGeom);
	return Obj;
}

static JSON SerializeConstraintInstance(const FConstraintInstance& Constraint)
{
	JSON Obj = JSON::Make(JSON::Class::Object);
	Obj["ConstraintBone1"] = Constraint.ConstraintBone1.ToString();
	Obj["ConstraintBone2"] = Constraint.ConstraintBone2.ToString();

	// Linear Limits
	Obj["LinearXMotion"] = static_cast<int32>(Constraint.LinearXMotion);
	Obj["LinearYMotion"] = static_cast<int32>(Constraint.LinearYMotion);
	Obj["LinearZMotion"] = static_cast<int32>(Constraint.LinearZMotion);
	Obj["LinearLimit"] = Constraint.LinearLimit;

	// Angular Limits
	Obj["TwistMotion"] = static_cast<int32>(Constraint.TwistMotion);
	Obj["Swing1Motion"] = static_cast<int32>(Constraint.Swing1Motion);
	Obj["Swing2Motion"] = static_cast<int32>(Constraint.Swing2Motion);
	Obj["TwistLimitAngle"] = Constraint.TwistLimitAngle;
	Obj["Swing1LimitAngle"] = Constraint.Swing1LimitAngle;
	Obj["Swing2LimitAngle"] = Constraint.Swing2LimitAngle;

	// Transform
	Obj["Position1"] = FJsonSerializer::VectorToJson(Constraint.Position1);
	Obj["Rotation1"] = FJsonSerializer::VectorToJson(Constraint.Rotation1);
	Obj["Position2"] = FJsonSerializer::VectorToJson(Constraint.Position2);
	Obj["Rotation2"] = FJsonSerializer::VectorToJson(Constraint.Rotation2);

	// Collision
	Obj["bDisableCollision"] = Constraint.bDisableCollision;

	// Motor
	Obj["bAngularMotorEnabled"] = Constraint.bAngularMotorEnabled;
	Obj["AngularMotorStrength"] = Constraint.AngularMotorStrength;
	Obj["AngularMotorDamping"] = Constraint.AngularMotorDamping;

	return Obj;
}

bool PhysicsAssetEditorBootstrap::SavePhysicsAsset(UPhysicsAsset* Asset, const FString& FilePath, const FString& SourceFbxPath)
{
	if (!Asset)
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: Asset이 nullptr입니다");
		return false;
	}

	if (FilePath.empty())
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: FilePath가 비어있습니다");
		return false;
	}

	// Root JSON Object
	JSON JsonHandle = JSON::Make(JSON::Class::Object);

	// 소스 FBX 경로 저장 (백슬래시를 슬래시로 정규화하여 JSON 이스케이프 문제 방지)
	if (!SourceFbxPath.empty())
	{
		FString NormalizedFbxPath = NormalizePath(SourceFbxPath);
		JsonHandle["SourceFbxPath"] = NormalizedFbxPath;
	}

	// Bodies 배열 직렬화
	JSON BodiesArray = JSON::Make(JSON::Class::Array);
	for (UBodySetup* Body : Asset->Bodies)
	{
		if (Body)
		{
			BodiesArray.append(SerializeBodySetup(Body));
		}
	}
	JsonHandle["Bodies"] = BodiesArray;

	// Constraints 배열 직렬화
	JSON ConstraintsArray = JSON::Make(JSON::Class::Array);
	for (const FConstraintInstance& Constraint : Asset->Constraints)
	{
		ConstraintsArray.append(SerializeConstraintInstance(Constraint));
	}
	JsonHandle["Constraints"] = ConstraintsArray;

	// 파일로 저장
	FWideString WidePath = UTF8ToWide(FilePath);
	if (!FJsonSerializer::SaveJsonToFile(JsonHandle, WidePath))
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: 파일 저장 실패: %s", FilePath.c_str());
		return false;
	}

	UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: 저장 성공: %s", FilePath.c_str());
	return true;
}

// ===== Helper 함수들: Shape 역직렬화 =====
static FKSphereElem DeserializeSphereElem(const JSON& Data)
{
	FKSphereElem Elem;
	FString NameStr;
	FJsonSerializer::ReadString(Data, "Name", NameStr, "", false);
	Elem.Name = FName(NameStr);
	FJsonSerializer::ReadVector(Data, "Center", Elem.Center, FVector::Zero(), false);
	FJsonSerializer::ReadFloat(Data, "Radius", Elem.Radius, 1.0f, false);
	return Elem;
}

static FKBoxElem DeserializeBoxElem(const JSON& Data)
{
	FKBoxElem Elem;
	FString NameStr;
	FJsonSerializer::ReadString(Data, "Name", NameStr, "", false);
	Elem.Name = FName(NameStr);
	FJsonSerializer::ReadVector(Data, "Center", Elem.Center, FVector::Zero(), false);
	FJsonSerializer::ReadVector(Data, "Rotation", Elem.Rotation, FVector::Zero(), false);
	FJsonSerializer::ReadFloat(Data, "X", Elem.X, 1.0f, false);
	FJsonSerializer::ReadFloat(Data, "Y", Elem.Y, 1.0f, false);
	FJsonSerializer::ReadFloat(Data, "Z", Elem.Z, 1.0f, false);
	return Elem;
}

static FKSphylElem DeserializeSphylElem(const JSON& Data)
{
	FKSphylElem Elem;
	FString NameStr;
	FJsonSerializer::ReadString(Data, "Name", NameStr, "", false);
	Elem.Name = FName(NameStr);
	FJsonSerializer::ReadVector(Data, "Center", Elem.Center, FVector::Zero(), false);
	FJsonSerializer::ReadVector(Data, "Rotation", Elem.Rotation, FVector::Zero(), false);
	FJsonSerializer::ReadFloat(Data, "Radius", Elem.Radius, 1.0f, false);
	FJsonSerializer::ReadFloat(Data, "Length", Elem.Length, 1.0f, false);
	return Elem;
}

static void DeserializeAggGeom(const JSON& Data, FKAggregateGeom& OutAggGeom)
{
	OutAggGeom.EmptyElements();

	// Sphere Elements
	JSON SphereArray;
	if (FJsonSerializer::ReadArray(Data, "SphereElems", SphereArray, nullptr, false))
	{
		for (size_t i = 0; i < SphereArray.size(); ++i)
		{
			OutAggGeom.SphereElems.Add(DeserializeSphereElem(SphereArray.at(i)));
		}
	}

	// Box Elements
	JSON BoxArray;
	if (FJsonSerializer::ReadArray(Data, "BoxElems", BoxArray, nullptr, false))
	{
		for (size_t i = 0; i < BoxArray.size(); ++i)
		{
			OutAggGeom.BoxElems.Add(DeserializeBoxElem(BoxArray.at(i)));
		}
	}

	// Sphyl (Capsule) Elements
	JSON SphylArray;
	if (FJsonSerializer::ReadArray(Data, "SphylElems", SphylArray, nullptr, false))
	{
		for (size_t i = 0; i < SphylArray.size(); ++i)
		{
			OutAggGeom.SphylElems.Add(DeserializeSphylElem(SphylArray.at(i)));
		}
	}
}

static UBodySetup* DeserializeBodySetup(const JSON& Data)
{
	UBodySetup* Body = NewObject<UBodySetup>();

	FString BoneNameStr;
	FJsonSerializer::ReadString(Data, "BoneName", BoneNameStr, "", false);
	Body->BoneName = FName(BoneNameStr);

	FJsonSerializer::ReadBool(Data, "bSimulatePhysics", Body->bSimulatePhysics, false, false);

	int32 CollisionEnabledInt = 0;
	FJsonSerializer::ReadInt32(Data, "CollisionEnabled", CollisionEnabledInt, static_cast<int32>(ECollisionEnabled::PhysicsAndQuery), false);
	Body->CollisionEnabled = static_cast<ECollisionEnabled>(CollisionEnabledInt);

	FJsonSerializer::ReadFloat(Data, "MassInKg", Body->MassInKg, 1.0f, false);
	FJsonSerializer::ReadFloat(Data, "LinearDamping", Body->LinearDamping, 0.01f, false);
	FJsonSerializer::ReadFloat(Data, "AngularDamping", Body->AngularDamping, 0.05f, false);
	FJsonSerializer::ReadFloat(Data, "Friction", Body->Friction, 0.5f, false);
	FJsonSerializer::ReadFloat(Data, "Restitution", Body->Restitution, 0.1f, false);

	// AggGeom 역직렬화
	JSON AggGeomData;
	if (FJsonSerializer::ReadObject(Data, "AggGeom", AggGeomData, nullptr, false))
	{
		DeserializeAggGeom(AggGeomData, Body->AggGeom);
	}

	return Body;
}

static FConstraintInstance DeserializeConstraintInstance(const JSON& Data)
{
	FConstraintInstance Constraint;

	FString Bone1Str, Bone2Str;
	FJsonSerializer::ReadString(Data, "ConstraintBone1", Bone1Str, "", false);
	FJsonSerializer::ReadString(Data, "ConstraintBone2", Bone2Str, "", false);
	Constraint.ConstraintBone1 = FName(Bone1Str);
	Constraint.ConstraintBone2 = FName(Bone2Str);

	// Linear Limits
	int32 LinearXInt = 0, LinearYInt = 0, LinearZInt = 0;
	FJsonSerializer::ReadInt32(Data, "LinearXMotion", LinearXInt, static_cast<int32>(ELinearConstraintMotion::Locked), false);
	FJsonSerializer::ReadInt32(Data, "LinearYMotion", LinearYInt, static_cast<int32>(ELinearConstraintMotion::Locked), false);
	FJsonSerializer::ReadInt32(Data, "LinearZMotion", LinearZInt, static_cast<int32>(ELinearConstraintMotion::Locked), false);
	Constraint.LinearXMotion = static_cast<ELinearConstraintMotion>(LinearXInt);
	Constraint.LinearYMotion = static_cast<ELinearConstraintMotion>(LinearYInt);
	Constraint.LinearZMotion = static_cast<ELinearConstraintMotion>(LinearZInt);
	FJsonSerializer::ReadFloat(Data, "LinearLimit", Constraint.LinearLimit, 0.0f, false);

	// Angular Limits
	int32 TwistInt = 0, Swing1Int = 0, Swing2Int = 0;
	FJsonSerializer::ReadInt32(Data, "TwistMotion", TwistInt, static_cast<int32>(EAngularConstraintMotion::Limited), false);
	FJsonSerializer::ReadInt32(Data, "Swing1Motion", Swing1Int, static_cast<int32>(EAngularConstraintMotion::Limited), false);
	FJsonSerializer::ReadInt32(Data, "Swing2Motion", Swing2Int, static_cast<int32>(EAngularConstraintMotion::Limited), false);
	Constraint.TwistMotion = static_cast<EAngularConstraintMotion>(TwistInt);
	Constraint.Swing1Motion = static_cast<EAngularConstraintMotion>(Swing1Int);
	Constraint.Swing2Motion = static_cast<EAngularConstraintMotion>(Swing2Int);
	FJsonSerializer::ReadFloat(Data, "TwistLimitAngle", Constraint.TwistLimitAngle, 45.0f, false);
	FJsonSerializer::ReadFloat(Data, "Swing1LimitAngle", Constraint.Swing1LimitAngle, 45.0f, false);
	FJsonSerializer::ReadFloat(Data, "Swing2LimitAngle", Constraint.Swing2LimitAngle, 45.0f, false);

	// Transform
	FJsonSerializer::ReadVector(Data, "Position1", Constraint.Position1, FVector::Zero(), false);
	FJsonSerializer::ReadVector(Data, "Rotation1", Constraint.Rotation1, FVector::Zero(), false);
	FJsonSerializer::ReadVector(Data, "Position2", Constraint.Position2, FVector::Zero(), false);
	FJsonSerializer::ReadVector(Data, "Rotation2", Constraint.Rotation2, FVector::Zero(), false);

	// Collision
	FJsonSerializer::ReadBool(Data, "bDisableCollision", Constraint.bDisableCollision, true, false);

	// Motor
	FJsonSerializer::ReadBool(Data, "bAngularMotorEnabled", Constraint.bAngularMotorEnabled, false, false);
	FJsonSerializer::ReadFloat(Data, "AngularMotorStrength", Constraint.AngularMotorStrength, 0.0f, false);
	FJsonSerializer::ReadFloat(Data, "AngularMotorDamping", Constraint.AngularMotorDamping, 0.0f, false);

	return Constraint;
}

UPhysicsAsset* PhysicsAssetEditorBootstrap::LoadPhysicsAsset(const FString& FilePath, FString* OutSourceFbxPath)
{
	if (FilePath.empty())
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: FilePath가 비어있습니다");
		return nullptr;
	}

	// 경로 정규화
	FString NormalizedFilePath = ResolveAssetRelativePath(NormalizePath(FilePath), "");

	// 파일에서 JSON 로드
	FWideString WidePath = UTF8ToWide(NormalizedFilePath);
	JSON JsonHandle;
	if (!FJsonSerializer::LoadJsonFromFile(JsonHandle, WidePath))
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: 파일 로드 실패: %s", NormalizedFilePath.c_str());
		return nullptr;
	}

	// 소스 FBX 경로 읽기
	if (OutSourceFbxPath)
	{
		FJsonSerializer::ReadString(JsonHandle, "SourceFbxPath", *OutSourceFbxPath, "", false);
	}

	// 새 PhysicsAsset 객체 생성
	UPhysicsAsset* LoadedAsset = NewObject<UPhysicsAsset>();
	if (!LoadedAsset)
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: PhysicsAsset 객체 생성 실패");
		return nullptr;
	}

	// Bodies 배열 역직렬화
	JSON BodiesArray;
	if (FJsonSerializer::ReadArray(JsonHandle, "Bodies", BodiesArray, nullptr, false))
	{
		for (size_t i = 0; i < BodiesArray.size(); ++i)
		{
			UBodySetup* Body = DeserializeBodySetup(BodiesArray.at(i));
			if (Body)
			{
				LoadedAsset->Bodies.Add(Body);
			}
		}
	}

	// Constraints 배열 역직렬화
	JSON ConstraintsArray;
	if (FJsonSerializer::ReadArray(JsonHandle, "Constraints", ConstraintsArray, nullptr, false))
	{
		for (size_t i = 0; i < ConstraintsArray.size(); ++i)
		{
			FConstraintInstance Constraint = DeserializeConstraintInstance(ConstraintsArray.at(i));
			LoadedAsset->Constraints.Add(Constraint);
		}
	}

	UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: 로드 성공: %s", NormalizedFilePath.c_str());
	return LoadedAsset;
}

// ============================================================================
// Physics Asset 파일 스캔 함수들
// ============================================================================

void PhysicsAssetEditorBootstrap::GetAllPhysicsAssetPaths(TArray<FString>& OutPaths, TArray<FString>& OutDisplayNames)
{
	OutPaths.Empty();
	OutDisplayNames.Empty();

	// "없음" 옵션 추가
	OutPaths.Add("");
	OutDisplayNames.Add("None");

	// Data/Physics 폴더 스캔
	FString PhysicsDir = GDataDir + "/Physics";
	std::filesystem::path SearchPath(PhysicsDir);

	if (!std::filesystem::exists(SearchPath))
	{
		return;
	}

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(SearchPath))
	{
		if (Entry.is_regular_file())
		{
			std::filesystem::path FilePath = Entry.path();
			if (FilePath.extension() == ".physics")
			{
				FString FullPath = FilePath.string();
				FString FileName = FilePath.filename().string();

				// 경로 정규화
				FullPath = NormalizePath(FullPath);

				OutPaths.Add(FullPath);
				OutDisplayNames.Add(FileName);
			}
		}
	}
}

void PhysicsAssetEditorBootstrap::GetCompatiblePhysicsAssetPaths(const FString& FbxPath, TArray<FString>& OutPaths, TArray<FString>& OutDisplayNames)
{
	OutPaths.Empty();
	OutDisplayNames.Empty();

	// "없음" 옵션 추가
	OutPaths.Add("");
	OutDisplayNames.Add("None");

	if (FbxPath.empty())
	{
		// FBX 경로가 없으면 모든 Physics Asset 반환
		TArray<FString> AllPaths, AllNames;
		GetAllPhysicsAssetPaths(AllPaths, AllNames);

		// "None" 제외하고 추가
		for (int32 i = 1; i < AllPaths.Num(); ++i)
		{
			OutPaths.Add(AllPaths[i]);
			OutDisplayNames.Add(AllNames[i]);
		}
		return;
	}

	// FBX 경로 정규화
	FString NormalizedFbxPath = NormalizePath(FbxPath);

	// Data/Physics 폴더 스캔
	FString PhysicsDir = GDataDir + "/Physics";
	std::filesystem::path SearchPath(PhysicsDir);

	if (!std::filesystem::exists(SearchPath))
	{
		return;
	}

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(SearchPath))
	{
		if (Entry.is_regular_file())
		{
			std::filesystem::path FilePath = Entry.path();
			if (FilePath.extension() == ".physics")
			{
				FString FullPath = NormalizePath(FilePath.string());

				// .physics 파일에서 SourceFbxPath 읽기
				FString SourceFbx;
				FWideString WidePath = UTF8ToWide(FullPath);
				JSON JsonHandle;

				if (FJsonSerializer::LoadJsonFromFile(JsonHandle, WidePath))
				{
					FJsonSerializer::ReadString(JsonHandle, "SourceFbxPath", SourceFbx, "", false);
					SourceFbx = NormalizePath(SourceFbx);

					// FBX 경로가 매칭되면 추가
					if (SourceFbx == NormalizedFbxPath)
					{
						FString FileName = FilePath.filename().string();
						OutPaths.Add(FullPath);
						OutDisplayNames.Add(FileName);
					}
				}
			}
		}
	}

	// 매칭되는 것이 없으면 모든 Physics Asset도 표시 (선택 가능하게)
	if (OutPaths.Num() == 1)  // "None"만 있으면
	{
		TArray<FString> AllPaths, AllNames;
		GetAllPhysicsAssetPaths(AllPaths, AllNames);

		// "None" 제외하고 추가 (다른 메쉬용이라고 표시)
		for (int32 i = 1; i < AllPaths.Num(); ++i)
		{
			OutPaths.Add(AllPaths[i]);
			OutDisplayNames.Add(AllNames[i] + " (other)");
		}
	}
}
