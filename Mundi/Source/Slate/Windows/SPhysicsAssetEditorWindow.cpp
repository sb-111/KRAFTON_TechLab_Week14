#include "pch.h"
#include "SlateManager.h"
#include "SPhysicsAssetEditorWindow.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/World.h"
#include "Source/Runtime/Renderer/FViewport.h"
#include "Source/Runtime/Engine/Physics/PhysicsAsset.h"
#include "Source/Runtime/Engine/Physics/BodySetup.h"
#include "Source/Runtime/Engine/Physics/ConstraintInstance.h"
#include "Source/Runtime/Engine/Physics/AggregateGeometry.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/Engine/Components/ShapeAnchorComponent.h"
#include "Source/Runtime/Engine/Components/ConstraintAnchorComponent.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Renderer/FViewportClient.h"
#include "SelectionManager.h"
#include "PlatformProcess.h"
#include "PathUtils.h"

// Static 변수 정의
bool SPhysicsAssetEditorWindow::bIsAnyPhysicsAssetEditorFocused = false;

// ========== Shape 와이어프레임 생성 함수들 ==========

static const float PI_CONST = 3.14159265358979323846f;

// 박스 와이어프레임 (12개 라인) - 회전 지원
static void CreateBoxWireframe(ULineComponent* LineComp, const FVector& Center, const FVector& HalfExtent, const FQuat& Rotation, const FVector4& Color)
{
    // 로컬 좌표 꼭짓점
    FVector LocalCorners[8] = {
        FVector(-HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z),
        FVector(+HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z),
        FVector(+HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z),
        FVector(-HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z),
        FVector(-HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z),
        FVector(+HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z),
        FVector(+HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z),
        FVector(-HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z),
    };

    // 월드 좌표로 변환
    FVector Corners[8];
    for (int i = 0; i < 8; ++i)
    {
        Corners[i] = Center + Rotation.RotateVector(LocalCorners[i]);
    }

    // 아래 면 (4개 라인)
    LineComp->AddLine(Corners[0], Corners[1], Color);
    LineComp->AddLine(Corners[1], Corners[2], Color);
    LineComp->AddLine(Corners[2], Corners[3], Color);
    LineComp->AddLine(Corners[3], Corners[0], Color);

    // 위 면 (4개 라인)
    LineComp->AddLine(Corners[4], Corners[5], Color);
    LineComp->AddLine(Corners[5], Corners[6], Color);
    LineComp->AddLine(Corners[6], Corners[7], Color);
    LineComp->AddLine(Corners[7], Corners[4], Color);

    // 수직 연결 (4개 라인)
    LineComp->AddLine(Corners[0], Corners[4], Color);
    LineComp->AddLine(Corners[1], Corners[5], Color);
    LineComp->AddLine(Corners[2], Corners[6], Color);
    LineComp->AddLine(Corners[3], Corners[7], Color);
}

// 구 와이어프레임 (3개 원: XY, XZ, YZ 평면)
static void CreateSphereWireframe(ULineComponent* LineComp, const FVector& Center, float Radius, const FVector4& Color)
{
    const int32 NumSegments = 16;  // 성능을 위해 세그먼트 수 감소

    // XY 평면 원
    for (int32 i = 0; i < NumSegments; ++i)
    {
        float Angle1 = (float(i) / NumSegments) * 2.0f * PI_CONST;
        float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI_CONST;

        FVector P1 = Center + FVector(cos(Angle1) * Radius, sin(Angle1) * Radius, 0.0f);
        FVector P2 = Center + FVector(cos(Angle2) * Radius, sin(Angle2) * Radius, 0.0f);
        LineComp->AddLine(P1, P2, Color);
    }

    // XZ 평면 원
    for (int32 i = 0; i < NumSegments; ++i)
    {
        float Angle1 = (float(i) / NumSegments) * 2.0f * PI_CONST;
        float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI_CONST;

        FVector P1 = Center + FVector(cos(Angle1) * Radius, 0.0f, sin(Angle1) * Radius);
        FVector P2 = Center + FVector(cos(Angle2) * Radius, 0.0f, sin(Angle2) * Radius);
        LineComp->AddLine(P1, P2, Color);
    }

    // YZ 평면 원
    for (int32 i = 0; i < NumSegments; ++i)
    {
        float Angle1 = (float(i) / NumSegments) * 2.0f * PI_CONST;
        float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI_CONST;

        FVector P1 = Center + FVector(0.0f, cos(Angle1) * Radius, sin(Angle1) * Radius);
        FVector P2 = Center + FVector(0.0f, cos(Angle2) * Radius, sin(Angle2) * Radius);
        LineComp->AddLine(P1, P2, Color);
    }
}

// 캡슐 와이어프레임 (2개 반구 + 4개 세로선 + 2개 링)
static void CreateCapsuleWireframe(ULineComponent* LineComp, const FVector& Center, const FQuat& Rotation, float Radius, float HalfHeight, const FVector4& Color)
{
    const int32 NumSegments = 16;  // 성능을 위해 세그먼트 수 감소
    const int32 HemiSegments = 8;

    // 캡슐 방향 (Z축 기준)
    FVector Up = Rotation.RotateVector(FVector(0, 0, 1));
    FVector Right = Rotation.RotateVector(FVector(1, 0, 0));
    FVector Forward = Rotation.RotateVector(FVector(0, 1, 0));

    FVector TopCenter = Center + Up * HalfHeight;
    FVector BottomCenter = Center - Up * HalfHeight;

    // 상단/하단 원형 링
    for (int32 i = 0; i < NumSegments; ++i)
    {
        float Angle1 = (float(i) / NumSegments) * 2.0f * PI_CONST;
        float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI_CONST;

        FVector Offset1 = Right * cos(Angle1) * Radius + Forward * sin(Angle1) * Radius;
        FVector Offset2 = Right * cos(Angle2) * Radius + Forward * sin(Angle2) * Radius;

        // 상단 링
        LineComp->AddLine(TopCenter + Offset1, TopCenter + Offset2, Color);
        // 하단 링
        LineComp->AddLine(BottomCenter + Offset1, BottomCenter + Offset2, Color);
    }

    // 세로선 (4개)
    for (int32 i = 0; i < 4; ++i)
    {
        float Angle = (float(i) / 4) * 2.0f * PI_CONST;
        FVector Offset = Right * cos(Angle) * Radius + Forward * sin(Angle) * Radius;
        LineComp->AddLine(TopCenter + Offset, BottomCenter + Offset, Color);
    }

    // 상단 반구 (XZ, YZ 평면 반원)
    for (int32 i = 0; i < HemiSegments; ++i)
    {
        float Angle1 = (float(i) / HemiSegments) * PI_CONST * 0.5f;  // 0 ~ 90도
        float Angle2 = (float(i + 1) / HemiSegments) * PI_CONST * 0.5f;

        // XZ 평면 반원
        FVector P1 = TopCenter + Right * cos(Angle1) * Radius + Up * sin(Angle1) * Radius;
        FVector P2 = TopCenter + Right * cos(Angle2) * Radius + Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);

        P1 = TopCenter - Right * cos(Angle1) * Radius + Up * sin(Angle1) * Radius;
        P2 = TopCenter - Right * cos(Angle2) * Radius + Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);

        // YZ 평면 반원
        P1 = TopCenter + Forward * cos(Angle1) * Radius + Up * sin(Angle1) * Radius;
        P2 = TopCenter + Forward * cos(Angle2) * Radius + Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);

        P1 = TopCenter - Forward * cos(Angle1) * Radius + Up * sin(Angle1) * Radius;
        P2 = TopCenter - Forward * cos(Angle2) * Radius + Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);
    }

    // 하단 반구 (XZ, YZ 평면 반원)
    for (int32 i = 0; i < HemiSegments; ++i)
    {
        float Angle1 = (float(i) / HemiSegments) * PI_CONST * 0.5f;
        float Angle2 = (float(i + 1) / HemiSegments) * PI_CONST * 0.5f;

        // XZ 평면 반원
        FVector P1 = BottomCenter + Right * cos(Angle1) * Radius - Up * sin(Angle1) * Radius;
        FVector P2 = BottomCenter + Right * cos(Angle2) * Radius - Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);

        P1 = BottomCenter - Right * cos(Angle1) * Radius - Up * sin(Angle1) * Radius;
        P2 = BottomCenter - Right * cos(Angle2) * Radius - Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);

        // YZ 평면 반원
        P1 = BottomCenter + Forward * cos(Angle1) * Radius - Up * sin(Angle1) * Radius;
        P2 = BottomCenter + Forward * cos(Angle2) * Radius - Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);

        P1 = BottomCenter - Forward * cos(Angle1) * Radius - Up * sin(Angle1) * Radius;
        P2 = BottomCenter - Forward * cos(Angle2) * Radius - Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);
    }
}

// ========== 클래스 구현 ==========

SPhysicsAssetEditorWindow::SPhysicsAssetEditorWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
    LeftPanelRatio = 0.2f;
    RightPanelRatio = 0.25f;
}

SPhysicsAssetEditorWindow::~SPhysicsAssetEditorWindow()
{
    // 포커스 플래그 리셋 (윈도우 파괴 시)
    bIsAnyPhysicsAssetEditorFocused = false;

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

    // 아이콘 로드 (한 번만)
    if (!IconSave && Device)
    {
        IconSave = NewObject<UTexture>();
        IconSave->Load(GDataDir + "/Icon/Toolbar_Save.png", Device);

        IconSaveAs = NewObject<UTexture>();
        IconSaveAs->Load(GDataDir + "/Icon/Toolbar_SaveAs.png", Device);

        IconLoad = NewObject<UTexture>();
        IconLoad->Load(GDataDir + "/Icon/Toolbar_Load.png", Device);
    }

    // 매 프레임 시작 시 포커스 플래그 리셋 (윈도우가 닫혀도 리셋되도록)
    bIsAnyPhysicsAssetEditorFocused = false;

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

        // 다른 위젯에서 체크할 수 있도록 static 변수 업데이트
        bIsAnyPhysicsAssetEditorFocused = bIsWindowFocused;

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

        // Delete 키로 선택된 Constraint 또는 Shape 삭제 (OnRender에서 bIsWindowFocused 설정 후 처리)
        PhysicsAssetEditorState* PhysState = GetPhysicsState();
        if (bIsWindowFocused && ImGui::IsKeyPressed(ImGuiKey_Delete))
        {
            if (PhysState && PhysState->SelectedConstraintIndex >= 0)
            {
                RemoveConstraint(PhysState->SelectedConstraintIndex);
            }
            else
            {
                DeleteSelectedShape();
            }
        }
    }
    ImGui::End();

    if (!bViewerVisible)
    {
        CenterRect = FRect(0, 0, 0, 0);
        CenterRect.UpdateMinMax();
        bIsWindowHovered = false;
        bIsWindowFocused = false;
        bIsAnyPhysicsAssetEditorFocused = false;
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

    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;

    // 기즈모 드래그 상태 체크 - 드래그 중일 때만 UpdateAnchorFromShape 방지
    AGizmoActor* Gizmo = GetGizmoActor();
    if (PhysState->ShapeGizmoAnchor)
    {
        PhysState->ShapeGizmoAnchor->bIsBeingManipulated = (Gizmo && Gizmo->GetbIsDragging());
    }
    if (PhysState->ConstraintGizmoAnchor)
    {
        PhysState->ConstraintGizmoAnchor->bIsBeingManipulated = (Gizmo && Gizmo->GetbIsDragging());
    }

    // 기즈모로 Shape가 수정되었는지 체크
    if (PhysState->ShapeGizmoAnchor && PhysState->ShapeGizmoAnchor->bShapeModified)
    {
        PhysState->bShapesDirty = true;
        PhysState->bIsDirty = true;
        PhysState->ShapeGizmoAnchor->bShapeModified = false;
    }

    // 기즈모로 Constraint가 수정되었는지 체크
    if (PhysState->ConstraintGizmoAnchor && PhysState->ConstraintGizmoAnchor->bConstraintModified)
    {
        PhysState->bIsDirty = true;
        PhysState->ConstraintGizmoAnchor->bConstraintModified = false;
    }
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

    RenderPhysicsBodies();
    RenderConstraintVisuals();

    // Constraint 기즈모 앵커 위치 업데이트 (드래그 중이 아닐 때만)
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (PhysState && PhysState->ConstraintGizmoAnchor && !PhysState->ConstraintGizmoAnchor->bIsBeingManipulated)
    {
        PhysState->ConstraintGizmoAnchor->UpdateAnchorFromConstraint();
    }
}

void SPhysicsAssetEditorWindow::OnSave()
{
    SavePhysicsAsset();
}

void SPhysicsAssetEditorWindow::SavePhysicsAsset()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState)
    {
        UE_LOG("[PhysicsAssetEditor] SavePhysicsAsset: 활성 상태가 없습니다");
        return;
    }

    if (!PhysState->EditingAsset)
    {
        UE_LOG("[PhysicsAssetEditor] SavePhysicsAsset: 편집 중인 에셋이 없습니다");
        return;
    }

    // 파일 경로가 없으면 SaveAs 호출
    if (PhysState->CurrentFilePath.empty())
    {
        SavePhysicsAssetAs();
        return;
    }

    // 저장 수행 (FBX 경로도 함께 저장)
    FString SourceFbxPath = ActiveState ? ActiveState->LoadedMeshPath : "";
    if (PhysicsAssetEditorBootstrap::SavePhysicsAsset(PhysState->EditingAsset, PhysState->CurrentFilePath, SourceFbxPath))
    {
        PhysState->bIsDirty = false;
        UE_LOG("[PhysicsAssetEditor] 저장 완료: %s (FBX: %s)", PhysState->CurrentFilePath.c_str(), SourceFbxPath.c_str());
    }
    else
    {
        UE_LOG("[PhysicsAssetEditor] 저장 실패: %s", PhysState->CurrentFilePath.c_str());
    }
}

void SPhysicsAssetEditorWindow::SavePhysicsAssetAs()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState)
    {
        UE_LOG("[PhysicsAssetEditor] SavePhysicsAssetAs: 활성 상태가 없습니다");
        return;
    }

    if (!PhysState->EditingAsset)
    {
        UE_LOG("[PhysicsAssetEditor] SavePhysicsAssetAs: 편집 중인 에셋이 없습니다");
        return;
    }

    // 저장 파일 다이얼로그 열기
    std::filesystem::path SavePath = FPlatformProcess::OpenSaveFileDialog(
        L"Data/Physics",           // 기본 디렉토리
        L"physics",                // 기본 확장자
        L"Physics Asset Files",    // 파일 타입 설명
        L"NewPhysicsAsset"         // 기본 파일명
    );

    // 사용자가 취소한 경우
    if (SavePath.empty())
    {
        UE_LOG("[PhysicsAssetEditor] SavePhysicsAssetAs: 사용자가 취소했습니다");
        return;
    }

    // std::filesystem::path를 FString으로 변환 (상대 경로로 변환)
    FString SavePathStr = ResolveAssetRelativePath(NormalizePath(WideToUTF8(SavePath.wstring())), "");

    // 저장 수행 (FBX 경로도 함께 저장)
    FString SourceFbxPath = ActiveState ? ActiveState->LoadedMeshPath : "";
    if (PhysicsAssetEditorBootstrap::SavePhysicsAsset(PhysState->EditingAsset, SavePathStr, SourceFbxPath))
    {
        // 에디터 상태 업데이트
        PhysState->CurrentFilePath = SavePathStr;
        PhysState->bIsDirty = false;

        UE_LOG("[PhysicsAssetEditor] 저장 완료: %s (FBX: %s)", SavePathStr.c_str(), SourceFbxPath.c_str());
    }
    else
    {
        UE_LOG("[PhysicsAssetEditor] 저장 실패: %s", SavePathStr.c_str());
    }
}

void SPhysicsAssetEditorWindow::LoadPhysicsAsset()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState)
    {
        UE_LOG("[PhysicsAssetEditor] LoadPhysicsAsset: 활성 상태가 없습니다");
        return;
    }

    // 불러오기 파일 다이얼로그 열기
    std::filesystem::path LoadPath = FPlatformProcess::OpenLoadFileDialog(
        L"Data/Physics",           // 기본 디렉토리
        L"physics",                // 확장자 필터
        L"Physics Asset Files"     // 파일 타입 설명
    );

    // 사용자가 취소한 경우
    if (LoadPath.empty())
    {
        UE_LOG("[PhysicsAssetEditor] LoadPhysicsAsset: 사용자가 취소했습니다");
        return;
    }

    // std::filesystem::path를 FString으로 변환
    FString LoadPathStr = WideToUTF8(LoadPath.wstring());

    // 파일에서 Physics Asset 로드 (FBX 경로도 함께 반환)
    FString SourceFbxPath;
    UPhysicsAsset* LoadedAsset = PhysicsAssetEditorBootstrap::LoadPhysicsAsset(LoadPathStr, &SourceFbxPath);
    if (!LoadedAsset)
    {
        UE_LOG("[PhysicsAssetEditor] 로드 실패: %s", LoadPathStr.c_str());
        return;
    }

    // 기존 에셋 교체
    PhysState->EditingAsset = LoadedAsset;

    // 파일 경로 설정 및 Dirty 플래그 해제
    PhysState->CurrentFilePath = LoadPathStr;
    PhysState->bIsDirty = false;

    // 선택 상태 초기화
    PhysState->SelectedBodyIndex = -1;
    PhysState->SelectedConstraintIndex = -1;
    PhysState->SelectedShapeIndex = -1;
    PhysState->SelectedShapeType = EShapeType::None;

    // 소스 FBX 파일이 있으면 자동으로 로드
    if (!SourceFbxPath.empty() && ActiveState)
    {
        USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(SourceFbxPath);
        if (Mesh && ActiveState->PreviewActor)
        {
            ActiveState->PreviewActor->SetSkeletalMesh(SourceFbxPath);
            ActiveState->CurrentMesh = Mesh;

            // MeshPathBuffer 업데이트 (Asset Browser UI에 표시)
            size_t copyLen = std::min(SourceFbxPath.size(), sizeof(ActiveState->MeshPathBuffer) - 1);
            memcpy(ActiveState->MeshPathBuffer, SourceFbxPath.c_str(), copyLen);
            ActiveState->MeshPathBuffer[copyLen] = '\0';

            // 본 노드 확장
            ActiveState->ExpandedBoneIndices.clear();
            if (const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton())
            {
                for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
                {
                    ActiveState->ExpandedBoneIndices.insert(i);
                }
            }

            // Physics Asset 로드 시에는 OnSkeletalMeshLoaded를 호출하지 않음
            // (이미 로드된 Body/Constraint 데이터를 유지하기 위해)

            ActiveState->LoadedMeshPath = SourceFbxPath;
            if (auto* Skeletal = ActiveState->PreviewActor->GetSkeletalMeshComponent())
            {
                Skeletal->SetVisibility(ActiveState->bShowMesh);
            }
            ActiveState->bBoneLinesDirty = true;
            if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
            {
                LineComp->ClearLines();
                LineComp->SetLineVisible(ActiveState->bShowBones);
            }

            // Shape 라인 재구성 필요
            PhysState->bShapesDirty = true;

            UE_LOG("[PhysicsAssetEditor] 로드 완료: %s (FBX: %s)", LoadPathStr.c_str(), SourceFbxPath.c_str());
        }
        else
        {
            UE_LOG("[PhysicsAssetEditor] 로드 완료: %s (FBX 로드 실패: %s)", LoadPathStr.c_str(), SourceFbxPath.c_str());
        }
    }
    else
    {
        UE_LOG("[PhysicsAssetEditor] 로드 완료: %s (FBX 경로 없음)", LoadPathStr.c_str());
    }
}

void SPhysicsAssetEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    // 부모 클래스 호출 (기즈모 마우스업 처리)
    SViewerWindow::OnMouseUp(MousePos, Button);

    // 마우스 업 시 기즈모가 드래그 중이 아니면 플래그 리셋
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    AGizmoActor* Gizmo = GetGizmoActor();
    if (PhysState && PhysState->ShapeGizmoAnchor && (!Gizmo || !Gizmo->GetbIsDragging()))
    {
        PhysState->ShapeGizmoAnchor->bIsBeingManipulated = false;
    }
}

void SPhysicsAssetEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->Viewport) return;
    if (!PhysState->Viewport->IsViewportHovered()) return;

    if (!CenterRect.Contains(MousePos)) return;

    FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);

    // 기즈모 피킹 먼저 시도
    PhysState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

    // 좌클릭일 때만 Shape/Constraint 피킹 시도
    if (Button != 0) return;

    // 기즈모가 선택되었는지 확인 - 선택됐으면 Shape/Constraint 피킹 스킵
    if (PhysState->World)
    {
        UActorComponent* SelectedComp = PhysState->World->GetSelectionManager()->GetSelectedComponent();
        if (SelectedComp)
        {
            // ShapeAnchorComponent 또는 ConstraintAnchorComponent가 선택됐으면 피킹 스킵
            if (Cast<UShapeAnchorComponent>(SelectedComp) || Cast<UConstraintAnchorComponent>(SelectedComp))
            {
                return;
            }
        }
    }

    if (!PhysState->EditingAsset) return;
    if (!PhysState->CurrentMesh) return;
    if (!PhysState->Client) return;

    const FSkeleton* Skeleton = PhysState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    ASkeletalMeshActor* PreviewActor = static_cast<ASkeletalMeshActor*>(PhysState->PreviewActor);
    if (!PreviewActor) return;
    USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp) return;

    ACameraActor* Camera = PhysState->Client->GetCamera();
    if (!Camera) return;

    // 레이 생성
    FVector CameraPos = Camera->GetActorLocation();
    FVector CameraRight = Camera->GetRight();
    FVector CameraUp = Camera->GetUp();
    FVector CameraForward = Camera->GetForward();

    FVector2D ViewportMousePos(MousePos.X - CenterRect.Left, MousePos.Y - CenterRect.Top);
    FVector2D ViewportSize(CenterRect.GetWidth(), CenterRect.GetHeight());

    FRay Ray = MakeRayFromViewport(
        Camera->GetViewMatrix(),
        Camera->GetProjectionMatrix(CenterRect.GetWidth() / CenterRect.GetHeight(), PhysState->Viewport),
        CameraPos,
        CameraRight,
        CameraUp,
        CameraForward,
        ViewportMousePos,
        ViewportSize
    );

    // 모든 Body의 Shape들과 레이캐스트
    int32 ClosestBodyIndex = -1;
    int32 ClosestShapeIndex = -1;
    EShapeType ClosestShapeType = EShapeType::None;
    float ClosestDistance = FLT_MAX;

    for (int32 BodyIndex = 0; BodyIndex < PhysState->EditingAsset->Bodies.Num(); ++BodyIndex)
    {
        UBodySetup* Body = PhysState->EditingAsset->Bodies[BodyIndex];
        if (!Body) continue;

        // 본 인덱스 찾기
        int32 BoneIndex = -1;
        for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
        {
            if (Skeleton->Bones[i].Name == Body->BoneName.ToString())
            {
                BoneIndex = i;
                break;
            }
        }
        if (BoneIndex < 0) continue;

        FTransform BoneWorldTransform = MeshComp->GetBoneWorldTransform(BoneIndex);

        // Sphere Shape 피킹
        for (int32 i = 0; i < Body->AggGeom.SphereElems.Num(); ++i)
        {
            const FKSphereElem& Sphere = Body->AggGeom.SphereElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Sphere.Center);

            float HitT;
            if (IntersectRaySphere(Ray, ShapeCenter, Sphere.Radius, HitT))
            {
                if (HitT < ClosestDistance)
                {
                    ClosestDistance = HitT;
                    ClosestBodyIndex = BodyIndex;
                    ClosestShapeIndex = i;
                    ClosestShapeType = EShapeType::Sphere;
                }
            }
        }

        // Box Shape 피킹 (OBB 충돌 검사)
        for (int32 i = 0; i < Body->AggGeom.BoxElems.Num(); ++i)
        {
            const FKBoxElem& Box = Body->AggGeom.BoxElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Box.Center);

            FQuat BoxRotation = FQuat::MakeFromEulerZYX(Box.Rotation);
            FQuat FinalRotation = BoneWorldTransform.Rotation * BoxRotation;

            // 단위 박스(-1~1) × Scale = HalfExtent 그대로 사용
            FVector HalfExtent(Box.X, Box.Y, Box.Z);

            float HitT;
            if (IntersectRayOBB(Ray, ShapeCenter, HalfExtent, FinalRotation, HitT))
            {
                if (HitT < ClosestDistance)
                {
                    ClosestDistance = HitT;
                    ClosestBodyIndex = BodyIndex;
                    ClosestShapeIndex = i;
                    ClosestShapeType = EShapeType::Box;
                }
            }
        }

        // Capsule Shape 피킹 (정확한 캡슐 충돌 검사)
        for (int32 i = 0; i < Body->AggGeom.SphylElems.Num(); ++i)
        {
            const FKSphylElem& Capsule = Body->AggGeom.SphylElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Capsule.Center);

            FQuat CapsuleRotation = FQuat::MakeFromEulerZYX(Capsule.Rotation);
            FQuat FinalRotation = BoneWorldTransform.Rotation * CapsuleRotation;

            // 언리얼 방식: HalfHeight = CylinderHalfHeight + Radius
            // Capsule.Length는 실린더 부분의 전체 길이이므로, 반구 포함 HalfHeight로 변환
            float HalfHeight = (Capsule.Length * 0.5f) + Capsule.Radius;

            float HitT;
            if (IntersectRayCapsule(Ray, ShapeCenter, FinalRotation, Capsule.Radius, HalfHeight, HitT))
            {
                if (HitT < ClosestDistance)
                {
                    ClosestDistance = HitT;
                    ClosestBodyIndex = BodyIndex;
                    ClosestShapeIndex = i;
                    ClosestShapeType = EShapeType::Capsule;
                }
            }
        }
    }

    // Constraint 피킹 (Shape보다 우선순위 낮음)
    int32 ClosestConstraintIndex = -1;
    float ClosestConstraintDistance = FLT_MAX;
    const float ConstraintPickRadius = 0.15f;  // 피킹 반경 (월드 스페이스)

    for (int32 i = 0; i < PhysState->EditingAsset->Constraints.Num(); ++i)
    {
        FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[i];

        // Bone2 (Child 본) 인덱스 찾기 - Constraint는 Child 본 위치에 있음
        int32 Bone2Index = -1;
        for (int32 j = 0; j < (int32)Skeleton->Bones.size(); ++j)
        {
            if (Skeleton->Bones[j].Name == Constraint.ConstraintBone2.ToString())
            {
                Bone2Index = j;
                break;
            }
        }
        if (Bone2Index < 0) continue;

        // Constraint 월드 위치 계산 (Bone2/Child 본 기준)
        FTransform Bone2WorldTransform = MeshComp->GetBoneWorldTransform(Bone2Index);
        FVector ConstraintWorldPos = Bone2WorldTransform.Translation +
            Bone2WorldTransform.Rotation.RotateVector(Constraint.Position2);

        // Ray와 점 사이의 최단 거리 계산
        FVector RayToPoint = ConstraintWorldPos - Ray.Origin;
        float ProjectedLength = FVector::Dot(RayToPoint, Ray.Direction);

        // 카메라 뒤에 있으면 스킵
        if (ProjectedLength < 0) continue;

        FVector ClosestPointOnRay = Ray.Origin + Ray.Direction * ProjectedLength;
        float DistanceToRay = (ConstraintWorldPos - ClosestPointOnRay).Size();

        // 피킹 반경 내에 있고 Shape보다 가까우면 선택
        if (DistanceToRay < ConstraintPickRadius && ProjectedLength < ClosestConstraintDistance)
        {
            // Shape가 더 가까우면 Shape 우선
            if (ClosestBodyIndex >= 0 && ClosestDistance < ProjectedLength)
                continue;

            ClosestConstraintDistance = ProjectedLength;
            ClosestConstraintIndex = i;
        }
    }

    // 피킹 결과 적용 (Shape 우선, 그 다음 Constraint)
    if (ClosestBodyIndex >= 0 && (ClosestConstraintIndex < 0 || ClosestDistance <= ClosestConstraintDistance))
    {
        SelectBody(ClosestBodyIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
        PhysState->SelectedShapeIndex = ClosestShapeIndex;
        PhysState->SelectedShapeType = ClosestShapeType;
        UpdateShapeGizmo();
    }
    else if (ClosestConstraintIndex >= 0)
    {
        SelectConstraint(ClosestConstraintIndex);
    }
    else
    {
        // 아무것도 피킹되지 않으면 선택 해제
        ClearSelection();
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

    // Asset Browser 섹션 (부모 클래스에서 가져옴)
    RenderAssetBrowser(PanelWidth);

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

void SPhysicsAssetEditorWindow::RenderTabsAndToolbar(EViewerType CurrentViewerType)
{
    // ============================================
    // 1. 탭바 렌더링 (뷰어 전환 버튼 제외)
    // ============================================
    if (!ImGui::BeginTabBar("PhysicsAssetEditorTabs",
        ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
        return;

    // 탭 렌더링
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        PhysicsAssetEditorState* PState = static_cast<PhysicsAssetEditorState*>(State);
        bool open = true;

        // 동적 탭 이름 생성
        FString TabDisplayName;
        if (PState->CurrentFilePath.empty())
        {
            TabDisplayName = "Untitled";
        }
        else
        {
            // 파일 경로에서 파일명만 추출 (확장자 제외)
            size_t lastSlash = PState->CurrentFilePath.find_last_of("/\\");
            FString filename = (lastSlash != FString::npos)
                ? PState->CurrentFilePath.substr(lastSlash + 1)
                : PState->CurrentFilePath;

            // 확장자 제거
            size_t dotPos = filename.find_last_of('.');
            if (dotPos != FString::npos)
                filename = filename.substr(0, dotPos);

            TabDisplayName = filename;
        }

        // 수정됨 표시 추가
        if (PState->bIsDirty)
        {
            TabDisplayName += "*";
        }

        // ImGui ID 충돌 방지
        TabDisplayName += "##";
        TabDisplayName += State->Name.ToString();

        if (ImGui::BeginTabItem(TabDisplayName.c_str(), &open))
        {
            ActiveTabIndex = i;
            ActiveState = State;
            ImGui::EndTabItem();
        }
        if (!open)
        {
            CloseTab(i);
            ImGui::EndTabBar();
            return;
        }
    }

    // + 버튼 (새 탭 추가)
    if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
    {
        int maxViewerNum = 0;
        for (int i = 0; i < Tabs.Num(); ++i)
        {
            const FString& tabName = Tabs[i]->Name.ToString();
            const char* prefix = "PhysicsTab";
            if (strncmp(tabName.c_str(), prefix, strlen(prefix)) == 0)
            {
                const char* numberPart = tabName.c_str() + strlen(prefix);
                int num = atoi(numberPart);
                if (num > maxViewerNum)
                {
                    maxViewerNum = num;
                }
            }
        }

        char label[64];
        sprintf_s(label, "PhysicsTab%d", maxViewerNum + 1);
        OpenNewTab(label);
    }
    ImGui::EndTabBar();

    // ============================================
    // 2. 툴바 렌더링 (Save, SaveAs, Load 버튼)
    // ============================================
    ImGui::BeginChild("PhysicsToolbar", ImVec2(0, 30.0f), false, ImGuiWindowFlags_NoScrollbar);

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 IconSizeVec(22, 22);
    float BtnHeight = IconSizeVec.y + style.FramePadding.y * 2.0f;
    float availableHeight = ImGui::GetContentRegionAvail().y;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (availableHeight - BtnHeight) * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    // Save 버튼
    if (IconSave && IconSave->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##SavePhysics", (void*)IconSave->GetShaderResourceView(), IconSizeVec))
        {
            SavePhysicsAsset();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Physics Asset 저장 (Ctrl+S)");
    }
    ImGui::SameLine();

    // SaveAs 버튼
    if (IconSaveAs && IconSaveAs->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##SaveAsPhysics", (void*)IconSaveAs->GetShaderResourceView(), IconSizeVec))
        {
            SavePhysicsAssetAs();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("다른 이름으로 저장");
    }
    ImGui::SameLine();

    // Load 버튼
    if (IconLoad && IconLoad->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##LoadPhysics", (void*)IconLoad->GetShaderResourceView(), IconSizeVec))
        {
            LoadPhysicsAsset();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Physics Asset 불러오기");
    }

    ImGui::PopStyleColor();
    ImGui::EndChild();
}

PhysicsAssetEditorState* SPhysicsAssetEditorWindow::GetPhysicsState() const
{
    return static_cast<PhysicsAssetEditorState*>(ActiveState);
}

void SPhysicsAssetEditorWindow::RenderSkeletonTreePanel(float Width, float Height)
{
    ImGui::BeginChild("SkeletonTree", ImVec2(Width - 16, Height), false);

    // 헤더: Skeleton 텍스트 + 톱니바퀴 설정 버튼
    float headerWidth = ImGui::GetContentRegionAvail().x;
    ImGui::Text("Skeleton");
    ImGui::SameLine(headerWidth - 20);

    // 톱니바퀴 버튼
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    if (ImGui::Button("##TreeSettings", ImVec2(20, 20)))
    {
        ImGui::OpenPopup("SkeletonTreeSettingsPopup");
    }
    ImGui::PopStyleColor(2);

    // 톱니바퀴 아이콘 그리기 (간단한 텍스트로 대체)
    ImVec2 btnMin = ImGui::GetItemRectMin();
    ImVec2 btnMax = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center((btnMin.x + btnMax.x) * 0.5f, (btnMin.y + btnMax.y) * 0.5f);
    drawList->AddText(ImVec2(center.x - 4, center.y - 7), IM_COL32(200, 200, 200, 255), "*");

    // 설정 팝업 메뉴
    RenderSkeletonTreeSettings();

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

    // 본 숨김 모드: Body만 트리 구조로 표시
    if (TreeSettings.BoneDisplayMode == EBoneDisplayMode::HideBones)
    {
        PhysicsAssetEditorState* PhysState = GetPhysicsState();
        if (PhysState && PhysState->EditingAsset)
        {
            // 루트 Body 찾기 (부모 본에 Body가 없는 Body들)
            for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
            {
                UBodySetup* Body = PhysState->EditingAsset->Bodies[i];
                if (!Body) continue;

                // 이 Body의 본 인덱스 찾기
                int32 BoneIndex = -1;
                for (int32 j = 0; j < (int32)Skeleton->Bones.size(); ++j)
                {
                    if (Skeleton->Bones[j].Name == Body->BoneName.ToString())
                    {
                        BoneIndex = j;
                        break;
                    }
                }

                if (BoneIndex < 0) continue;

                // 부모 본 체인에서 Body가 있는지 확인
                bool bHasParentBody = false;
                int32 ParentBoneIndex = Skeleton->Bones[BoneIndex].ParentIndex;
                while (ParentBoneIndex >= 0)
                {
                    const FString& ParentBoneName = Skeleton->Bones[ParentBoneIndex].Name;
                    for (int32 k = 0; k < PhysState->EditingAsset->Bodies.Num(); ++k)
                    {
                        if (PhysState->EditingAsset->Bodies[k] &&
                            PhysState->EditingAsset->Bodies[k]->BoneName.ToString() == ParentBoneName)
                        {
                            bHasParentBody = true;
                            break;
                        }
                    }
                    if (bHasParentBody) break;
                    ParentBoneIndex = Skeleton->Bones[ParentBoneIndex].ParentIndex;
                }

                // 부모 Body가 없으면 루트로서 렌더링
                if (!bHasParentBody)
                {
                    RenderBodyTreeNode(i, Skeleton);
                }
            }
        }
    }
    else
    {
        // 일반 모드: 스켈레톤 트리 표시
        for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
        {
            if (Skeleton->Bones[i].ParentIndex == -1)
            {
                RenderBoneTreeNode(i, 0);
            }
        }
    }

    ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::RenderSkeletonTreeSettings()
{
    if (ImGui::BeginPopup("SkeletonTreeSettingsPopup"))
    {
        // 라디오 버튼 스타일: 하나만 선택 가능
        bool bAllBones = (TreeSettings.BoneDisplayMode == EBoneDisplayMode::AllBones);
        bool bMeshBones = (TreeSettings.BoneDisplayMode == EBoneDisplayMode::MeshBones);
        bool bHideBones = (TreeSettings.BoneDisplayMode == EBoneDisplayMode::HideBones);

        if (ImGui::RadioButton("Show All Bones", bAllBones))
        {
            TreeSettings.BoneDisplayMode = EBoneDisplayMode::AllBones;
        }

        if (ImGui::RadioButton("Show Mesh Bones", bMeshBones))
        {
            TreeSettings.BoneDisplayMode = EBoneDisplayMode::MeshBones;
        }

        if (ImGui::RadioButton("Hide Bones", bHideBones))
        {
            TreeSettings.BoneDisplayMode = EBoneDisplayMode::HideBones;
        }

        ImGui::Separator();

        // 컨스트레인트 표시 체크박스
        ImGui::Checkbox("Show Constraints", &TreeSettings.bShowConstraintsInTree);

        ImGui::EndPopup();
    }
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
    UBodySetup* BodySetup = nullptr;
    if (PhysState && PhysState->EditingAsset)
    {
        for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
        {
            if (PhysState->EditingAsset->Bodies[i] &&
                PhysState->EditingAsset->Bodies[i]->BoneName == Bone.Name)
            {
                bHasBody = true;
                BodyIndex = i;
                BodySetup = PhysState->EditingAsset->Bodies[i];
                break;
            }
        }
    }

    // Body가 있거나 자식이 있으면 Leaf가 아님
    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (ChildIndices.Num() == 0 && !bHasBody)
        nodeFlags |= ImGuiTreeNodeFlags_Leaf;

    // Bone.Name is already FString
    const FString& BoneName = Bone.Name;

    bool bOpen = ImGui::TreeNodeEx((void*)(intptr_t)BoneIndex, nodeFlags, "%s", BoneName.c_str());

    if (ImGui::IsItemClicked())
    {
        ActiveState->SelectedBoneIndex = BoneIndex;
        // 본 클릭 시 Body 선택 해제
        if (PhysState)
        {
            PhysState->SelectedBodyIndex = -1;
        }
    }

    // 우클릭 컨텍스트 메뉴
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        ContextMenuBoneIndex = BoneIndex;
        ImGui::OpenPopup("BoneContextMenu");
    }

    // 컨텍스트 메뉴 렌더링 (이 노드에서 열린 경우에만)
    if (ContextMenuBoneIndex == BoneIndex)
    {
        RenderBoneContextMenu(BoneIndex, bHasBody, BodyIndex);
    }

    if (bOpen)
    {
        // Body가 있으면 하위에 BodySetup 노드 표시
        if (bHasBody && BodySetup)
        {
            // 이 Body에 연결된 Constraint 개수 확인
            int32 ConstraintCount = 0;
            if (TreeSettings.bShowConstraintsInTree && PhysState && PhysState->EditingAsset)
            {
                for (int32 i = 0; i < PhysState->EditingAsset->Constraints.Num(); ++i)
                {
                    if (PhysState->EditingAsset->Constraints[i].ConstraintBone2 == BodySetup->BoneName)
                        ConstraintCount++;
                }
            }

            bool bBodySelected = (PhysState && PhysState->SelectedBodyIndex == BodyIndex);
            ImGuiTreeNodeFlags bodyFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            // Constraint가 없으면 Leaf 노드
            if (ConstraintCount == 0)
                bodyFlags |= ImGuiTreeNodeFlags_Leaf;
            if (bBodySelected)
                bodyFlags |= ImGuiTreeNodeFlags_Selected;

            // 아이콘 + 본 이름으로 BodySetup 표시
            FString bodyLabel = FString("[Body] ") + BoneName;

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f)); // 파란색 텍스트
            bool bBodyOpen = ImGui::TreeNodeEx((void*)(intptr_t)(10000 + BodyIndex), bodyFlags, "%s", bodyLabel.c_str());
            ImGui::PopStyleColor();

            if (ImGui::IsItemClicked())
            {
                SelectBody(BodyIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
            }

            // Body 노드가 열려있으면 Constraint를 자식으로 표시
            if (bBodyOpen)
            {
                // 이 Body에 연결된 Constraint 표시 (설정에서 활성화된 경우)
                // 언리얼 스타일: Bone2(자식) 아래에 Constraint 표시
                if (TreeSettings.bShowConstraintsInTree && PhysState && PhysState->EditingAsset)
                {
                    for (int32 i = 0; i < PhysState->EditingAsset->Constraints.Num(); ++i)
                    {
                        FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[i];
                        // Bone2(자식)가 현재 Body인 Constraint만 표시
                        if (Constraint.ConstraintBone2 == BodySetup->BoneName)
                        {
                            RenderConstraintTreeNode(i, BodySetup->BoneName);
                        }
                    }
                }
                ImGui::TreePop();
            }
        }

        // 자식 본 렌더링
        for (int32 ChildIndex : ChildIndices)
            RenderBoneTreeNode(ChildIndex, Depth + 1);

        ImGui::TreePop();
    }
}

void SPhysicsAssetEditorWindow::RenderBodyTreeNode(int32 BodyIndex, const FSkeleton* Skeleton)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset || !Skeleton) return;
    if (BodyIndex < 0 || BodyIndex >= PhysState->EditingAsset->Bodies.Num()) return;

    UBodySetup* Body = PhysState->EditingAsset->Bodies[BodyIndex];
    if (!Body) return;

    // 이 Body의 본 인덱스 찾기
    int32 BoneIndex = -1;
    for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
    {
        if (Skeleton->Bones[i].Name == Body->BoneName.ToString())
        {
            BoneIndex = i;
            break;
        }
    }

    if (BoneIndex < 0) return;

    // 자식 Body 찾기 (이 본의 자손 본들 중 Body가 있는 것)
    TArray<int32> ChildBodyIndices;
    for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
    {
        if (i == BodyIndex) continue;
        UBodySetup* OtherBody = PhysState->EditingAsset->Bodies[i];
        if (!OtherBody) continue;

        // OtherBody의 본 인덱스 찾기
        int32 OtherBoneIndex = -1;
        for (int32 j = 0; j < (int32)Skeleton->Bones.size(); ++j)
        {
            if (Skeleton->Bones[j].Name == OtherBody->BoneName.ToString())
            {
                OtherBoneIndex = j;
                break;
            }
        }

        if (OtherBoneIndex < 0) continue;

        // OtherBody의 부모 체인에서 이 Body가 바로 위 Body인지 확인
        int32 ParentIdx = Skeleton->Bones[OtherBoneIndex].ParentIndex;
        bool bIsDirectChild = false;
        while (ParentIdx >= 0)
        {
            // 이 부모 본에 Body가 있는지 확인
            for (int32 k = 0; k < PhysState->EditingAsset->Bodies.Num(); ++k)
            {
                if (PhysState->EditingAsset->Bodies[k] &&
                    PhysState->EditingAsset->Bodies[k]->BoneName.ToString() == Skeleton->Bones[ParentIdx].Name)
                {
                    // 찾은 Body가 현재 Body면 직접 자식
                    if (k == BodyIndex)
                    {
                        bIsDirectChild = true;
                    }
                    // 다른 Body면 직접 자식이 아님 (중간에 다른 Body가 있음)
                    goto done_search;
                }
            }
            ParentIdx = Skeleton->Bones[ParentIdx].ParentIndex;
        }
    done_search:
        if (bIsDirectChild)
        {
            ChildBodyIndices.Add(i);
        }
    }

    // 이 Body에 연결된 Constraint 개수 확인
    int32 ConstraintCount = 0;
    if (TreeSettings.bShowConstraintsInTree)
    {
        for (int32 i = 0; i < PhysState->EditingAsset->Constraints.Num(); ++i)
        {
            if (PhysState->EditingAsset->Constraints[i].ConstraintBone2 == Body->BoneName)
                ConstraintCount++;
        }
    }

    // 트리 노드 렌더링
    bool bSelected = (PhysState->SelectedBodyIndex == BodyIndex);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    // 자식 Body도 없고 Constraint도 없으면 Leaf
    if (ChildBodyIndices.Num() == 0 && ConstraintCount == 0)
        flags |= ImGuiTreeNodeFlags_Leaf;
    if (bSelected)
        flags |= ImGuiTreeNodeFlags_Selected;

    FString label = Body->BoneName.ToString();

    bool bOpen = ImGui::TreeNodeEx((void*)(intptr_t)(20000 + BodyIndex), flags, "%s", label.c_str());

    if (ImGui::IsItemClicked())
    {
        SelectBody(BodyIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
    }

    if (bOpen)
    {
        // 이 Body에 연결된 Constraint 표시 (설정에서 활성화된 경우)
        // 언리얼 스타일: Bone2(자식) 아래에 Constraint 표시
        if (TreeSettings.bShowConstraintsInTree)
        {
            for (int32 i = 0; i < PhysState->EditingAsset->Constraints.Num(); ++i)
            {
                FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[i];
                // Bone2(자식)가 현재 Body인 Constraint만 표시
                if (Constraint.ConstraintBone2 == Body->BoneName)
                {
                    RenderConstraintTreeNode(i, Body->BoneName);
                }
            }
        }

        // 자식 Body 렌더링
        for (int32 ChildBodyIndex : ChildBodyIndices)
        {
            RenderBodyTreeNode(ChildBodyIndex, Skeleton);
        }
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

        // Simulate Physics 체크박스
        bool bSimulate = Body->bSimulatePhysics;
        if (ImGui::Checkbox("Simulate Physics", &bSimulate))
        {
            Body->bSimulatePhysics = bSimulate;
            if (PhysState) PhysState->bIsDirty = true;
        }

        // Collision Enabled 콤보박스
        const char* collisionModes[] = { "None", "Query Only", "Physics Only", "Physics and Query" };
        int currentCollision = static_cast<int>(Body->CollisionEnabled);
        ImGui::Text("Collision Enabled");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##CollisionEnabled", &currentCollision, collisionModes, 4))
        {
            Body->CollisionEnabled = static_cast<ECollisionEnabled>(currentCollision);
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Separator();

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

        ImGui::Separator();

        // Friction (마찰 계수)
        float friction = Body->Friction;
        ImGui::Text("Friction");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##Friction", &friction, 0.01f, 0.0f, 1.0f, "%.2f"))
        {
            Body->Friction = friction;
            if (PhysState) PhysState->bIsDirty = true;
        }

        // Restitution (반발 계수)
        float restitution = Body->Restitution;
        ImGui::Text("Restitution");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##Restitution", &restitution, 0.01f, 0.0f, 1.0f, "%.2f"))
        {
            Body->Restitution = restitution;
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Body Setup", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        ImGui::Text("Bone Name");
        ImGui::TextDisabled("%s", Body->BoneName.ToString().c_str());
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Primitives", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();

        // Sphere 개수
        int32 sphereCount = Body->AggGeom.SphereElems.Num();
        ImGui::Text("Sphere");
        ImGui::SameLine(150);
        ImGui::TextDisabled("Elements: %d", sphereCount);

        // Box 개수
        int32 boxCount = Body->AggGeom.BoxElems.Num();
        ImGui::Text("Box");
        ImGui::SameLine(150);
        ImGui::TextDisabled("Elements: %d", boxCount);

        // Capsule 개수
        int32 capsuleCount = Body->AggGeom.SphylElems.Num();
        ImGui::Text("Capsule");
        ImGui::SameLine(150);
        ImGui::TextDisabled("Elements: %d", capsuleCount);

        // Convex 개수
        int32 convexCount = Body->AggGeom.ConvexElems.Num();
        ImGui::Text("Convex");
        ImGui::SameLine(150);
        ImGui::TextDisabled("Elements: %d", convexCount);

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

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();

        // Position1 (Bone1 공간에서의 조인트 위치)
        ImGui::Text("Position 1 (Parent Frame)");
        float pos1[3] = { Constraint->Position1.X, Constraint->Position1.Y, Constraint->Position1.Z };
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat3("##Position1", pos1, 0.001f, -100.0f, 100.0f, "%.3f"))
        {
            Constraint->Position1 = FVector(pos1[0], pos1[1], pos1[2]);
            if (PhysState)
            {
                PhysState->bIsDirty = true;
                PhysState->bConstraintsDirty = true;
            }
        }

        // Rotation1 (Bone1 공간에서의 조인트 회전)
        ImGui::Text("Rotation 1 (Euler)");
        float rot1[3] = { Constraint->Rotation1.X, Constraint->Rotation1.Y, Constraint->Rotation1.Z };
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat3("##Rotation1", rot1, 0.5f, -180.0f, 180.0f, "%.1f"))
        {
            Constraint->Rotation1 = FVector(rot1[0], rot1[1], rot1[2]);
            if (PhysState)
            {
                PhysState->bIsDirty = true;
                PhysState->bConstraintsDirty = true;
            }
        }

        ImGui::Separator();

        // Position2 (Bone2 공간에서의 조인트 위치)
        ImGui::Text("Position 2 (Child Frame)");
        float pos2[3] = { Constraint->Position2.X, Constraint->Position2.Y, Constraint->Position2.Z };
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat3("##Position2", pos2, 0.001f, -100.0f, 100.0f, "%.3f"))
        {
            Constraint->Position2 = FVector(pos2[0], pos2[1], pos2[2]);
            if (PhysState)
            {
                PhysState->bIsDirty = true;
                PhysState->bConstraintsDirty = true;
            }
        }

        // Rotation2 (Bone2 공간에서의 조인트 회전)
        ImGui::Text("Rotation 2 (Euler)");
        float rot2[3] = { Constraint->Rotation2.X, Constraint->Rotation2.Y, Constraint->Rotation2.Z };
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat3("##Rotation2", rot2, 0.5f, -180.0f, 180.0f, "%.1f"))
        {
            Constraint->Rotation2 = FVector(rot2[0], rot2[1], rot2[2]);
            if (PhysState)
            {
                PhysState->bIsDirty = true;
                PhysState->bConstraintsDirty = true;
            }
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Collision"))
    {
        ImGui::Indent();

        // 인접 본 간 충돌 비활성화
        bool bDisableCollision = Constraint->bDisableCollision;
        if (ImGui::Checkbox("Disable Collision", &bDisableCollision))
        {
            Constraint->bDisableCollision = bDisableCollision;
            if (PhysState) PhysState->bIsDirty = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Disable collision between connected bodies");
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Angular Motor"))
    {
        ImGui::Indent();

        // 모터 활성화
        bool bMotorEnabled = Constraint->bAngularMotorEnabled;
        if (ImGui::Checkbox("Enable Motor", &bMotorEnabled))
        {
            Constraint->bAngularMotorEnabled = bMotorEnabled;
            if (PhysState) PhysState->bIsDirty = true;
        }

        // 모터가 활성화된 경우에만 설정 표시
        if (Constraint->bAngularMotorEnabled)
        {
            float motorStrength = Constraint->AngularMotorStrength;
            ImGui::Text("Strength");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##MotorStrength", &motorStrength, 1.0f, 0.0f, 10000.0f, "%.1f"))
            {
                Constraint->AngularMotorStrength = motorStrength;
                if (PhysState) PhysState->bIsDirty = true;
            }

            float motorDamping = Constraint->AngularMotorDamping;
            ImGui::Text("Damping");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##MotorDamping", &motorDamping, 0.1f, 0.0f, 1000.0f, "%.2f"))
            {
                Constraint->AngularMotorDamping = motorDamping;
                if (PhysState) PhysState->bIsDirty = true;
            }
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
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->World) return;
    if (!PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    ASkeletalMeshActor* PreviewActor = static_cast<ASkeletalMeshActor*>(ActiveState->PreviewActor);
    if (!PreviewActor) return;
    USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp) return;

    UWorld* World = PhysState->World;

    // 색상 정의
    const FLinearColor UnselectedColor(0.5f, 0.5f, 0.5f, 0.6f);      // 회색: 선택 안됨
    const FLinearColor SiblingColor(0.6f, 0.4f, 0.8f, 0.7f);         // 보라색: 같은 Body의 다른 Shape
    const FLinearColor SelectedShapeColor(0.3f, 0.7f, 1.0f, 0.8f);   // 파란색: 직접 피킹된 Shape

    // 모든 Body를 순회하며 렌더링
    for (int32 BodyIndex = 0; BodyIndex < PhysState->EditingAsset->Bodies.Num(); ++BodyIndex)
    {
        UBodySetup* Body = PhysState->EditingAsset->Bodies[BodyIndex];
        if (!Body) continue;

        // 본 인덱스 찾기
        int32 BoneIndex = -1;
        for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
        {
            if (Skeleton->Bones[i].Name == Body->BoneName.ToString())
            {
                BoneIndex = i;
                break;
            }
        }
        if (BoneIndex < 0) continue;

        FTransform BoneWorldTransform = MeshComp->GetBoneWorldTransform(BoneIndex);

        // 선택된 Body인지 확인
        bool bIsBodySelected = (BodyIndex == PhysState->SelectedBodyIndex);

        // Box Shape
        for (int32 i = 0; i < Body->AggGeom.BoxElems.Num(); ++i)
        {
            const FKBoxElem& Box = Body->AggGeom.BoxElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Box.Center);
            FQuat BoxRotation = FQuat::MakeFromEulerZYX(Box.Rotation);
            FQuat FinalRotation = BoneWorldTransform.Rotation * BoxRotation;
            FVector HalfExtent(Box.X, Box.Y, Box.Z);
            FMatrix Transform = FMatrix::FromTRS(ShapeCenter, FinalRotation, HalfExtent);

            // 색상 결정: 직접 피킹된 Shape > 같은 Body의 Shape > 미선택
            FLinearColor ShapeColor = UnselectedColor;
            if (bIsBodySelected)
            {
                bool bIsThisShapeSelected = (PhysState->SelectedShapeType == EShapeType::Box &&
                                             PhysState->SelectedShapeIndex == i);
                ShapeColor = bIsThisShapeSelected ? SelectedShapeColor : SiblingColor;
            }
            World->AddDebugBox(Transform, ShapeColor, 0);
        }

        // Sphere Shape
        for (int32 i = 0; i < Body->AggGeom.SphereElems.Num(); ++i)
        {
            const FKSphereElem& Sphere = Body->AggGeom.SphereElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Sphere.Center);
            FMatrix Transform = FMatrix::FromTRS(ShapeCenter, FQuat::Identity(), FVector(Sphere.Radius, Sphere.Radius, Sphere.Radius));

            FLinearColor ShapeColor = UnselectedColor;
            if (bIsBodySelected)
            {
                bool bIsThisShapeSelected = (PhysState->SelectedShapeType == EShapeType::Sphere &&
                                             PhysState->SelectedShapeIndex == i);
                ShapeColor = bIsThisShapeSelected ? SelectedShapeColor : SiblingColor;
            }
            World->AddDebugSphere(Transform, ShapeColor, 0);
        }

        // Capsule Shape
        for (int32 i = 0; i < Body->AggGeom.SphylElems.Num(); ++i)
        {
            const FKSphylElem& Capsule = Body->AggGeom.SphylElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Capsule.Center);
            FQuat CapsuleRotation = FQuat::MakeFromEulerZYX(Capsule.Rotation);
            FQuat FinalRotation = BoneWorldTransform.Rotation * CapsuleRotation;
            FMatrix Transform = FMatrix::FromTRS(ShapeCenter, FinalRotation, FVector::One());
            // 언리얼 방식: HalfHeight = CylinderHalfHeight + Radius
            float HalfHeight = (Capsule.Length * 0.5f) + Capsule.Radius;

            FLinearColor ShapeColor = UnselectedColor;
            if (bIsBodySelected)
            {
                bool bIsThisShapeSelected = (PhysState->SelectedShapeType == EShapeType::Capsule &&
                                             PhysState->SelectedShapeIndex == i);
                ShapeColor = bIsThisShapeSelected ? SelectedShapeColor : SiblingColor;
            }
            World->AddDebugCapsule(Transform, Capsule.Radius, HalfHeight, ShapeColor, 0);
        }
    }
}

void SPhysicsAssetEditorWindow::RenderConstraintVisuals()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->World) return;
    if (!PhysState->EditingAsset) return;
    if (!PhysState->bShowConstraints) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    ASkeletalMeshActor* PreviewActor = static_cast<ASkeletalMeshActor*>(ActiveState->PreviewActor);
    if (!PreviewActor) return;
    USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp) return;

    UWorld* World = PhysState->World;

    // 색상 정의 - 언리얼 스타일
    const FLinearColor SwingConeColor(1.0f, 0.3f, 0.3f, 0.4f);      // 빨간색 반투명 (Swing 원뿔)
    const FLinearColor SwingConeSelectedColor(1.0f, 0.3f, 0.3f, 0.6f);  // 선택 시 더 진하게
    const FLinearColor TwistArcColor(0.3f, 1.0f, 0.3f, 0.4f);       // 녹색 반투명 (Twist 부채꼴)
    const FLinearColor TwistArcSelectedColor(0.3f, 1.0f, 0.3f, 0.6f);   // 선택 시 더 진하게
    const FLinearColor MarkerColor(1.0f, 1.0f, 1.0f, 0.8f);         // 흰색 (마커)

    // 크기 설정 (축소됨)
    const float NormalConeHeight = 0.1f;
    const float SelectedConeHeight = 0.15f;  // 선택 시 확대
    const float NormalArcRadius = 0.1f;
    const float SelectedArcRadius = 0.15f;   // 선택 시 확대
    const float MarkerRadius = 0.015f;

    // 모든 Constraint 순회
    for (int32 ConstraintIndex = 0; ConstraintIndex < PhysState->EditingAsset->Constraints.Num(); ++ConstraintIndex)
    {
        const FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[ConstraintIndex];

        bool bIsSelected = (ConstraintIndex == PhysState->SelectedConstraintIndex);

        // Bone1, Bone2 인덱스 찾기
        int32 Bone1Index = -1, Bone2Index = -1;
        for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
        {
            if (Skeleton->Bones[i].Name == Constraint.ConstraintBone1.ToString())
                Bone1Index = i;
            if (Skeleton->Bones[i].Name == Constraint.ConstraintBone2.ToString())
                Bone2Index = i;
        }
        if (Bone1Index < 0 || Bone2Index < 0) continue;

        // 본 월드 트랜스폼
        FTransform Bone1World = MeshComp->GetBoneWorldTransform(Bone1Index);
        FTransform Bone2World = MeshComp->GetBoneWorldTransform(Bone2Index);

        // Joint 위치 계산 - Child 본(Bone2) 위치 기준 (언리얼 방식)
        // Constraint는 Child 본의 원점(관절 위치)에 생성됨
        FQuat JointRot2 = FQuat::MakeFromEulerZYX(Constraint.Rotation2);
        FVector JointPos = Bone2World.Translation +
            Bone2World.Rotation.RotateVector(Constraint.Position2);
        FQuat JointWorldRot = Bone2World.Rotation * JointRot2;

        // 선택 상태에 따른 크기
        float ConeHeight = bIsSelected ? SelectedConeHeight : NormalConeHeight;
        float ArcRadius = bIsSelected ? SelectedArcRadius : NormalArcRadius;

        // 1. Joint 마커 (작은 구)
        FMatrix MarkerTransform = FMatrix::FromTRS(JointPos, FQuat::Identity(), FVector(MarkerRadius, MarkerRadius, MarkerRadius));
        World->AddDebugSphere(MarkerTransform, MarkerColor);

        // 2. Swing 원뿔 (양방향 다이아몬드 형태, 빨간색)
        if (Constraint.Swing1Motion == EAngularConstraintMotion::Limited ||
            Constraint.Swing2Motion == EAngularConstraintMotion::Limited)
        {
            float Swing1Rad = Constraint.Swing1Motion == EAngularConstraintMotion::Limited
                ? Constraint.Swing1LimitAngle * PI_CONST / 180.0f : PI_CONST * 0.5f;
            float Swing2Rad = Constraint.Swing2Motion == EAngularConstraintMotion::Limited
                ? Constraint.Swing2LimitAngle * PI_CONST / 180.0f : PI_CONST * 0.5f;

            // Swing 각도가 너무 작으면 최소값 설정
            Swing1Rad = FMath::Max(Swing1Rad, 0.05f);
            Swing2Rad = FMath::Max(Swing2Rad, 0.05f);

            FLinearColor ConeColor = bIsSelected ? SwingConeSelectedColor : SwingConeColor;

            // 양방향 원뿔 (메시 자체가 양방향)
            FMatrix ConeTransform = FMatrix::FromTRS(JointPos, JointWorldRot, FVector::One());
            World->AddDebugCone(ConeTransform, Swing1Rad, Swing2Rad, ConeHeight, ConeColor);
        }

        // 3. Twist 부채꼴 (YZ 평면, 녹색)
        // 반지름은 고정, TwistAngle에 따라 부채꼴 각도만 변함
        if (Constraint.TwistMotion == EAngularConstraintMotion::Limited)
        {
            float TwistRad = Constraint.TwistLimitAngle * PI_CONST / 180.0f;
            TwistRad = FMath::Max(TwistRad, 0.05f);

            FMatrix ArcTransform = FMatrix::FromTRS(JointPos, JointWorldRot, FVector::One());
            FLinearColor ArcColor = bIsSelected ? TwistArcSelectedColor : TwistArcColor;
            World->AddDebugArc(ArcTransform, TwistRad, ArcRadius, ArcColor);
        }
        else if (Constraint.TwistMotion == EAngularConstraintMotion::Free)
        {
            // Free면 전체 원 (PI = 180도, 최대 크기)
            FMatrix ArcTransform = FMatrix::FromTRS(JointPos, JointWorldRot, FVector::One());
            FLinearColor ArcColor = bIsSelected ? TwistArcSelectedColor : TwistArcColor;
            World->AddDebugArc(ArcTransform, PI_CONST, ArcRadius, ArcColor);
        }
    }
}

void SPhysicsAssetEditorWindow::SelectBody(int32 Index, PhysicsAssetEditorState::ESelectionSource Source)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;
    PhysState->SelectedBodyIndex = Index;
    PhysState->SelectedConstraintIndex = -1;  // Constraint 선택 해제
    PhysState->SelectionSource = Source;
    PhysState->bShapesDirty = true;  // Shape 라인 재구성 필요

    // Body의 첫 번째 Shape 자동 선택
    PhysState->SelectedShapeIndex = -1;
    PhysState->SelectedShapeType = EShapeType::None;
    if (PhysState->EditingAsset && Index >= 0 && Index < PhysState->EditingAsset->Bodies.Num())
    {
        UBodySetup* Body = PhysState->EditingAsset->Bodies[Index];
        if (Body)
        {
            // 우선순위: Sphere > Box > Capsule
            if (Body->AggGeom.SphereElems.Num() > 0)
            {
                PhysState->SelectedShapeIndex = 0;
                PhysState->SelectedShapeType = EShapeType::Sphere;
            }
            else if (Body->AggGeom.BoxElems.Num() > 0)
            {
                PhysState->SelectedShapeIndex = 0;
                PhysState->SelectedShapeType = EShapeType::Box;
            }
            else if (Body->AggGeom.SphylElems.Num() > 0)
            {
                PhysState->SelectedShapeIndex = 0;
                PhysState->SelectedShapeType = EShapeType::Capsule;
            }
        }
    }

    // 기즈모 업데이트
    UpdateShapeGizmo();
    UpdateConstraintGizmo();
}

void SPhysicsAssetEditorWindow::SelectConstraint(int32 Index)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;
    PhysState->SelectedConstraintIndex = Index;
    PhysState->SelectedBodyIndex = -1;
    PhysState->SelectedShapeIndex = -1;
    PhysState->SelectedShapeType = EShapeType::None;

    // Shape 기즈모 숨김
    UpdateShapeGizmo();
    // Constraint 기즈모 업데이트
    UpdateConstraintGizmo();
}

void SPhysicsAssetEditorWindow::ClearSelection()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;
    PhysState->SelectedBodyIndex = -1;
    PhysState->SelectedConstraintIndex = -1;
    PhysState->SelectedShapeIndex = -1;
    PhysState->SelectedShapeType = EShapeType::None;
    PhysState->bShapesDirty = true;  // Shape 라인 클리어 필요
    UpdateShapeGizmo();  // Shape 기즈모 숨김
    UpdateConstraintGizmo();  // Constraint 기즈모 숨김
}

void SPhysicsAssetEditorWindow::DeleteSelectedShape()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (PhysState->SelectedBodyIndex < 0 || PhysState->SelectedShapeIndex < 0) return;
    if (PhysState->SelectedShapeType == EShapeType::None) return;

    UBodySetup* Body = PhysState->EditingAsset->Bodies[PhysState->SelectedBodyIndex];
    if (!Body) return;

    bool bDeleted = false;

    switch (PhysState->SelectedShapeType)
    {
    case EShapeType::Sphere:
        if (PhysState->SelectedShapeIndex < Body->AggGeom.SphereElems.Num())
        {
            Body->AggGeom.SphereElems.RemoveAt(PhysState->SelectedShapeIndex);
            bDeleted = true;
        }
        break;
    case EShapeType::Box:
        if (PhysState->SelectedShapeIndex < Body->AggGeom.BoxElems.Num())
        {
            Body->AggGeom.BoxElems.RemoveAt(PhysState->SelectedShapeIndex);
            bDeleted = true;
        }
        break;
    case EShapeType::Capsule:
        if (PhysState->SelectedShapeIndex < Body->AggGeom.SphylElems.Num())
        {
            Body->AggGeom.SphylElems.RemoveAt(PhysState->SelectedShapeIndex);
            bDeleted = true;
        }
        break;
    default:
        break;
    }

    if (bDeleted)
    {
        PhysState->bIsDirty = true;

        // Body에 Shape가 하나도 없으면 Body도 삭제
        if (Body->AggGeom.SphereElems.Num() == 0 &&
            Body->AggGeom.BoxElems.Num() == 0 &&
            Body->AggGeom.SphylElems.Num() == 0)
        {
            RemoveBody(PhysState->SelectedBodyIndex);
        }

        // 모든 선택 해제 (회색으로 돌아감)
        ClearSelection();
    }
}

int32 SPhysicsAssetEditorWindow::GetShapeCountByType(UBodySetup* Body, EShapeType ShapeType)
{
    if (!Body) return 0;

    switch (ShapeType)
    {
    case EShapeType::Sphere:  return Body->AggGeom.SphereElems.Num();
    case EShapeType::Box:     return Body->AggGeom.BoxElems.Num();
    case EShapeType::Capsule: return Body->AggGeom.SphylElems.Num();
    default:                  return 0;
    }
}

void SPhysicsAssetEditorWindow::UpdateShapeGizmo()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->World || !PhysState->ShapeGizmoAnchor) return;
    if (!PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    // Shape가 선택되지 않았으면 기즈모 숨김
    if (PhysState->SelectedBodyIndex < 0 ||
        PhysState->SelectedShapeIndex < 0 ||
        PhysState->SelectedShapeType == EShapeType::None)
    {
        PhysState->ShapeGizmoAnchor->ClearTarget();
        PhysState->ShapeGizmoAnchor->SetVisibility(false);
        PhysState->ShapeGizmoAnchor->SetEditability(false);
        // Constraint 기즈모가 활성화되지 않은 경우에만 SelectionManager 클리어
        if (PhysState->SelectedConstraintIndex < 0)
        {
            PhysState->World->GetSelectionManager()->ClearSelection();
        }
        return;
    }

    UBodySetup* Body = PhysState->EditingAsset->Bodies[PhysState->SelectedBodyIndex];
    if (!Body) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    ASkeletalMeshActor* PreviewActor = static_cast<ASkeletalMeshActor*>(ActiveState->PreviewActor);
    if (!PreviewActor) return;
    USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp) return;

    // 본 인덱스 찾기
    int32 BoneIndex = -1;
    for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
    {
        if (Skeleton->Bones[i].Name == Body->BoneName.ToString())
        {
            BoneIndex = i;
            break;
        }
    }
    if (BoneIndex < 0) return;

    // ShapeAnchorComponent에 타겟 설정 및 위치 업데이트
    PhysState->ShapeGizmoAnchor->SetTarget(Body, PhysState->SelectedShapeType,
                                            PhysState->SelectedShapeIndex, MeshComp, BoneIndex);
    PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
    PhysState->ShapeGizmoAnchor->SetVisibility(true);
    PhysState->ShapeGizmoAnchor->SetEditability(true);

    // SelectionManager에 등록하여 기즈모 표시
    PhysState->World->GetSelectionManager()->SelectActor(PreviewActor);
    PhysState->World->GetSelectionManager()->SelectComponent(PhysState->ShapeGizmoAnchor);
}

void SPhysicsAssetEditorWindow::UpdateConstraintGizmo()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->World || !PhysState->ConstraintGizmoAnchor) return;
    if (!PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    // Constraint가 선택되지 않았으면 기즈모 숨김
    if (PhysState->SelectedConstraintIndex < 0 ||
        PhysState->SelectedConstraintIndex >= PhysState->EditingAsset->Constraints.Num())
    {
        PhysState->ConstraintGizmoAnchor->ClearTarget();
        PhysState->ConstraintGizmoAnchor->SetVisibility(false);
        PhysState->ConstraintGizmoAnchor->SetEditability(false);
        return;
    }

    FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[PhysState->SelectedConstraintIndex];

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    ASkeletalMeshActor* PreviewActor = static_cast<ASkeletalMeshActor*>(ActiveState->PreviewActor);
    if (!PreviewActor) return;
    USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp) return;

    // Bone2 (Child 본) 인덱스 찾기 - Constraint는 Child 본 위치에 생성됨
    int32 Bone2Index = -1;
    for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
    {
        if (Skeleton->Bones[i].Name == Constraint.ConstraintBone2.ToString())
        {
            Bone2Index = i;
            break;
        }
    }
    if (Bone2Index < 0) return;

    // ConstraintAnchorComponent에 타겟 설정 및 위치 업데이트 (Child 본 기준)
    PhysState->ConstraintGizmoAnchor->SetTarget(&Constraint, MeshComp, Bone2Index);
    PhysState->ConstraintGizmoAnchor->UpdateAnchorFromConstraint();
    PhysState->ConstraintGizmoAnchor->SetVisibility(true);
    PhysState->ConstraintGizmoAnchor->SetEditability(true);

    // SelectionManager에 등록하여 기즈모 표시
    PhysState->World->GetSelectionManager()->SelectActor(PreviewActor);
    PhysState->World->GetSelectionManager()->SelectComponent(PhysState->ConstraintGizmoAnchor);
}

void SPhysicsAssetEditorWindow::RenderBoneContextMenu(int32 BoneIndex, bool bHasBody, int32 BodyIndex)
{
    if (ImGui::BeginPopup("BoneContextMenu"))
    {
        if (!ActiveState || !ActiveState->CurrentMesh)
        {
            ImGui::EndPopup();
            return;
        }

        const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
        if (!Skeleton || BoneIndex < 0 || BoneIndex >= (int32)Skeleton->Bones.size())
        {
            ImGui::EndPopup();
            return;
        }

        const FString& BoneName = Skeleton->Bones[BoneIndex].Name;
        ImGui::TextDisabled("%s", BoneName.c_str());
        ImGui::Separator();

        // Shape 추가 서브메뉴 (Body 유무와 관계없이 항상 표시)
        if (ImGui::BeginMenu("Add Shape"))
        {
            if (ImGui::MenuItem("Box"))
            {
                AddShapeToBone(BoneIndex, EShapeType::Box);
                ContextMenuBoneIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Sphere"))
            {
                AddShapeToBone(BoneIndex, EShapeType::Sphere);
                ContextMenuBoneIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Capsule"))
            {
                AddShapeToBone(BoneIndex, EShapeType::Capsule);
                ContextMenuBoneIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndMenu();
        }

        // Body가 있으면 Constraint 및 삭제 옵션
        if (bHasBody)
        {
            ImGui::Separator();

            PhysicsAssetEditorState* PhysState = GetPhysicsState();

            // Constraint 생성 옵션
            if (PhysState && PhysState->ConstraintStartBodyIndex < 0)
            {
                // Constraint 시작점 설정
                if (ImGui::MenuItem("Start Constraint"))
                {
                    PhysState->ConstraintStartBodyIndex = BodyIndex;
                    ContextMenuBoneIndex = -1;
                    UE_LOG("[PhysicsAssetEditor] Constraint start: %s", BoneName.c_str());
                }
            }
            else if (PhysState && PhysState->ConstraintStartBodyIndex != BodyIndex)
            {
                // 시작점이 설정된 상태에서 다른 Body 우클릭
                UBodySetup* StartBody = PhysState->EditingAsset->Bodies[PhysState->ConstraintStartBodyIndex];
                FString startBoneName = StartBody ? StartBody->BoneName.ToString() : "Unknown";

                FString menuText = FString("Complete Constraint from ") + startBoneName;
                if (ImGui::MenuItem(menuText.c_str()))
                {
                    AddConstraintBetweenBodies(PhysState->ConstraintStartBodyIndex, BodyIndex);
                    PhysState->ConstraintStartBodyIndex = -1;
                    ContextMenuBoneIndex = -1;
                }

                if (ImGui::MenuItem("Cancel Constraint"))
                {
                    PhysState->ConstraintStartBodyIndex = -1;
                    ContextMenuBoneIndex = -1;
                }
            }

            // 부모 Body로 자동 Constraint 추가
            if (ImGui::MenuItem("Add Constraint to Parent"))
            {
                AddConstraintToParentBody(BodyIndex);
                ContextMenuBoneIndex = -1;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Remove Body"))
            {
                RemoveBody(BodyIndex);
                ContextMenuBoneIndex = -1;
            }
        }

        ImGui::EndPopup();
    }
}

void SPhysicsAssetEditorWindow::AddBodyToSelectedBone()
{
    if (!ActiveState || ActiveState->SelectedBoneIndex < 0)
        return;
    AddBodyToBone(ActiveState->SelectedBoneIndex);
}

void SPhysicsAssetEditorWindow::AddBodyToBone(int32 BoneIndex)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= (int32)Skeleton->Bones.size())
        return;

    const FString& BoneName = Skeleton->Bones[BoneIndex].Name;

    // 이미 Body가 있는지 확인
    for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
    {
        if (PhysState->EditingAsset->Bodies[i] &&
            PhysState->EditingAsset->Bodies[i]->BoneName == BoneName)
        {
            UE_LOG("[PhysicsAssetEditor] Body already exists for bone: %s", BoneName.c_str());
            return;
        }
    }

    // Body 생성
    UBodySetup* NewBody = CreateDefaultBodySetup(BoneName);
    if (NewBody)
    {
        PhysState->EditingAsset->Bodies.Add(NewBody);
        int32 NewIndex = PhysState->EditingAsset->Bodies.Num() - 1;
        SelectBody(NewIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
        PhysState->bIsDirty = true;
        UE_LOG("[PhysicsAssetEditor] Added body for bone: %s", BoneName.c_str());
    }
}

void SPhysicsAssetEditorWindow::RemoveBody(int32 BodyIndex)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (BodyIndex < 0 || BodyIndex >= PhysState->EditingAsset->Bodies.Num()) return;

    UBodySetup* Body = PhysState->EditingAsset->Bodies[BodyIndex];
    if (!Body) return;

    FName BoneName = Body->BoneName;
    UE_LOG("[PhysicsAssetEditor] Removed body for bone: %s", BoneName.ToString().c_str());

    // 이 Body와 연결된 모든 Constraint 삭제 (역순으로 삭제해야 인덱스 문제 없음)
    for (int32 i = PhysState->EditingAsset->Constraints.Num() - 1; i >= 0; --i)
    {
        const FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[i];
        if (Constraint.ConstraintBone1 == BoneName || Constraint.ConstraintBone2 == BoneName)
        {
            UE_LOG("[PhysicsAssetEditor] Removed constraint: %s - %s",
                Constraint.ConstraintBone1.ToString().c_str(),
                Constraint.ConstraintBone2.ToString().c_str());
            PhysState->EditingAsset->Constraints.RemoveAt(i);
        }
    }

    PhysState->EditingAsset->Bodies.RemoveAt(BodyIndex);
    ClearSelection();
    PhysState->bIsDirty = true;
    PhysState->bShapesDirty = true;
}

UBodySetup* SPhysicsAssetEditorWindow::CreateDefaultBodySetup(const FString& BoneName)
{
    return CreateBodySetupWithShape(BoneName, EShapeType::Capsule);
}

void SPhysicsAssetEditorWindow::AddShapeToBone(int32 BoneIndex, EShapeType ShapeType)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= (int32)Skeleton->Bones.size())
        return;

    const FString& BoneName = Skeleton->Bones[BoneIndex].Name;

    // 이미 Body가 있는지 확인
    UBodySetup* ExistingBody = nullptr;
    int32 ExistingBodyIndex = -1;
    for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
    {
        if (PhysState->EditingAsset->Bodies[i] &&
            PhysState->EditingAsset->Bodies[i]->BoneName == BoneName)
        {
            ExistingBody = PhysState->EditingAsset->Bodies[i];
            ExistingBodyIndex = i;
            break;
        }
    }

    if (ExistingBody)
    {
        // 기존 Body에 Shape 추가 - 추가 전 인덱스 기록
        int32 NewShapeIndex = GetShapeCountByType(ExistingBody, ShapeType);
        AddShapeToBody(ExistingBody, ShapeType);
        SelectBody(ExistingBodyIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
        PhysState->SelectedShapeType = ShapeType;
        PhysState->SelectedShapeIndex = NewShapeIndex;
    }
    else
    {
        // 새 Body 생성 후 Shape 추가
        UBodySetup* NewBody = NewObject<UBodySetup>();
        NewBody->BoneName = FName(BoneName);
        NewBody->MassInKg = 1.0f;
        NewBody->LinearDamping = 0.01f;
        NewBody->AngularDamping = 0.0f;

        AddShapeToBody(NewBody, ShapeType);

        PhysState->EditingAsset->Bodies.Add(NewBody);
        int32 NewIndex = PhysState->EditingAsset->Bodies.Num() - 1;

        // 자동 Constraint 생성: 부모 방향으로 Body를 탐색하여 연결
        AddConstraintToParentBody(NewIndex);

        SelectBody(NewIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
        PhysState->SelectedShapeType = ShapeType;
        PhysState->SelectedShapeIndex = 0;  // 새 Body의 첫 번째 Shape
    }

    PhysState->bIsDirty = true;
    UpdateShapeGizmo();  // 기즈모 표시
    const char* ShapeNames[] = { "None", "Sphere", "Box", "Capsule" };
    UE_LOG("[PhysicsAssetEditor] Added %s shape to bone: %s", ShapeNames[(int)ShapeType], BoneName.c_str());
}

void SPhysicsAssetEditorWindow::AddShapeToBody(UBodySetup* Body, EShapeType ShapeType)
{
    if (!Body) return;

    switch (ShapeType)
    {
    case EShapeType::Box:
    {
        FKBoxElem Box;
        Box.Center = FVector(0.0f, 0.0f, 0.0f);
        Box.Rotation = FVector(0.0f, 0.0f, 0.0f);
        Box.X = 0.15f;  // Half extent
        Box.Y = 0.15f;
        Box.Z = 0.15f;
        Body->AggGeom.BoxElems.Add(Box);
        break;
    }
    case EShapeType::Sphere:
    {
        FKSphereElem Sphere;
        Sphere.Center = FVector(0.0f, 0.0f, 0.0f);
        Sphere.Radius = 0.15f;
        Body->AggGeom.SphereElems.Add(Sphere);
        break;
    }
    case EShapeType::Capsule:
    default:
    {
        FKSphylElem Capsule;
        Capsule.Center = FVector(0.0f, 0.0f, 0.0f);
        Capsule.Rotation = FVector(0.0f, 0.0f, 0.0f);
        Capsule.Radius = 0.1f;
        Capsule.Length = 0.3f;  // 전체 실린더 길이
        Body->AggGeom.SphylElems.Add(Capsule);
        break;
    }
    }
}

void SPhysicsAssetEditorWindow::AddBodyToBoneWithShape(int32 BoneIndex, EShapeType ShapeType)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= (int32)Skeleton->Bones.size())
        return;

    const FString& BoneName = Skeleton->Bones[BoneIndex].Name;

    // 이미 Body가 있는지 확인
    for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
    {
        if (PhysState->EditingAsset->Bodies[i] &&
            PhysState->EditingAsset->Bodies[i]->BoneName == BoneName)
        {
            UE_LOG("[PhysicsAssetEditor] Body already exists for bone: %s", BoneName.c_str());
            return;
        }
    }

    // Body 생성
    UBodySetup* NewBody = CreateBodySetupWithShape(BoneName, ShapeType);
    if (NewBody)
    {
        PhysState->EditingAsset->Bodies.Add(NewBody);
        int32 NewIndex = PhysState->EditingAsset->Bodies.Num() - 1;
        SelectBody(NewIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
        PhysState->bIsDirty = true;

        const char* ShapeNames[] = { "Box", "Sphere", "Capsule" };
        UE_LOG("[PhysicsAssetEditor] Added %s body for bone: %s", ShapeNames[(int)ShapeType], BoneName.c_str());
    }
}

UBodySetup* SPhysicsAssetEditorWindow::CreateBodySetupWithShape(const FString& BoneName, EShapeType ShapeType)
{
    UBodySetup* Body = NewObject<UBodySetup>();
    Body->BoneName = FName(BoneName);

    // Shape 추가 (AddShapeToBody 호출로 통일)
    AddShapeToBody(Body, ShapeType);

    // 기본 물리 속성
    Body->MassInKg = 1.0f;
    Body->LinearDamping = 0.01f;
    Body->AngularDamping = 0.05f;

    return Body;
}

// ========== Constraint 생성/삭제 함수들 ==========

FConstraintInstance SPhysicsAssetEditorWindow::CreateDefaultConstraint(const FName& Bone1, const FName& Bone2)
{
    FConstraintInstance Constraint;
    Constraint.ConstraintBone1 = Bone1;  // Parent
    Constraint.ConstraintBone2 = Bone2;  // Child

    // Linear Limits: 기본적으로 모두 잠금 (뼈가 빠지면 안됨)
    Constraint.LinearXMotion = ELinearConstraintMotion::Locked;
    Constraint.LinearYMotion = ELinearConstraintMotion::Locked;
    Constraint.LinearZMotion = ELinearConstraintMotion::Locked;
    Constraint.LinearLimit = 0.0f;

    // Angular Limits: 기본적으로 Limited
    Constraint.TwistMotion = EAngularConstraintMotion::Limited;
    Constraint.Swing1Motion = EAngularConstraintMotion::Limited;
    Constraint.Swing2Motion = EAngularConstraintMotion::Limited;

    Constraint.TwistLimitAngle = 45.0f;
    Constraint.Swing1LimitAngle = 45.0f;
    Constraint.Swing2LimitAngle = 45.0f;

    // 충돌 비활성화 (인접 본 간 충돌 무시)
    Constraint.bDisableCollision = true;

    return Constraint;
}

void SPhysicsAssetEditorWindow::AddConstraintBetweenBodies(int32 BodyIndex1, int32 BodyIndex2)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;

    // 유효성 검사
    if (BodyIndex1 < 0 || BodyIndex1 >= PhysState->EditingAsset->Bodies.Num()) return;
    if (BodyIndex2 < 0 || BodyIndex2 >= PhysState->EditingAsset->Bodies.Num()) return;
    if (BodyIndex1 == BodyIndex2) return;

    UBodySetup* Body1 = PhysState->EditingAsset->Bodies[BodyIndex1];
    UBodySetup* Body2 = PhysState->EditingAsset->Bodies[BodyIndex2];
    if (!Body1 || !Body2) return;

    // 중복 Constraint 체크
    for (const auto& Constraint : PhysState->EditingAsset->Constraints)
    {
        if ((Constraint.ConstraintBone1 == Body1->BoneName && Constraint.ConstraintBone2 == Body2->BoneName) ||
            (Constraint.ConstraintBone1 == Body2->BoneName && Constraint.ConstraintBone2 == Body1->BoneName))
        {
            UE_LOG("[PhysicsAssetEditor] Constraint already exists between %s and %s",
                   Body1->BoneName.ToString().c_str(), Body2->BoneName.ToString().c_str());
            return;
        }
    }

    // Constraint 생성
    FConstraintInstance NewConstraint = CreateDefaultConstraint(Body1->BoneName, Body2->BoneName);
    PhysState->EditingAsset->Constraints.Add(NewConstraint);

    int32 NewIndex = PhysState->EditingAsset->Constraints.Num() - 1;
    SelectConstraint(NewIndex);
    PhysState->bIsDirty = true;
    PhysState->bConstraintsDirty = true;

    UE_LOG("[PhysicsAssetEditor] Added constraint between %s and %s",
           Body1->BoneName.ToString().c_str(), Body2->BoneName.ToString().c_str());
}

void SPhysicsAssetEditorWindow::AddConstraintToParentBody(int32 ChildBodyIndex)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    if (ChildBodyIndex < 0 || ChildBodyIndex >= PhysState->EditingAsset->Bodies.Num()) return;

    UBodySetup* ChildBody = PhysState->EditingAsset->Bodies[ChildBodyIndex];
    if (!ChildBody) return;

    // Child Body의 본 인덱스 찾기
    int32 ChildBoneIndex = -1;
    for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
    {
        if (Skeleton->Bones[i].Name == ChildBody->BoneName.ToString())
        {
            ChildBoneIndex = i;
            break;
        }
    }
    if (ChildBoneIndex < 0) return;

    // 부모 본 체인을 따라가며 Body가 있는 본 찾기
    int32 ParentBoneIndex = Skeleton->Bones[ChildBoneIndex].ParentIndex;
    while (ParentBoneIndex >= 0)
    {
        const FString& ParentBoneName = Skeleton->Bones[ParentBoneIndex].Name;

        for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
        {
            UBodySetup* ParentBody = PhysState->EditingAsset->Bodies[i];
            if (ParentBody && ParentBody->BoneName.ToString() == ParentBoneName)
            {
                // 부모 Body 발견 - Constraint 생성
                AddConstraintBetweenBodies(i, ChildBodyIndex);
                return;
            }
        }

        ParentBoneIndex = Skeleton->Bones[ParentBoneIndex].ParentIndex;
    }

    UE_LOG("[PhysicsAssetEditor] No parent body found for %s", ChildBody->BoneName.ToString().c_str());
}

void SPhysicsAssetEditorWindow::RemoveConstraint(int32 ConstraintIndex)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (ConstraintIndex < 0 || ConstraintIndex >= PhysState->EditingAsset->Constraints.Num()) return;

    FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[ConstraintIndex];
    UE_LOG("[PhysicsAssetEditor] Removed constraint between %s and %s",
           Constraint.ConstraintBone1.ToString().c_str(),
           Constraint.ConstraintBone2.ToString().c_str());

    PhysState->EditingAsset->Constraints.RemoveAt(ConstraintIndex);
    ClearSelection();
    PhysState->bIsDirty = true;
    PhysState->bConstraintsDirty = true;
}

void SPhysicsAssetEditorWindow::RenderConstraintTreeNode(int32 ConstraintIndex, const FName& CurrentBoneName)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (ConstraintIndex < 0 || ConstraintIndex >= PhysState->EditingAsset->Constraints.Num()) return;

    FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[ConstraintIndex];

    // 언리얼 스타일: [ Bone1 -> Bone2 ] 컨스트레인트
    bool bSelected = (PhysState->SelectedConstraintIndex == ConstraintIndex);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (bSelected) flags |= ImGuiTreeNodeFlags_Selected;

    FString label = FString("[ ") + Constraint.ConstraintBone1.ToString() +
                    " -> " + Constraint.ConstraintBone2.ToString() + " ] Constraint";

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f)); // 주황색
    if (ImGui::TreeNodeEx((void*)(intptr_t)(30000 + ConstraintIndex), flags, "%s", label.c_str()))
    {
        ImGui::TreePop();
    }
    ImGui::PopStyleColor();

    if (ImGui::IsItemClicked())
    {
        SelectConstraint(ConstraintIndex);
    }

    // 우클릭 컨텍스트 메뉴
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        ContextMenuConstraintIndex = ConstraintIndex;
        ImGui::OpenPopup("ConstraintContextMenu");
    }

    RenderConstraintContextMenu(ConstraintIndex);
}

void SPhysicsAssetEditorWindow::RenderConstraintContextMenu(int32 ConstraintIndex)
{
    if (ContextMenuConstraintIndex != ConstraintIndex) return;

    if (ImGui::BeginPopup("ConstraintContextMenu"))
    {
        PhysicsAssetEditorState* PhysState = GetPhysicsState();
        if (PhysState && PhysState->EditingAsset &&
            ConstraintIndex >= 0 && ConstraintIndex < PhysState->EditingAsset->Constraints.Num())
        {
            FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[ConstraintIndex];

            ImGui::TextDisabled("%s <-> %s",
                Constraint.ConstraintBone1.ToString().c_str(),
                Constraint.ConstraintBone2.ToString().c_str());
            ImGui::Separator();

            if (ImGui::MenuItem("Delete Constraint"))
            {
                RemoveConstraint(ConstraintIndex);
                ContextMenuConstraintIndex = -1;
            }
        }
        ImGui::EndPopup();
    }
}
