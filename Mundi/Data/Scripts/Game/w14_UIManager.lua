-- Game/w14_UIManager.lua
-- D2D 기반 UI 시스템

local GameState = require("Game/w14_GameStateManager")
local ScoreManager = require("Game/w14_ScoreManager")
local AmmoManager = require("Game/w14_AmmoManager")

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

-- ===== 설정 상수 =====
local TOP_MARGIN = 10
local CROSSHAIR_SIZE = 64

-- ===== 초기화 =====
function M.Init()
    -- 이미지 프리로드
    if HUD then
        for _, path in pairs(UI_TEXTURES) do
            HUD:LoadImage(path)
        end
    end
    print("[UIManager] Initialized (D2D mode)")
    return true
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

-- ===== HUD 프레임 =====
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

    -- 로고 (화면 상단 중앙)
    local logoW = 600
    local logoH = 225
    local logoX = (screenW - logoW) / 2
    local logoY = screenH * 0.10
    HUD:DrawImage(UI_TEXTURES.LOGO, logoX, logoY, logoW, logoH)

    -- 버튼 크기
    local btnW = 200
    local btnH = 60
    local btnY = screenH * 0.55  -- 화면 중앙 아래
    local btnSpacing = 40  -- 버튼 간격

    -- 버튼 X 위치 계산 (가운데 정렬)
    local totalWidth = btnW * 3 + btnSpacing * 2
    local startX = (screenW - totalWidth) / 2

    -- How To Play 버튼 (왼쪽)
    local howToPlayX = startX
    buttonStates.howToPlay.rect = { x = howToPlayX, y = btnY, w = btnW, h = btnH }
    buttonStates.howToPlay.hovered = isPointInRect(mouseX, mouseY, buttonStates.howToPlay.rect)
    local howToPlayTex = buttonStates.howToPlay.hovered and UI_TEXTURES.HOW_TO_PLAY_HOVER or UI_TEXTURES.HOW_TO_PLAY
    HUD:DrawImage(howToPlayTex, howToPlayX, btnY, btnW, btnH)

    -- START 버튼 (가운데)
    local startBtnX = startX + btnW + btnSpacing
    buttonStates.start.rect = { x = startBtnX, y = btnY, w = btnW, h = btnH }
    buttonStates.start.hovered = isPointInRect(mouseX, mouseY, buttonStates.start.rect)
    local startTex = buttonStates.start.hovered and UI_TEXTURES.START_BUTTON_HOVER or UI_TEXTURES.START_BUTTON
    HUD:DrawImage(startTex, startBtnX, btnY, btnW, btnH)

    -- EXIT 버튼 (오른쪽)
    local exitX = startX + (btnW + btnSpacing) * 2
    buttonStates.exit.rect = { x = exitX, y = btnY, w = btnW, h = btnH }
    buttonStates.exit.hovered = isPointInRect(mouseX, mouseY, buttonStates.exit.rect)
    local exitTex = buttonStates.exit.hovered and UI_TEXTURES.EXIT_BUTTON_HOVER or UI_TEXTURES.EXIT_BUTTON
    HUD:DrawImage(exitTex, exitX, btnY, btnW, btnH)

    -- 클릭 처리
    if InputManager:IsMouseButtonPressed(0) then
        if buttonStates.start.hovered then
            print("[UIManager] START clicked")
            GameState.StartGame()
            InputManager:SetCursorVisible(false)  -- 게임 시작 시 커서 숨김
        elseif buttonStates.howToPlay.hovered then
            print("[UIManager] HOW TO PLAY clicked")
            -- TODO: How To Play 화면 구현
        elseif buttonStates.exit.hovered then
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

    -- 탄약 수 표시 (화면 중앙 상단)
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
    local centerX = screenW / 2
    local centerY = screenH / 2
    HUD:DrawImage(
        UI_TEXTURES.CROSSHAIR,
        centerX - CROSSHAIR_SIZE / 2,
        centerY - CROSSHAIR_SIZE / 2,
        CROSSHAIR_SIZE,
        CROSSHAIR_SIZE
    )
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
