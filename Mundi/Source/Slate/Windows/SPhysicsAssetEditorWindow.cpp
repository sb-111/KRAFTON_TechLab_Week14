#include "pch.h"
#include "SlateManager.h"
#include "SPhysicsAssetEditorWindow.h"
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
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Renderer/FViewportClient.h"

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

    RenderPhysicsBodies();
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

void SPhysicsAssetEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->Viewport) return;
    if (!PhysState->Viewport->IsViewportHovered()) return;

    if (!CenterRect.Contains(MousePos)) return;

    FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);

    // 기즈모 피킹 먼저 시도
    PhysState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

    // 좌클릭일 때만 Body 피킹 시도
    if (Button != 0) return;
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

            // 단위 캡슐(Radius=1, HalfHeight=1) × Scale = 값 그대로 사용
            float HalfHeight = Capsule.Length * 0.5f;

            float HitT;
            if (IntersectRayCapsule(Ray, ShapeCenter, FinalRotation, Capsule.Radius, HalfHeight, HitT))
            {
                if (HitT < ClosestDistance)
                {
                    ClosestDistance = HitT;
                    ClosestBodyIndex = BodyIndex;
                }
            }
        }
    }

    // 피킹 결과 적용
    if (ClosestBodyIndex >= 0)
    {
        SelectBody(ClosestBodyIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
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
            bool bBodySelected = (PhysState && PhysState->SelectedBodyIndex == BodyIndex);
            ImGuiTreeNodeFlags bodyFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (bBodySelected) bodyFlags |= ImGuiTreeNodeFlags_Selected;

            // 아이콘 + 본 이름으로 BodySetup 표시
            FString bodyLabel = FString("[Body] ") + BoneName;

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f)); // 파란색 텍스트
            if (ImGui::TreeNodeEx((void*)(intptr_t)(10000 + BodyIndex), bodyFlags, "%s", bodyLabel.c_str()))
            {
                ImGui::TreePop();
            }
            ImGui::PopStyleColor();

            if (ImGui::IsItemClicked())
            {
                SelectBody(BodyIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
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

    // 트리 노드 렌더링
    bool bSelected = (PhysState->SelectedBodyIndex == BodyIndex);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (ChildBodyIndices.Num() == 0)
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
    const FLinearColor UnselectedColor(0.5f, 0.5f, 0.5f, 0.6f);  // 회색 불투명
    const FLinearColor SelectedColor(0.3f, 0.7f, 1.0f, 0.7f);    // 하늘색 하이라이트

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
        bool bIsSelected = (BodyIndex == PhysState->SelectedBodyIndex);
        FLinearColor ShapeColor = bIsSelected ? SelectedColor : UnselectedColor;

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
            World->AddDebugBox(Transform, ShapeColor, 0);
        }

        // Sphere Shape
        for (int32 i = 0; i < Body->AggGeom.SphereElems.Num(); ++i)
        {
            const FKSphereElem& Sphere = Body->AggGeom.SphereElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Sphere.Center);
            FMatrix Transform = FMatrix::FromTRS(ShapeCenter, FQuat::Identity(), FVector(Sphere.Radius, Sphere.Radius, Sphere.Radius));
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
            float HalfHeight = Capsule.Length * 0.5f;
            World->AddDebugCapsule(Transform, Capsule.Radius, HalfHeight, ShapeColor, 0);
        }
    }
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
    PhysState->bShapesDirty = true;  // Shape 라인 재구성 필요
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
    PhysState->bShapesDirty = true;  // Shape 라인 클리어 필요
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

        // Body가 있으면 삭제 옵션
        if (bHasBody)
        {
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
    if (Body)
    {
        UE_LOG("[PhysicsAssetEditor] Removed body for bone: %s", Body->BoneName.ToString().c_str());
    }

    PhysState->EditingAsset->Bodies.RemoveAt(BodyIndex);
    ClearSelection();
    PhysState->bIsDirty = true;
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
        // 기존 Body에 Shape 추가
        AddShapeToBody(ExistingBody, ShapeType);
        SelectBody(ExistingBodyIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
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
        SelectBody(NewIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
    }

    PhysState->bIsDirty = true;
    const char* ShapeNames[] = { "Box", "Sphere", "Capsule" };
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
        Box.X = 2.0f;  // Half extent
        Box.Y = 2.0f;
        Box.Z = 2.0f;
        Body->AggGeom.BoxElems.Add(Box);
        break;
    }
    case EShapeType::Sphere:
    {
        FKSphereElem Sphere;
        Sphere.Center = FVector(0.0f, 0.0f, 0.0f);
        Sphere.Radius = 2.0f;
        Body->AggGeom.SphereElems.Add(Sphere);
        break;
    }
    case EShapeType::Capsule:
    default:
    {
        FKSphylElem Capsule;
        Capsule.Center = FVector(0.0f, 0.0f, 0.0f);
        Capsule.Rotation = FVector(0.0f, 0.0f, 0.0f);
        Capsule.Radius = 1.5f;
        Capsule.Length = 4.0f;  // 전체 실린더 길이
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
