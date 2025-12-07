-- Game/w14_UIManager.lua
-- D2D 기반 UI 시스템

local GameState = require("Game/w14_GameStateManager")
local ScoreManager = require("Game/w14_ScoreManager")
local AmmoManager = require("Game/w14_AmmoManager")
local HPManager = require("Game/w14_HPManager")
local Audio = require("Game/w14_AudioManager")

local M = {}

-- ===== UI 텍스처 경로 =====
local UI_TEXTURES = {
    LOGO = "Data/Textures/UI/UI_Logo.png",
    START_BUTTON = "Data/Textures/UI/UI_StartButton.png",
    START_BUTTON_HOVER = "Data/Textures/UI/UI_StartButton_Hover.png",
    HOW_TO_PLAY = "Data/Textures/UI/UI_HowToPlay.png",
    HOW_TO_PLAY_HOVER = "Data/Textures/UI/UI_HowToPlay_Hover.png",
    EXIT_BUTTON = "Data/Textures/UI/UI_ExitButton.png",
    EXIT_BUTTON_HOVER = "Data/Textures/UI/UI_ExitButton_Hover.png",
    CROSSHAIR = "Data/Textures/CrossHair.png",
    AMMO_ICON = "Data/Textures/Ammo.png",
    HEADSHOT = "Data/Textures/HeadShot.png",
}

-- ===== 버튼 상태 =====
local buttonStates = {
    start = { hovered = false, rect = { x = 0, y = 0, w = 0, h = 0 } },
    howToPlay = { hovered = false, rect = { x = 0, y = 0, w = 0, h = 0 } },
    exit = { hovered = false, rect = { x = 0, y = 0, w = 0, h = 0 } },
}

-- ===== HUD 상태 =====
local hudState = {
    displayedDistance = 0,
    targetDistance = 0,
    animationSpeed = 50,
}

-- 헤드샷 피드백 타이머
local headshotTimer = 0
local HEADSHOT_DURATION = 0.1 -- 0.5초 동안 표시

-- ===== 설정 상수 =====
local TOP_MARGIN = 10
local CROSSHAIR_SIZE = 64
local HEADSHOT_SIZE = 16

-- ===== 초기화 =====
function M.Init()
    -- 이미지 프리로드
    if HUD then
        for _, path in pairs(UI_TEXTURES) do
            HUD:LoadImage(path)
        end
    end
    

    Audio.Init()
    local result = Audio.RegisterSFX("buttonClicked", "ButtonClickedActor")
    local result = Audio.RegisterSFX("buttonHovered", "ButtonHoveredActor")

    print("[UIManager] Initialized (D2D mode)")
    return true
end

function M.ShowHeadshotFeedback()
    headshotTimer = HEADSHOT_DURATION
end

-- ===== 유틸리티 =====
local function isPointInRect(px, py, rect)
    return px >= rect.x and px <= rect.x + rect.w and
           py >= rect.y and py <= rect.y + rect.h
end

local function getMouseInHUDSpace()
    local mousePos = InputManager:GetMousePosition()
    local offsetX = HUD:GetScreenOffsetX()
    local offsetY = HUD:GetScreenOffsetY()
    return mousePos.X - offsetX, mousePos.Y - offsetY
end

-- 매 프레임 업데이트 (카메라 앞에 위치 유지)
local debugPrinted = false
function M.Update()
    -- Lazy init: 아직 초기화 안됐으면 시도
    if not isInitialized then
        M.Init()
        return
    end

    if not billboardComp or not uiObject then return end

    local camera = GetCamera()
    if not camera then
        if not debugPrinted then
            print("[UIManager] Camera not found!")
            debugPrinted = true
        end
        return
    end

    local camPos = camera:GetActorLocation()
    local camForward = camera:GetActorForward()

    uiObject.Location = camPos + (camForward * uiDistance)

    -- 디버그: 위치 출력 (한 번만)
    if not debugPrinted then
        print("[UIManager] Camera Pos: " .. camPos.X .. ", " .. camPos.Y .. ", " .. camPos.Z)
        print("[UIManager] Camera Forward: " .. camForward.X .. ", " .. camForward.Y .. ", " .. camForward.Z)
        print("[UIManager] UI Pos: " .. uiObject.Location.X .. ", " .. uiObject.Location.Y .. ", " .. uiObject.Location.Z)
        debugPrinted = true
    end
end

-- 카메라 거리 설정
function M.SetDistance(dist)
    uiDistance = dist
end

-- ===== GameHUD API (D2D 기반) =====
-- HUD:BeginFrame()은 매 프레임 시작 시 호출되어야 합니다.
-- HUD:EndFrame()은 D3D11RHI::Present()에서 자동 호출됩니다.

-- HUD 상태 (슬롯머신 애니메이션용)
local hudState = {
    displayedDistance = 0,      -- 화면에 표시되는 거리 (애니메이션)
    targetDistance = 0,         -- 실제 거리
    animationSpeed = 50,        -- 초당 증가량 (m/s)
}

-- 상단 여백 (픽셀)
local TOP_MARGIN = 10

-- 탄환 아이콘 경로
local AMMO_ICON_PATH = "Data/Textures/Ammo.png"
-- 하트 아이콘 경로
local HEART_ICON_PATH = "Data/Textures/Heart.png"

-- 크로스헤어 설정
local CROSSHAIR_PATH = "Data/Textures/CrossHair.png"
local CROSSHAIR_SIZE = 40  -- 크로스헤어 크기 (픽셀)

--- HUD 프레임 시작 (매 프레임 Tick 시작 시 호출)
function M.BeginHUDFrame()
    if HUD then
        HUD:BeginFrame()
    end
end

-- ===== 시작 화면 렌더링 =====
function M.UpdateStartScreen()
    if not HUD or not HUD:IsVisible() then return end

    local screenW = HUD:GetScreenWidth()
    local screenH = HUD:GetScreenHeight()
    local mouseX, mouseY = getMouseInHUDSpace()

    -- 로고 (화면 상단 중앙) - 더 크게
    local logoW = 1000
    local logoH = 600
    local logoX = (screenW - logoW) / 2
    local logoY = 0
    HUD:DrawImage(UI_TEXTURES.LOGO, logoX, logoY, logoW, logoH)

    -- 버튼 크기 - 더 크게
    local btnW = 500
    local btnH = 150
    local btnSpacing = -10  -- 버튼 간격
    local startY = screenH * 0.50  -- 시작 Y 위치
    local btnX = (screenW - btnW) / 2  -- 중앙 정렬

    -- START 버튼 (위)
    local startBtnY = startY
    buttonStates.start.rect = { x = btnX, y = startBtnY, w = btnW, h = btnH }
    local wasStartHovered = buttonStates.start.hovered
    buttonStates.start.hovered = isPointInRect(mouseX, mouseY, buttonStates.start.rect)
    
    -- START 버튼 호버 사운드
    if buttonStates.start.hovered and not wasStartHovered then
        Audio.PlaySFX("buttonHovered")
    end
    
    local startTex = buttonStates.start.hovered and UI_TEXTURES.START_BUTTON_HOVER or UI_TEXTURES.START_BUTTON
    HUD:DrawImage(startTex, btnX, startBtnY, btnW, btnH)

    -- How To Play 버튼 (가운데)
    local howToPlayY = startBtnY + btnH + btnSpacing
    buttonStates.howToPlay.rect = { x = btnX, y = howToPlayY, w = btnW, h = btnH }
    local wasHowToPlayHovered = buttonStates.howToPlay.hovered
    buttonStates.howToPlay.hovered = isPointInRect(mouseX, mouseY, buttonStates.howToPlay.rect)
    
    -- How To Play 버튼 호버 사운드
    if buttonStates.howToPlay.hovered and not wasHowToPlayHovered then
        Audio.PlaySFX("buttonHovered")
    end
    
    local howToPlayTex = buttonStates.howToPlay.hovered and UI_TEXTURES.HOW_TO_PLAY_HOVER or UI_TEXTURES.HOW_TO_PLAY
    HUD:DrawImage(howToPlayTex, btnX, howToPlayY, btnW, btnH)

    -- EXIT 버튼 (아래)
    local exitY = howToPlayY + btnH + btnSpacing
    buttonStates.exit.rect = { x = btnX, y = exitY, w = btnW, h = btnH }
    local wasExitHovered = buttonStates.exit.hovered
    buttonStates.exit.hovered = isPointInRect(mouseX, mouseY, buttonStates.exit.rect)
    
    -- EXIT 버튼 호버 사운드
    if buttonStates.exit.hovered and not wasExitHovered then
        Audio.PlaySFX("buttonHovered")
    end
    
    local exitTex = buttonStates.exit.hovered and UI_TEXTURES.EXIT_BUTTON_HOVER or UI_TEXTURES.EXIT_BUTTON
    HUD:DrawImage(exitTex, btnX, exitY, btnW, btnH)

    -- 클릭 처리
    if InputManager:IsMouseButtonPressed(0) then
        if buttonStates.start.hovered then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] START clicked")
            GameState.StartGame()
            InputManager:SetCursorVisible(false)  -- 게임 시작 시 커서 숨김
        elseif buttonStates.howToPlay.hovered then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] HOW TO PLAY clicked")
            -- TODO: How To Play 화면 구현
        elseif buttonStates.exit.hovered then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] EXIT clicked")
            QuitGame()
        end
    end
end

-- ===== 게임 HUD 렌더링 =====
function M.UpdateGameHUD(dt)
    if not HUD or not HUD:IsVisible() then return end

    dt = dt or 0.016

    local screenW = HUD:GetScreenWidth()
    local screenH = HUD:GetScreenHeight()

    -- ScoreManager에서 값 가져오기
    local distance = ScoreManager.GetDistance()
    local kills = ScoreManager.GetKillCount()
    local score = ScoreManager.GetScore()
    local ammo = AmmoManager.GetCurrentAmmo()
    local currentHP = HPManager.GetCurrentHP()
    local maxHP = HPManager.GetMaxHP()

    -- 슬롯머신 애니메이션
    hudState.targetDistance = distance
    if hudState.displayedDistance < hudState.targetDistance then
        local diff = hudState.targetDistance - hudState.displayedDistance
        local speed = math.max(hudState.animationSpeed, diff * 2)
        hudState.displayedDistance = math.min(
            hudState.displayedDistance + speed * dt,
            hudState.targetDistance
        )
    end

    local leftMargin = screenW * 0.02

    -- 거리 표시 (좌측 상단)
    HUD:DrawText(
        ScoreManager.FormatDistance(hudState.displayedDistance),
        leftMargin, TOP_MARGIN,
        36,
        Color(1, 1, 1, 1)
    )

    -- 킬수 표시
    HUD:DrawText(
        "Kills: " .. ScoreManager.FormatNumber(kills),
        leftMargin, TOP_MARGIN + 45,
        28,
        Color(1, 1, 1, 1)
    )

    -- 점수 표시
    HUD:DrawText(
        "Score: " .. ScoreManager.FormatNumber(score),
        leftMargin, TOP_MARGIN + 75,
        28,
        Color(1, 1, 1, 1)
    )

    -- HP 바 (화면 하단 중앙) - 업그레이드 디자인
    local hpRatio = currentHP / maxHP
    local barMaxWidth = 450  -- 최대 바 너비 (증가)
    local barHeight = 35     -- 바 높이 (증가)
    local barWidth = barMaxWidth * hpRatio  -- 현재 HP에 따른 바 너비

    -- HP 바 위치 (화면 하단 중앙)
    local barBottomMargin = 50  -- 하단 여백
    local barX = (screenW - barMaxWidth) / 2  -- 중앙 정렬
    local barY = screenH - barBottomMargin - barHeight

    -- HP 비율에 따른 색상 결정 (빨간색 → 검은색)
    -- HP가 높을수록 빨간색, 낮을수록 검은색
    local hpColor = Color(
        1.0,                    -- R: 항상 최대
        hpRatio * 0.1,          -- G: HP가 낮을수록 어두워짐
        hpRatio * 0.1,          -- B: HP가 낮을수록 어두워짐
        1.0                     -- A: 불투명
    )

    -- 외부 테두리 (검은색, 두껍게)
    local borderSize = 3
    HUD:DrawRect(barX - borderSize, barY - borderSize, barMaxWidth + borderSize * 2, barHeight + borderSize * 2, Color(0, 0, 0, 0.9))

    -- 배경 바 (어두운 회색)
    HUD:DrawRect(barX, barY, barMaxWidth, barHeight, Color(0.2, 0.2, 0.2, 0.9))

    -- 현재 HP 바 (색상 변화)
    if barWidth > 0 then
        HUD:DrawRect(barX, barY, barWidth, barHeight, hpColor)

        -- HP 바 위에 밝은 하이라이트 효과 (상단 절반)
        local highlightHeight = barHeight * 0.4
        HUD:DrawRect(barX, barY, barWidth, highlightHeight, Color(1, 1, 1, 0.2))
    end

    -- 하트 아이콘 (HP 바 왼쪽)
    local heartSize = 45
    local heartX = barX - heartSize - 15
    local heartY = barY - 5
    HUD:DrawImage(
        HEART_ICON_PATH,
        heartX, heartY,
        heartSize, heartSize
    )

    -- HP 텍스트 (바 오른쪽에 표시, Bullet 스타일과 동일)
    local hpText = currentHP .. " | " .. maxHP  -- "10 | 10" 형식
    local hpTextX = barX + barMaxWidth + 15  -- 바 오른쪽
    local hpTextY = barY - 12  -- 세로 중앙 정렬
    HUD:DrawText(
        hpText,
        hpTextX, hpTextY,
        36,  -- Bullet과 동일한 폰트 크기
        Color(1, 1, 1, 1)
    )

    -- 탄약 수 표시 (화면 중앙 상단) - 탄창/예비탄약 형식 (30|120)
    local totalAmmo = AmmoManager.GetTotalAmmo()
    local ammoText = ammo .. " | " .. totalAmmo
    local ammoCenterX = screenW * 0.5 - 50
    HUD:DrawText(
        ammoText,
        ammoCenterX, TOP_MARGIN,
        36,
        Color(1, 1, 1, 1)
    )

    -- 탄약 아이콘
    local ammoIconX = ammoCenterX + 150
    HUD:DrawImage(
        UI_TEXTURES.AMMO_ICON,
        ammoIconX, TOP_MARGIN + 10,
        40, 40
    )

    -- 크로스헤어 (화면 정중앙)
    local centerOffsetY = 15
    local centerX = screenW / 2
    local centerY = screenH / 2
    HUD:DrawImage(
        UI_TEXTURES.CROSSHAIR,
        centerX - CROSSHAIR_SIZE / 2,
        (centerY - CROSSHAIR_SIZE / 2) - centerOffsetY,
        CROSSHAIR_SIZE,
        CROSSHAIR_SIZE
    )

    if headshotTimer > 0 then
        -- 타이머 감소
        headshotTimer = headshotTimer - dt

        -- 붉은 X 표시 그리기
        -- 수학 공식: (화면중앙) - (이미지크기의 절반) = 이미지가 정중앙에 위치함
        HUD:DrawImage(
            UI_TEXTURES.HEADSHOT,
            centerX - HEADSHOT_SIZE / 2,                    -- X좌표: 내 크기 기준으로 중앙 정렬
            (centerY - HEADSHOT_SIZE / 2) - centerOffsetY,  -- Y좌표: 내 크기 기준 중앙 + 오프셋 보정
            HEADSHOT_SIZE,                                  -- 가로 크기
            HEADSHOT_SIZE                                   -- 세로 크기
        )
    end
end

-- ===== HUD 상태 초기화 =====
function M.ResetHUD()
    hudState.displayedDistance = 0
    hudState.targetDistance = 0
end

-- ===== 메인 업데이트 (상태에 따라 다른 UI 표시) =====
function M.Update(dt)
    local state = GameState.GetState()

    if state == GameState.States.START then
        M.UpdateStartScreen()
    elseif state == GameState.States.PLAYING then
        M.UpdateGameHUD(dt)
    elseif state == GameState.States.END then
        -- TODO: 게임 오버 화면
        M.UpdateStartScreen()  -- 임시로 시작 화면 재사용
    end
end

-- ===== 레거시 API (호환성) =====
function M.DrawText(text, x, y, size, r, g, b)
    if not HUD then return end
    size = size or 24
    r = r or 1
    g = g or 1
    b = b or 1
    HUD:DrawText(text, x, y, size, Color(r, g, b, 1))
end

function M.DrawTextRel(text, rx, ry, size, r, g, b)
    if not HUD then return end
    size = size or 24
    r = r or 1
    g = g or 1
    b = b or 1
    HUD:DrawTextRel(text, rx, ry, size, Color(r, g, b, 1))
end

function M.ToggleHUD()
    if HUD then
        HUD:SetVisible(not HUD:IsVisible())
    end
end

function M.SetHUDScreenSize(width, height)
    if HUD then
        HUD:SetScreenSize(width, height)
    end
end

return M
