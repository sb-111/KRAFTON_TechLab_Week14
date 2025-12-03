#pragma once

class ViewerState;
class UWorld;
struct ID3D11Device;
class UEditorAssetPreviewContext;
class UPhysicsAsset;
class UPhysicalMaterial;

class PhysicsAssetEditorBootstrap
{
public:
	// ViewerState 생성
	static ViewerState* CreateViewerState(const char* Name, UWorld* InWorld,
		ID3D11Device* InDevice, UEditorAssetPreviewContext* Context);

	// ViewerState 소멸
	static void DestroyViewerState(ViewerState*& State);

	// Physics Asset을 JSON 파일로 저장 (FBX 경로도 함께 저장)
	static bool SavePhysicsAsset(UPhysicsAsset* Asset, const FString& FilePath, const FString& SourceFbxPath = "");

	// JSON 파일에서 Physics Asset 로드 (FBX 경로도 함께 반환)
	static UPhysicsAsset* LoadPhysicsAsset(const FString& FilePath, FString* OutSourceFbxPath = nullptr);

	// Physics Asset 파일 목록 스캔 (Data/Physics 폴더)
	// OutPaths: .physics 파일 경로들
	// OutDisplayNames: 표시용 이름들 (파일명)
	static void GetAllPhysicsAssetPaths(TArray<FString>& OutPaths, TArray<FString>& OutDisplayNames);

	// 특정 FBX와 매칭되는 Physics Asset 파일들만 반환
	// FbxPath: 매칭할 FBX 파일 경로
	static void GetCompatiblePhysicsAssetPaths(const FString& FbxPath, TArray<FString>& OutPaths, TArray<FString>& OutDisplayNames);

	// ===== Physical Material 에셋 관리 =====

	// Physical Material을 JSON 파일로 저장
	static bool SavePhysicalMaterial(UPhysicalMaterial* Material, const FString& FilePath);

	// JSON 파일에서 Physical Material 로드
	static UPhysicalMaterial* LoadPhysicalMaterial(const FString& FilePath);

	// Physical Material 에셋 파일 목록 스캔 (Data/Physics/Materials 폴더)
	static void GetAllPhysicalMaterialPaths(TArray<FString>& OutPaths, TArray<FString>& OutDisplayNames);
};
