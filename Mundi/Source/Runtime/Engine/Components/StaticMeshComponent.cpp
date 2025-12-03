#include "pch.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "Shader.h"
#include "ResourceManager.h"
#include "ObjManager.h"
#include "JsonSerializer.h"
#include "CameraComponent.h"
#include "MeshBatchElement.h"
#include "Material.h"
#include "SceneView.h"
#include "LuaBindHelpers.h"
#include "BodySetup.h"
#include "CollisionShapes.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UStaticMeshComponent::UStaticMeshComponent()
{
	SetStaticMesh(GDataDir + "/cube-tex.obj");     // 임시 기본 static mesh 설정
}

UStaticMeshComponent::~UStaticMeshComponent() = default;

void UStaticMeshComponent::OnStaticMeshReleased(UStaticMesh* ReleasedMesh)
{
	// TODO : 왜 this가 없는지 추적 필요!
	if (!this || !StaticMesh || StaticMesh != ReleasedMesh)
	{
		return;
	}

	StaticMesh = nullptr;
}

void UStaticMeshComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	if (!StaticMesh || !StaticMesh->GetStaticMeshAsset())
	{
		return;
	}

	const TArray<FGroupInfo>& MeshGroupInfos = StaticMesh->GetMeshGroupInfo();

	auto DetermineMaterialAndShader = [&](uint32 SectionIndex) -> TPair<UMaterialInterface*, UShader*>
		{
			UMaterialInterface* Material = GetMaterial(SectionIndex);
			UShader* Shader = nullptr;

			if (Material && Material->GetShader())
			{
				Shader = Material->GetShader();
			}
			else
			{
				UE_LOG("UStaticMeshComponent: 머티리얼이 없거나 셰이더가 없어서 기본 머티리얼 사용 section %u.", SectionIndex);
				Material = UResourceManager::GetInstance().GetDefaultMaterial();
				if (Material)
				{
					Shader = Material->GetShader();
				}
				if (!Material || !Shader)
				{
					UE_LOG("UStaticMeshComponent: 기본 머티리얼이 없습니다.");
					return { nullptr, nullptr };
				}
			}
			return { Material, Shader };
		};

	const bool bHasSections = !MeshGroupInfos.IsEmpty();
	const uint32 NumSectionsToProcess = bHasSections ? static_cast<uint32>(MeshGroupInfos.size()) : 1;

	for (uint32 SectionIndex = 0; SectionIndex < NumSectionsToProcess; ++SectionIndex)
	{
		uint32 IndexCount = 0;
		uint32 StartIndex = 0;

		if (bHasSections)
		{
			const FGroupInfo& Group = MeshGroupInfos[SectionIndex];
			IndexCount = Group.IndexCount;
			StartIndex = Group.StartIndex;
		}
		else
		{
			IndexCount = StaticMesh->GetIndexCount();
			StartIndex = 0;
		}

		if (IndexCount == 0)
		{
			continue;
		}

		auto [MaterialToUse, ShaderToUse] = DetermineMaterialAndShader(SectionIndex);
		if (!MaterialToUse || !ShaderToUse)
		{
			continue;
		}

		FMeshBatchElement BatchElement;
		// View 모드 전용 매크로와 머티리얼 개인 매크로를 결합한다
		TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;
		if (0 < MaterialToUse->GetShaderMacros().Num())
		{
			ShaderMacros.Append(MaterialToUse->GetShaderMacros());
		}
		FShaderVariant* ShaderVariant = ShaderToUse->GetOrCompileShaderVariant(ShaderMacros);

		if (ShaderVariant)
		{
			BatchElement.VertexShader = ShaderVariant->VertexShader;
			BatchElement.PixelShader = ShaderVariant->PixelShader;
			BatchElement.InputLayout = ShaderVariant->InputLayout;
		}

		// UMaterialInterface를 UMaterial로 캐스팅해야 할 수 있음. 렌더러가 UMaterial을 기대한다면.
		// 지금은 Material.h 구조상 UMaterialInterface에 필요한 정보가 다 있음.
		BatchElement.Material = MaterialToUse;
		BatchElement.VertexBuffer = StaticMesh->GetVertexBuffer();
		BatchElement.IndexBuffer = StaticMesh->GetIndexBuffer();
		BatchElement.VertexStride = StaticMesh->GetVertexStride();
		BatchElement.IndexCount = IndexCount;
		BatchElement.StartIndex = StartIndex;
		BatchElement.BaseVertexIndex = 0;
		BatchElement.WorldMatrix = GetWorldMatrix();
		BatchElement.ObjectID = InternalIndex;
		BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		OutMeshBatchElements.Add(BatchElement);
	}
}

void UStaticMeshComponent::SetStaticMesh(const FString& PathFileName)
{
	// 새 메시를 설정하기 전에, 기존에 생성된 모든 MID와 슬롯 정보를 정리합니다.
	ClearDynamicMaterials();

	// 새 메시를 로드합니다.
	StaticMesh = UResourceManager::GetInstance().Load<UStaticMesh>(PathFileName);
	if (StaticMesh && StaticMesh->GetStaticMeshAsset())
	{
		const TArray<FGroupInfo>& GroupInfos = StaticMesh->GetMeshGroupInfo();

		// 4. 새 메시 정보에 맞게 슬롯을 재설정합니다.
		MaterialSlots.resize(GroupInfos.size()); // ClearDynamicMaterials()에서 비워졌으므로, 새 크기로 재할당

		for (int i = 0; i < GroupInfos.size(); ++i)
		{
			SetMaterialByName(i, GroupInfos[i].InitialMaterialName);
		}
		MarkWorldPartitionDirty();
	}
	else
	{
		// 메시 로드에 실패한 경우, StaticMesh 포인터를 nullptr로 보장합니다.
		// (슬롯은 이미 위에서 비워졌습니다.)
		StaticMesh = nullptr;
	}
}

FAABB UStaticMeshComponent::GetWorldAABB() const
{
	const FTransform WorldTransform = GetWorldTransform();
	const FMatrix WorldMatrix = GetWorldMatrix();

	if (!StaticMesh)
	{
		const FVector Origin = WorldTransform.TransformPosition(FVector());
		return FAABB(Origin, Origin);
	}

	const FAABB LocalBound = StaticMesh->GetLocalBound();
	const FVector LocalMin = LocalBound.Min;
	const FVector LocalMax = LocalBound.Max;

	const FVector LocalCorners[8] = {
		FVector(LocalMin.X, LocalMin.Y, LocalMin.Z),
		FVector(LocalMax.X, LocalMin.Y, LocalMin.Z),
		FVector(LocalMin.X, LocalMax.Y, LocalMin.Z),
		FVector(LocalMax.X, LocalMax.Y, LocalMin.Z),
		FVector(LocalMin.X, LocalMin.Y, LocalMax.Z),
		FVector(LocalMax.X, LocalMin.Y, LocalMax.Z),
		FVector(LocalMin.X, LocalMax.Y, LocalMax.Z),
		FVector(LocalMax.X, LocalMax.Y, LocalMax.Z)
	};

	FVector4 WorldMin4 = FVector4(LocalCorners[0].X, LocalCorners[0].Y, LocalCorners[0].Z, 1.0f) * WorldMatrix;
	FVector4 WorldMax4 = WorldMin4;

	for (int32 CornerIndex = 1; CornerIndex < 8; ++CornerIndex)
	{
		const FVector4 WorldPos = FVector4(LocalCorners[CornerIndex].X
			, LocalCorners[CornerIndex].Y
			, LocalCorners[CornerIndex].Z
			, 1.0f)
			* WorldMatrix;
		WorldMin4 = WorldMin4.ComponentMin(WorldPos);
		WorldMax4 = WorldMax4.ComponentMax(WorldPos);
	}

	FVector WorldMin = FVector(WorldMin4.X, WorldMin4.Y, WorldMin4.Z);
	FVector WorldMax = FVector(WorldMax4.X, WorldMax4.Y, WorldMax4.Z);
	return FAABB(WorldMin, WorldMax);
}

void UStaticMeshComponent::OnTransformUpdated()
{
	Super::OnTransformUpdated();
	MarkWorldPartitionDirty();
}

void UStaticMeshComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void UStaticMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// BodySetup 로드
		if (InOutHandle.hasKey("BodySetup") && StaticMesh)
		{
			UBodySetup* BodySetup = StaticMesh->GetBodySetup();
			if (!BodySetup)
			{
				BodySetup = NewObject<UBodySetup>();
				StaticMesh->SetBodySetup(BodySetup);
			}

			JSON& BodyJson = InOutHandle["BodySetup"];

			// AggGeom 로드
			if (BodyJson.hasKey("AggGeom"))
			{
				JSON& AggGeomJson = BodyJson["AggGeom"];

				// Sphere Elements
				if (AggGeomJson.hasKey("SphereElems"))
				{
					BodySetup->AggGeom.SphereElems.Empty();
					for (auto& SphereJson : AggGeomJson["SphereElems"].ArrayRange())
					{
						FKSphereElem Sphere;
						Sphere.Name = FName(SphereJson["Name"].ToString());
						Sphere.Center = FJsonSerializer::JsonToVector(SphereJson["Center"]);
						Sphere.Radius = SphereJson["Radius"].ToFloat();
						Sphere.CollisionEnabled = static_cast<ECollisionEnabled>(SphereJson["CollisionEnabled"].ToInt());
						BodySetup->AggGeom.SphereElems.Add(Sphere);
					}
				}

				// Box Elements
				if (AggGeomJson.hasKey("BoxElems"))
				{
					BodySetup->AggGeom.BoxElems.Empty();
					for (auto& BoxJson : AggGeomJson["BoxElems"].ArrayRange())
					{
						FKBoxElem Box;
						Box.Name = FName(BoxJson["Name"].ToString());
						Box.Center = FJsonSerializer::JsonToVector(BoxJson["Center"]);
						Box.Rotation = FJsonSerializer::JsonToVector(BoxJson["Rotation"]);
						Box.X = BoxJson["X"].ToFloat();
						Box.Y = BoxJson["Y"].ToFloat();
						Box.Z = BoxJson["Z"].ToFloat();
						Box.CollisionEnabled = static_cast<ECollisionEnabled>(BoxJson["CollisionEnabled"].ToInt());
						BodySetup->AggGeom.BoxElems.Add(Box);
					}
				}

				// Capsule (Sphyl) Elements
				if (AggGeomJson.hasKey("SphylElems"))
				{
					BodySetup->AggGeom.SphylElems.Empty();
					for (auto& CapsuleJson : AggGeomJson["SphylElems"].ArrayRange())
					{
						FKSphylElem Capsule;
						Capsule.Name = FName(CapsuleJson["Name"].ToString());
						Capsule.Center = FJsonSerializer::JsonToVector(CapsuleJson["Center"]);
						Capsule.Rotation = FJsonSerializer::JsonToVector(CapsuleJson["Rotation"]);
						Capsule.Radius = CapsuleJson["Radius"].ToFloat();
						Capsule.Length = CapsuleJson["Length"].ToFloat();
						Capsule.CollisionEnabled = static_cast<ECollisionEnabled>(CapsuleJson["CollisionEnabled"].ToInt());
						BodySetup->AggGeom.SphylElems.Add(Capsule);
					}
				}

				// Convex Elements
				if (AggGeomJson.hasKey("ConvexElems"))
				{
					BodySetup->AggGeom.ConvexElems.Empty();
					for (auto& ConvexJson : AggGeomJson["ConvexElems"].ArrayRange())
					{
						FKConvexElem Convex;
						Convex.Name = FName(ConvexJson["Name"].ToString());

						// Transform 로드
						if (ConvexJson.hasKey("Transform"))
						{
							JSON& TransformJson = ConvexJson["Transform"];
							if (TransformJson.hasKey("Translation"))
							{
								Convex.Transform.Translation = FJsonSerializer::JsonToVector(TransformJson["Translation"]);
							}
							if (TransformJson.hasKey("Rotation"))
							{
								Convex.Transform.Rotation = FJsonSerializer::JsonToQuat(TransformJson["Rotation"]);
							}
							if (TransformJson.hasKey("Scale3D"))
							{
								Convex.Transform.Scale3D = FJsonSerializer::JsonToVector(TransformJson["Scale3D"]);
							}
						}

						// Vertices
						if (ConvexJson.hasKey("VertexData"))
						{
							for (auto& VertJson : ConvexJson["VertexData"].ArrayRange())
							{
								Convex.VertexData.Add(FJsonSerializer::JsonToVector(VertJson));
							}
						}

						// Indices
						if (ConvexJson.hasKey("IndexData"))
						{
							for (auto& IdxJson : ConvexJson["IndexData"].ArrayRange())
							{
								Convex.IndexData.Add(IdxJson.ToInt());
							}
						}

						// CollisionEnabled
						if (ConvexJson.hasKey("CollisionEnabled"))
						{
							Convex.CollisionEnabled = static_cast<ECollisionEnabled>(ConvexJson["CollisionEnabled"].ToInt());
						}

						BodySetup->AggGeom.ConvexElems.Add(Convex);
					}
				}
			}
		}
	}
	else
	{
		// BodySetup 저장
		if (StaticMesh)
		{
			UBodySetup* BodySetup = StaticMesh->GetBodySetup();
			if (BodySetup && BodySetup->AggGeom.GetElementCount() > 0)
			{
				JSON BodyJson = JSON::Make(JSON::Class::Object);
				JSON AggGeomJson = JSON::Make(JSON::Class::Object);

				// Sphere Elements
				JSON SphereArray = JSON::Make(JSON::Class::Array);
				for (const FKSphereElem& Sphere : BodySetup->AggGeom.SphereElems)
				{
					JSON SphereJson = JSON::Make(JSON::Class::Object);
					SphereJson["Name"] = Sphere.Name.ToString();
					SphereJson["Center"] = FJsonSerializer::VectorToJson(Sphere.Center);
					SphereJson["Radius"] = Sphere.Radius;
					SphereJson["CollisionEnabled"] = static_cast<int32>(Sphere.CollisionEnabled);
					SphereArray.append(SphereJson);
				}
				AggGeomJson["SphereElems"] = SphereArray;

				// Box Elements
				JSON BoxArray = JSON::Make(JSON::Class::Array);
				for (const FKBoxElem& Box : BodySetup->AggGeom.BoxElems)
				{
					JSON BoxJson = JSON::Make(JSON::Class::Object);
					BoxJson["Name"] = Box.Name.ToString();
					BoxJson["Center"] = FJsonSerializer::VectorToJson(Box.Center);
					BoxJson["Rotation"] = FJsonSerializer::VectorToJson(Box.Rotation);
					BoxJson["X"] = Box.X;
					BoxJson["Y"] = Box.Y;
					BoxJson["Z"] = Box.Z;
					BoxJson["CollisionEnabled"] = static_cast<int32>(Box.CollisionEnabled);
					BoxArray.append(BoxJson);
				}
				AggGeomJson["BoxElems"] = BoxArray;

				// Capsule (Sphyl) Elements
				JSON SphylArray = JSON::Make(JSON::Class::Array);
				for (const FKSphylElem& Capsule : BodySetup->AggGeom.SphylElems)
				{
					JSON CapsuleJson = JSON::Make(JSON::Class::Object);
					CapsuleJson["Name"] = Capsule.Name.ToString();
					CapsuleJson["Center"] = FJsonSerializer::VectorToJson(Capsule.Center);
					CapsuleJson["Rotation"] = FJsonSerializer::VectorToJson(Capsule.Rotation);
					CapsuleJson["Radius"] = Capsule.Radius;
					CapsuleJson["Length"] = Capsule.Length;
					CapsuleJson["CollisionEnabled"] = static_cast<int32>(Capsule.CollisionEnabled);
					SphylArray.append(CapsuleJson);
				}
				AggGeomJson["SphylElems"] = SphylArray;

				// Convex Elements
				JSON ConvexArray = JSON::Make(JSON::Class::Array);
				for (const FKConvexElem& Convex : BodySetup->AggGeom.ConvexElems)
				{
					JSON ConvexJson = JSON::Make(JSON::Class::Object);
					ConvexJson["Name"] = Convex.Name.ToString();

					// Transform 저장
					JSON TransformJson = JSON::Make(JSON::Class::Object);
					TransformJson["Translation"] = FJsonSerializer::VectorToJson(Convex.Transform.Translation);
					TransformJson["Rotation"] = FJsonSerializer::QuatToJson(Convex.Transform.Rotation);
					TransformJson["Scale3D"] = FJsonSerializer::VectorToJson(Convex.Transform.Scale3D);
					ConvexJson["Transform"] = TransformJson;

					// Vertices
					JSON VertexArray = JSON::Make(JSON::Class::Array);
					for (const FVector& V : Convex.VertexData)
					{
						VertexArray.append(FJsonSerializer::VectorToJson(V));
					}
					ConvexJson["VertexData"] = VertexArray;

					// Indices
					JSON IndexArray = JSON::Make(JSON::Class::Array);
					for (int32 Idx : Convex.IndexData)
					{
						IndexArray.append(Idx);
					}
					ConvexJson["IndexData"] = IndexArray;

					// CollisionEnabled
					ConvexJson["CollisionEnabled"] = static_cast<int32>(Convex.CollisionEnabled);

					ConvexArray.append(ConvexJson);
				}
				AggGeomJson["ConvexElems"] = ConvexArray;

				BodyJson["AggGeom"] = AggGeomJson;
				InOutHandle["BodySetup"] = BodyJson;
			}
		}
	}
}
