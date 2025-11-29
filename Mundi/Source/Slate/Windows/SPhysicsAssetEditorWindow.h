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

    // Left panel splitter ratio (상단/하단 분할)
    float LeftPanelSplitRatio = 0.6f;  // 60% tree, 40% graph
};
