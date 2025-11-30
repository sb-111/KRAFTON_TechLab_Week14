#pragma once
#include "SViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"

class FViewport;
class FViewportClient;
class UWorld;
struct ID3D11Device;
class UTexture;
class UPhysicsAsset;
class UBodySetup;
struct FConstraintInstance;

class SPhysicsAssetEditorWindow : public SViewerWindow
{
public:
    SPhysicsAssetEditorWindow();
    virtual ~SPhysicsAssetEditorWindow();

    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;
    virtual void PreRenderViewportUpdate() override;
    virtual void OnSave() override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;

protected:
    virtual ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) override;
    virtual void DestroyViewerState(ViewerState*& State) override;
    virtual FString GetWindowTitle() const override { return "Physics Asset Editor"; }
    virtual void OnSkeletalMeshLoaded(ViewerState* State, const FString& Path) override;

    virtual void RenderLeftPanel(float PanelWidth) override;
    virtual void RenderRightPanel() override;
    virtual void RenderBottomPanel() override;

private:
    // Helper to get typed state
    PhysicsAssetEditorState* GetPhysicsState() const;

    // Left Panel - Skeleton Tree (상단)
    void RenderSkeletonTreePanel(float Width, float Height);
    void RenderBoneTreeNode(int32 BoneIndex, int32 Depth = 0);
    void RenderBodyTreeNode(int32 BodyIndex, const FSkeleton* Skeleton);  // Hide Bones 모드용

    // Left Panel - Graph View (하단)
    void RenderGraphViewPanel(float Width, float Height);
    void RenderGraphNode(const FVector2D& Position, const FString& Label, bool bIsBody, bool bSelected, uint32 Color);

    // Right Panel - Details
    void RenderBodyDetails(UBodySetup* Body);
    void RenderConstraintDetails(FConstraintInstance* Constraint);

    // Viewport overlay
    void RenderViewportOverlay();

    // Physics body wireframe rendering
    void RenderPhysicsBodies();
    void RenderConstraintVisuals();

    // Selection helpers
    void SelectBody(int32 Index, PhysicsAssetEditorState::ESelectionSource Source);
    void SelectConstraint(int32 Index);
    void ClearSelection();

    // Body 생성/삭제
    void AddBodyToSelectedBone();
    void AddBodyToBone(int32 BoneIndex);
    void RemoveBody(int32 BodyIndex);
    UBodySetup* CreateDefaultBodySetup(const FString& BoneName);

    // Context menu
    void RenderBoneContextMenu(int32 BoneIndex, bool bHasBody, int32 BodyIndex);

    // Left panel splitter ratio (상단/하단 분할)
    float LeftPanelSplitRatio = 0.6f;  // 60% tree, 40% graph

    // Context menu state
    int32 ContextMenuBoneIndex = -1;

    // Skeleton Tree Settings
    void RenderSkeletonTreeSettings();

    // 본 표시 모드 (하나만 선택 가능)
    enum class EBoneDisplayMode
    {
        AllBones,       // 모든 본 표시
        MeshBones,      // 메시 본 표시
        HideBones       // 본 숨김 (Body만 표시)
    };

    // Shape 타입
    enum class EShapeType
    {
        Box,
        Sphere,
        Capsule
    };

    // 트리 표시 옵션
    struct FTreeDisplaySettings
    {
        EBoneDisplayMode BoneDisplayMode = EBoneDisplayMode::AllBones;
    };
    FTreeDisplaySettings TreeSettings;

    // Shape 추가 (기존 Body에 추가하거나 새 Body 생성)
    void AddShapeToBone(int32 BoneIndex, EShapeType ShapeType);
    void AddShapeToBody(UBodySetup* Body, EShapeType ShapeType);

    // Shape 타입별 Body 생성 (deprecated - use AddShapeToBone)
    void AddBodyToBoneWithShape(int32 BoneIndex, EShapeType ShapeType);
    UBodySetup* CreateBodySetupWithShape(const FString& BoneName, EShapeType ShapeType);
};
