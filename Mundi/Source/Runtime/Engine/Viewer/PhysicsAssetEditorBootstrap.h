#pragma once

class ViewerState;
class UWorld;
struct ID3D11Device;
class UEditorAssetPreviewContext;
class UPhysicsAsset;

class PhysicsAssetEditorBootstrap
{
public:
	// ViewerState 생성
	static ViewerState* CreateViewerState(const char* Name, UWorld* InWorld,
		ID3D11Device* InDevice, UEditorAssetPreviewContext* Context);

	// ViewerState 소멸
	static void DestroyViewerState(ViewerState*& State);

	// Physics Asset을 JSON 파일로 저장
	static bool SavePhysicsAsset(UPhysicsAsset* Asset, const FString& FilePath);

	// JSON 파일에서 Physics Asset 로드
	static UPhysicsAsset* LoadPhysicsAsset(const FString& FilePath);
};
