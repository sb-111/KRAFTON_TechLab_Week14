#include "pch.h"
#include "SlateManager.h"
#include "SPhysicsAssetEditorWindow.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Renderer/FViewport.h"
#include "Source/Runtime/Engine/Physics/PhysicsAsset.h"
#include "Source/Runtime/Engine/Physics/BodySetup.h"
#include "Source/Runtime/Engine/Physics/ConstraintInstance.h"

SPhysicsAssetEditorWindow::SPhysicsAssetEditorWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
    LeftPanelRatio = 0.2f;
    RightPanelRatio = 0.25f;
}

SPhysicsAssetEditorWindow::~SPhysicsAssetEditorWindow()
{
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        DestroyViewerState(State);
    }
    Tabs.Empty();
    ActiveState = nullptr;
}

void SPhysicsAssetEditorWindow::OnRender()
{
    if (!bIsOpen)
    {
        USlateManager::GetInstance().RequestCloseDetachedWindow(this);
        return;
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
        ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
        bInitialPlacementDone = true;
    }

    if (bRequestFocus)
    {
        ImGui::SetNextWindowFocus();
    }

    char UniqueTitle[256];
    FString Title = GetWindowTitle();
    sprintf_s(UniqueTitle, sizeof(UniqueTitle), "%s###%p", Title.c_str(), this);

    bool bViewerVisible = false;
    if (ImGui::Begin(UniqueTitle, &bIsOpen, flags))
    {
        bViewerVisible = true;
        bIsWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        bIsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        RenderTabsAndToolbar(EViewerType::PhysicsAsset);

        if (!bIsOpen)
        {
            USlateManager::GetInstance().RequestCloseDetachedWindow(this);
            ImGui::End();
            return;
        }

        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y;
        Rect.UpdateMinMax();

        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        float totalWidth = contentAvail.x;
        float totalHeight = contentAvail.y;
        float splitterWidth = 4.0f;

        float leftWidth = totalWidth * LeftPanelRatio;
        float rightWidth = totalWidth * RightPanelRatio;
        float centerWidth = totalWidth - leftWidth - rightWidth - (splitterWidth * 2);
        if (centerWidth < 0.0f) centerWidth = 0.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        // Left panel
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, totalHeight), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
        RenderLeftPanel(leftWidth);
        ImGui::EndChild();

        ImGui::SameLine(0, 0);

        // Left splitter
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
        ImGui::Button("##LeftSplitter", ImVec2(splitterWidth, totalHeight));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.x;
            if (delta != 0.0f)
            {
                float newLeftRatio = LeftPanelRatio + delta / totalWidth;
                float maxLeftRatio = 1.0f - RightPanelRatio - (splitterWidth * 2) / totalWidth;
                LeftPanelRatio = std::max(0.1f, std::min(newLeftRatio, maxLeftRatio));
            }
        }

        ImGui::SameLine(0, 0);

        // Center panel
        if (centerWidth > 0.0f)
        {
            ImGui::BeginChild("CenterPanel", ImVec2(centerWidth, totalHeight), false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);

            RenderViewerToolbar();

            float viewportHeight = ImGui::GetContentRegionAvail().y;
            float viewportWidth = ImGui::GetContentRegionAvail().x;

            ImVec2 Pos = ImGui::GetCursorScreenPos();
            CenterRect.Left = Pos.x;
            CenterRect.Top = Pos.y;
            CenterRect.Right = Pos.x + viewportWidth;
            CenterRect.Bottom = Pos.y + viewportHeight;
            CenterRect.UpdateMinMax();

            OnRenderViewport();

            if (ActiveState && ActiveState->Viewport)
            {
                ID3D11ShaderResourceView* SRV = ActiveState->Viewport->GetSRV();
                if (SRV)
                {
                    ImGui::Image((void*)SRV, ImVec2(viewportWidth, viewportHeight));
                    ActiveState->Viewport->SetViewportHovered(ImGui::IsItemHovered());
                }
                else
                {
                    ImGui::Dummy(ImVec2(viewportWidth, viewportHeight));
                }
            }
            else
            {
                ImGui::Dummy(ImVec2(viewportWidth, viewportHeight));
            }

            ImGui::EndChild();
            ImGui::SameLine(0, 0);
        }
        else
        {
            CenterRect = FRect(0, 0, 0, 0);
            CenterRect.UpdateMinMax();
        }

        // Right splitter
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
        ImGui::Button("##RightSplitter", ImVec2(splitterWidth, totalHeight));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.x;
            if (delta != 0.0f)
            {
                float newRightRatio = RightPanelRatio - delta / totalWidth;
                float maxRightRatio = 1.0f - LeftPanelRatio - (splitterWidth * 2) / totalWidth;
                RightPanelRatio = std::max(0.1f, std::min(newRightRatio, maxRightRatio));
            }
        }

        ImGui::SameLine(0, 0);

        // Right panel
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("RightPanel", ImVec2(rightWidth, totalHeight), true);
        ImGui::PopStyleVar();
        RenderRightPanel();
        ImGui::EndChild();

        ImGui::PopStyleVar();
    }
    ImGui::End();

    if (!bViewerVisible)
    {
        CenterRect = FRect(0, 0, 0, 0);
        CenterRect.UpdateMinMax();
        bIsWindowHovered = false;
        bIsWindowFocused = false;
    }

    if (!bIsOpen)
    {
        USlateManager::GetInstance().RequestCloseDetachedWindow(this);
    }

    bRequestFocus = false;
}

void SPhysicsAssetEditorWindow::OnUpdate(float DeltaSeconds)
{
    SViewerWindow::OnUpdate(DeltaSeconds);
}

void SPhysicsAssetEditorWindow::PreRenderViewportUpdate()
{
    if (!ActiveState || !ActiveState->PreviewActor) return;

    if (ActiveState->bShowBones && ActiveState->CurrentMesh && ActiveState->bBoneLinesDirty)
    {
        if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
        {
            LineComp->SetLineVisible(true);
        }
        ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex);
        ActiveState->bBoneLinesDirty = false;
    }
}

void SPhysicsAssetEditorWindow::OnSave()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (PhysState && PhysState->bIsDirty)
    {
        PhysState->bIsDirty = false;
        UE_LOG("Physics Asset saved: %s", PhysState->CurrentFilePath.c_str());
    }
}

ViewerState* SPhysicsAssetEditorWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
    return PhysicsAssetEditorBootstrap::CreateViewerState(Name, World, Device, Context);
}

void SPhysicsAssetEditorWindow::DestroyViewerState(ViewerState*& State)
{
    PhysicsAssetEditorBootstrap::DestroyViewerState(State);
}

void SPhysicsAssetEditorWindow::OnSkeletalMeshLoaded(ViewerState* State, const FString& Path)
{
    if (!State || !State->CurrentMesh) return;

    PhysicsAssetEditorState* PhysState = static_cast<PhysicsAssetEditorState*>(State);
    if (PhysState->EditingAsset)
    {
        PhysState->EditingAsset->Bodies.Empty();
        PhysState->EditingAsset->Constraints.Empty();
    }
    UE_LOG("SPhysicsAssetEditorWindow: Loaded skeletal mesh from %s", Path.c_str());
}

void SPhysicsAssetEditorWindow::RenderLeftPanel(float PanelWidth)
{
    if (!ActiveState) return;

    float totalHeight = ImGui::GetContentRegionAvail().y;
    float splitterHeight = 4.0f;
    float treeHeight = (totalHeight - splitterHeight) * LeftPanelSplitRatio;
    float graphHeight = (totalHeight - splitterHeight) * (1.0f - LeftPanelSplitRatio);

    RenderSkeletonTreePanel(PanelWidth, treeHeight);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
    ImGui::Button("##LeftPanelSplitter", ImVec2(PanelWidth - 16, splitterHeight));
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if (ImGui::IsItemActive())
    {
        float delta = ImGui::GetIO().MouseDelta.y;
        if (delta != 0.0f)
        {
            float newRatio = LeftPanelSplitRatio + delta / totalHeight;
            LeftPanelSplitRatio = std::clamp(newRatio, 0.2f, 0.8f);
        }
    }

    RenderGraphViewPanel(PanelWidth, graphHeight);
}

void SPhysicsAssetEditorWindow::RenderRightPanel()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;

    ImGui::Text("Details");
    ImGui::Separator();
    ImGui::Spacing();

    if (PhysState->SelectedBodyIndex >= 0 && PhysState->EditingAsset)
    {
        if (PhysState->SelectedBodyIndex < PhysState->EditingAsset->Bodies.Num())
        {
            UBodySetup* Body = PhysState->EditingAsset->Bodies[PhysState->SelectedBodyIndex];
            if (Body) RenderBodyDetails(Body);
        }
    }
    else if (PhysState->SelectedConstraintIndex >= 0 && PhysState->EditingAsset)
    {
        if (PhysState->SelectedConstraintIndex < PhysState->EditingAsset->Constraints.Num())
        {
            FConstraintInstance* Constraint = &PhysState->EditingAsset->Constraints[PhysState->SelectedConstraintIndex];
            RenderConstraintDetails(Constraint);
        }
    }
    else
    {
        ImGui::TextDisabled("Select a body or constraint to edit");
    }
}

void SPhysicsAssetEditorWindow::RenderBottomPanel()
{
}

PhysicsAssetEditorState* SPhysicsAssetEditorWindow::GetPhysicsState() const
{
    return static_cast<PhysicsAssetEditorState*>(ActiveState);
}

void SPhysicsAssetEditorWindow::RenderSkeletonTreePanel(float Width, float Height)
{
    ImGui::BeginChild("SkeletonTree", ImVec2(Width - 16, Height), false);
    ImGui::Text("Skeleton");
    ImGui::Separator();

    if (!ActiveState || !ActiveState->CurrentMesh)
    {
        ImGui::TextDisabled("Load a skeletal mesh first");
        ImGui::EndChild();
        return;
    }

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || Skeleton->Bones.empty())
    {
        ImGui::TextDisabled("No skeleton data");
        ImGui::EndChild();
        return;
    }

    for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
    {
        if (Skeleton->Bones[i].ParentIndex == -1)
        {
            RenderBoneTreeNode(i, 0);
        }
    }

    ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::RenderBoneTreeNode(int32 BoneIndex, int32 Depth)
{
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= (int32)Skeleton->Bones.size()) return;

    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    const FBone& Bone = Skeleton->Bones[BoneIndex];

    TArray<int32> ChildIndices;
    for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
    {
        if (Skeleton->Bones[i].ParentIndex == BoneIndex)
            ChildIndices.Add(i);
    }

    bool bHasBody = false;
    int32 BodyIndex = -1;
    if (PhysState && PhysState->EditingAsset)
    {
        for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
        {
            if (PhysState->EditingAsset->Bodies[i] &&
                PhysState->EditingAsset->Bodies[i]->BoneName == Bone.Name)
            {
                bHasBody = true;
                BodyIndex = i;
                break;
            }
        }
    }

    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (ChildIndices.Num() == 0) nodeFlags |= ImGuiTreeNodeFlags_Leaf;

    bool bSelected = (PhysState && PhysState->SelectedBodyIndex == BodyIndex && BodyIndex >= 0);
    if (bSelected) nodeFlags |= ImGuiTreeNodeFlags_Selected;

    // Bone.Name is already FString
    const FString& BoneName = Bone.Name;
    FString Label = bHasBody ? (FString("* ") + BoneName) : BoneName;

    bool bOpen = ImGui::TreeNodeEx((void*)(intptr_t)BoneIndex, nodeFlags, "%s", Label.c_str());

    if (ImGui::IsItemClicked())
    {
        if (bHasBody)
            SelectBody(BodyIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
        ActiveState->SelectedBoneIndex = BoneIndex;
    }

    if (bOpen)
    {
        for (int32 ChildIndex : ChildIndices)
            RenderBoneTreeNode(ChildIndex, Depth + 1);
        ImGui::TreePop();
    }
}

void SPhysicsAssetEditorWindow::RenderGraphViewPanel(float Width, float Height)
{
    ImGui::BeginChild("GraphView", ImVec2(Width - 16, Height), false);
    ImGui::Text("Graph");
    ImGui::Separator();

    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset)
    {
        ImGui::TextDisabled("No physics asset");
        ImGui::EndChild();
        return;
    }

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    DrawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(30, 30, 30, 255));

    float gridSize = 20.0f * PhysState->GraphZoom;
    for (float x = fmodf(PhysState->GraphOffset.X, gridSize); x < canvasSize.x; x += gridSize)
    {
        DrawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y),
            ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y), IM_COL32(50, 50, 50, 255));
    }
    for (float y = fmodf(PhysState->GraphOffset.Y, gridSize); y < canvasSize.y; y += gridSize)
    {
        DrawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y),
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y), IM_COL32(50, 50, 50, 255));
    }

    if (PhysState->SelectedBodyIndex >= 0 && PhysState->SelectedBodyIndex < PhysState->EditingAsset->Bodies.Num())
    {
        UBodySetup* Body = PhysState->EditingAsset->Bodies[PhysState->SelectedBodyIndex];
        if (Body)
        {
            FVector2D centerPos(canvasSize.x * 0.5f, canvasSize.y * 0.5f);
            bool bSelectedFromGraph = (PhysState->SelectionSource == PhysicsAssetEditorState::ESelectionSource::Graph);
            uint32 color = bSelectedFromGraph ? IM_COL32(150, 100, 200, 255) : IM_COL32(70, 130, 180, 255);
            RenderGraphNode(centerPos, Body->BoneName.ToString(), true, true, color);
        }
    }
    else
    {
        // Use Dummy to advance cursor instead of SetCursorScreenPos
        ImGui::Dummy(ImVec2(10, 30));
        ImGui::TextDisabled("Select a body to view connections");
    }

    // Ensure valid size for InvisibleButton
    if (canvasSize.x > 1 && canvasSize.y > 1)
    {
        ImGui::InvisibleButton("GraphCanvas", canvasSize);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        {
            PhysState->GraphOffset.X += ImGui::GetIO().MouseDelta.x;
            PhysState->GraphOffset.Y += ImGui::GetIO().MouseDelta.y;
        }
        if (ImGui::IsItemHovered())
        {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
                PhysState->GraphZoom = std::clamp(PhysState->GraphZoom + wheel * 0.1f, 0.5f, 2.0f);
        }
    }

    ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::RenderGraphNode(const FVector2D& Position, const FString& Label, bool bIsBody, bool bSelected, uint32 Color)
{
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    float nodeWidth = 120.0f;
    float nodeHeight = 40.0f;

    ImVec2 nodeMin(canvasPos.x + Position.X - nodeWidth * 0.5f, canvasPos.y + Position.Y - nodeHeight * 0.5f);
    ImVec2 nodeMax(nodeMin.x + nodeWidth, nodeMin.y + nodeHeight);

    DrawList->AddRectFilled(nodeMin, nodeMax, Color, 4.0f);
    if (bSelected)
        DrawList->AddRect(nodeMin, nodeMax, IM_COL32(255, 255, 255, 255), 4.0f, 0, 2.0f);

    ImVec2 textSize = ImGui::CalcTextSize(Label.c_str());
    ImVec2 textPos(nodeMin.x + (nodeWidth - textSize.x) * 0.5f, nodeMin.y + (nodeHeight - textSize.y) * 0.5f);
    DrawList->AddText(textPos, IM_COL32(255, 255, 255, 255), Label.c_str());
}

void SPhysicsAssetEditorWindow::RenderBodyDetails(UBodySetup* Body)
{
    if (!Body) return;

    PhysicsAssetEditorState* PhysState = GetPhysicsState();

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));

    if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();

        float mass = Body->MassInKg;
        ImGui::Text("Mass (kg)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##Mass", &mass, 0.1f, 0.0f, 10000.0f, "%.2f"))
        {
            Body->MassInKg = mass;
            if (PhysState) PhysState->bIsDirty = true;
        }

        float linearDamping = Body->LinearDamping;
        ImGui::Text("Linear Damping");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##LinearDamping", &linearDamping, 0.01f, 0.0f, 100.0f, "%.3f"))
        {
            Body->LinearDamping = linearDamping;
            if (PhysState) PhysState->bIsDirty = true;
        }

        float angularDamping = Body->AngularDamping;
        ImGui::Text("Angular Damping");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##AngularDamping", &angularDamping, 0.01f, 0.0f, 100.0f, "%.3f"))
        {
            Body->AngularDamping = angularDamping;
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Body Setup", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        ImGui::Text("Bone Name");
        ImGui::TextDisabled("%s", Body->BoneName.ToString().c_str());

        int32 shapeCount = Body->AggGeom.GetElementCount();
        ImGui::Text("Shapes: %d", shapeCount);
        ImGui::Unindent();
    }

    ImGui::PopStyleColor();
}

void SPhysicsAssetEditorWindow::RenderConstraintDetails(FConstraintInstance* Constraint)
{
    if (!Constraint) return;

    PhysicsAssetEditorState* PhysState = GetPhysicsState();

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));

    if (ImGui::CollapsingHeader("Constraint", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        ImGui::Text("Bone 1 (Parent)");
        ImGui::TextDisabled("%s", Constraint->ConstraintBone1.ToString().c_str());
        ImGui::Text("Bone 2 (Child)");
        ImGui::TextDisabled("%s", Constraint->ConstraintBone2.ToString().c_str());
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Linear Limits", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        const char* motionModes[] = { "Free", "Limited", "Locked" };

        ImGui::Text("X Motion");
        int xMotion = static_cast<int>(Constraint->LinearXMotion);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##XMotion", &xMotion, motionModes, 3))
        {
            Constraint->LinearXMotion = static_cast<ELinearConstraintMotion>(xMotion);
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Text("Y Motion");
        int yMotion = static_cast<int>(Constraint->LinearYMotion);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##YMotion", &yMotion, motionModes, 3))
        {
            Constraint->LinearYMotion = static_cast<ELinearConstraintMotion>(yMotion);
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Text("Z Motion");
        int zMotion = static_cast<int>(Constraint->LinearZMotion);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##ZMotion", &zMotion, motionModes, 3))
        {
            Constraint->LinearZMotion = static_cast<ELinearConstraintMotion>(zMotion);
            if (PhysState) PhysState->bIsDirty = true;
        }

        float limit = Constraint->LinearLimit;
        ImGui::Text("Limit");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##LinearLimit", &limit, 0.1f, 0.0f, 1000.0f, "%.1f"))
        {
            Constraint->LinearLimit = limit;
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Angular Limits", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        const char* angularMotionModes[] = { "Free", "Limited", "Locked" };

        ImGui::Text("Swing 1 Motion");
        int swing1 = static_cast<int>(Constraint->Swing1Motion);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##Swing1Motion", &swing1, angularMotionModes, 3))
        {
            Constraint->Swing1Motion = static_cast<EAngularConstraintMotion>(swing1);
            if (PhysState) PhysState->bIsDirty = true;
        }

        float swing1Limit = Constraint->Swing1LimitAngle;
        ImGui::Text("Swing 1 Limit");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##Swing1Limit", &swing1Limit, 0.5f, 0.0f, 180.0f, "%.1f"))
        {
            Constraint->Swing1LimitAngle = swing1Limit;
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Text("Swing 2 Motion");
        int swing2 = static_cast<int>(Constraint->Swing2Motion);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##Swing2Motion", &swing2, angularMotionModes, 3))
        {
            Constraint->Swing2Motion = static_cast<EAngularConstraintMotion>(swing2);
            if (PhysState) PhysState->bIsDirty = true;
        }

        float swing2Limit = Constraint->Swing2LimitAngle;
        ImGui::Text("Swing 2 Limit");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##Swing2Limit", &swing2Limit, 0.5f, 0.0f, 180.0f, "%.1f"))
        {
            Constraint->Swing2LimitAngle = swing2Limit;
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Text("Twist Motion");
        int twist = static_cast<int>(Constraint->TwistMotion);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##TwistMotion", &twist, angularMotionModes, 3))
        {
            Constraint->TwistMotion = static_cast<EAngularConstraintMotion>(twist);
            if (PhysState) PhysState->bIsDirty = true;
        }

        float twistLimit = Constraint->TwistLimitAngle;
        ImGui::Text("Twist Limit");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##TwistLimit", &twistLimit, 0.5f, 0.0f, 180.0f, "%.1f"))
        {
            Constraint->TwistLimitAngle = twistLimit;
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Unindent();
    }

    ImGui::PopStyleColor();
}

void SPhysicsAssetEditorWindow::RenderViewportOverlay()
{
    // TODO
}

void SPhysicsAssetEditorWindow::RenderPhysicsBodies()
{
    // TODO
}

void SPhysicsAssetEditorWindow::RenderConstraintVisuals()
{
    // TODO
}

void SPhysicsAssetEditorWindow::SelectBody(int32 Index, PhysicsAssetEditorState::ESelectionSource Source)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;
    PhysState->SelectedBodyIndex = Index;
    PhysState->SelectedConstraintIndex = -1;
    PhysState->SelectionSource = Source;
}

void SPhysicsAssetEditorWindow::SelectConstraint(int32 Index)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;
    PhysState->SelectedConstraintIndex = Index;
    PhysState->SelectedBodyIndex = -1;
}

void SPhysicsAssetEditorWindow::ClearSelection()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;
    PhysState->SelectedBodyIndex = -1;
    PhysState->SelectedConstraintIndex = -1;
    PhysState->SelectedShapeIndex = -1;
}
