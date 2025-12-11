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
    RESTART_BUTTON = "Data/Textures/UI/UI_RestartButton.png",
    RESTART_BUTTON_HOVER = "Data/Textures/UI/UI_RestartButton_Hover.png",
    HOW_TO_PLAY_IMG = "Data/Textures/UI/HowToPlay.png",
    CROSSHAIR = "Data/Textures/CrossHair.png",
    AMMO_ICON = "Data/Textures/Ammo.png",
    SHOT = "Data/Textures/Shot.png",
    HEADSHOT = "Data/Textures/HeadShot.png",
    -- 보스 관련 아이콘
    BOSS_ICON = "Data/Textures/UI/UI_BossIcon.png",
    BOSS_HP_ICON = "Data/Textures/UI/UI_BossHPIcon.png",
    BOSS_ATK_ICON = "Data/Textures/UI/UI_BossAtkIcon.png",
    -- 아드레날린 아이콘
    ADRENALINE_ICON = "Data/Textures/UI/UI_ItemAdrenalineIcon.png",
    -- 플레이어 HP/탄약 아이콘
    PLAYER_HP_ICON = "Data/Textures/UI/UI_PlayerHPIcon.png",
    PLAYER_AMMO_ICON = "Data/Textures/UI/UI_PlayerAmmoIcon.png",
}

-- ===== 버튼 상태 =====
local buttonStates = {
    start = { hovered = false, rect = { x = 0, y = 0, w = 0, h = 0 } },
    howToPlay = { hovered = false, rect = { x = 0, y = 0, w = 0, h = 0 } },
    exit = { hovered = false, rect = { x = 0, y = 0, w = 0, h = 0 } },
    -- 게임 오버 화면용 버튼
    restart = { hovered = false, rect = { x = 0, y = 0, w = 0, h = 0 } },
    gameOverExit = { hovered = false, rect = { x = 0, y = 0, w = 0, h = 0 } },
}

local currentButtonIndex = 1
local lastDpadVertical = 0

-- ===== HUD 상태 =====
local hudState = {
    displayedDistance = 0,
    targetDistance = 0,
    animationSpeed = 50,
}

-- 헤드샷 피드백 타이머
local headshotTimer = 0
local HEADSHOT_DURATION = 0.1 -- 0.1초 동안 표시

-- 일반 샷 피드백 타이머
local shotTimer = 0
local SHOT_DURATION = 0.08 -- 0.08초 동안 표시

-- ADS 상태 (줌 중일 때 크로스헤어 숨김)
local isADS = false

-- ===== 설정 상수 =====
local TOP_MARGIN = 10
local CROSSHAIR_SIZE = 64
local HEADSHOT_SIZE = 32
local SHOT_SIZE = 24

local showHowToPlayImage = false
-- 초기화 플래그
local isInitialized = false

-- ===== 보스 매니저 참조 (외부에서 설정) =====
local bossManagerRef = nil
local playerRef = nil

function M.SetBossManager(bossManager)
    bossManagerRef = bossManager
end

function M.SetPlayer(player)
    playerRef = player
end

-- ===== 초기화 =====
function M.Init()
    if isInitialized then return true end

    -- 이미지 프리로드
    if HUD then
        for _, path in pairs(UI_TEXTURES) do
            HUD:LoadImage(path)
        end
    end

    Audio.Init()
    Audio.RegisterSFX("buttonClicked", "ButtonClickedActor")
    Audio.RegisterSFX("buttonHovered", "ButtonHoveredActor")

    isInitialized = true
    print("[UIManager] Initialized (D2D mode)")
    return true
end

function M.ShowHeadshotFeedback()
    headshotTimer = HEADSHOT_DURATION
end

function M.ShowShotFeedback()
    shotTimer = SHOT_DURATION
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

-- HP 비율에 따른 테마 색상 계산 (흰색 -> 붉은색)
local function getThemeColor(hpRatio, baseAlpha)
    baseAlpha = baseAlpha or 1.0
    -- HP 100%: 흰색(1,1,1), HP 0%: 붉은색(1,0.3,0.3)
    local r = 1.0
    local g = 0.3 + 0.7 * hpRatio  -- 1.0 -> 0.3
    local b = 0.3 + 0.7 * hpRatio  -- 1.0 -> 0.3
    return Color(r, g, b, baseAlpha)
end

-- HP 비율에 따른 배경 색상 (어두운 버전)
local function getThemeBgColor(hpRatio, baseAlpha)
    baseAlpha = baseAlpha or 0.7
    local r = 0.1 + 0.1 * (1 - hpRatio)  -- 0.1 -> 0.2
    local g = 0.12 * hpRatio             -- 0.12 -> 0
    local b = 0.15 * hpRatio             -- 0.15 -> 0
    return Color(r, g, b, baseAlpha)
end


-- ===== GameHUD API (D2D 기반) =====
-- HUD:BeginFrame()은 매 프레임 시작 시 호출되어야 합니다.
-- HUD:EndFrame()은 D3D11RHI::Present()에서 자동 호출됩니다.

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
    local logoY = -80
    HUD:DrawImage(UI_TEXTURES.LOGO, logoX, logoY, logoW, logoH)

    -- 버튼 크기 - 더 크게
    local btnW = 350  -- 버튼 폭 조정 (이미지 길이 맞추기)
    local btnH = 150
    local btnSpacing = -50  -- 버튼 간격
    local startY = screenH * 0.45  -- 시작 Y 위치
    local btnX = 55  -- 왼쪽 정렬
    local howToPlayBtnX = 100  -- 왼쪽 정렬

    -- ===== 패드 십자키 네비게이션 =====
    if InputManager:IsGamepadConnected(0) then
        local dpadUp = InputManager:IsGamepadButtonPressed(GamepadButton.DPadUp, 0)
        local dpadDown = InputManager:IsGamepadButtonPressed(GamepadButton.DPadDown, 0)

        -- 버튼 전환 (방향 입력이 새로 들어왔을 때만)
        if dpadUp and lastDpadVertical <= 0 then
            currentButtonIndex = math.max(1, currentButtonIndex - 1)
            Audio.PlaySFX("buttonHovered")
        elseif dpadDown and lastDpadVertical >= 0 then
            currentButtonIndex = math.min(3, currentButtonIndex + 1)
            Audio.PlaySFX("buttonHovered")
        end

        lastDpadVertical = (dpadUp and 1) or (dpadDown and -1) or 0
    end

    -- How To Play 버튼 (맨 위)
    local howToPlayY = startY
    buttonStates.howToPlay.rect = { x = howToPlayBtnX, y = howToPlayY, w = btnW, h = btnH }
    local wasHowToPlayHovered = buttonStates.howToPlay.hovered

    -- 마우스 호버 또는 패드 선택
    if InputManager:IsGamepadConnected(0) then
        buttonStates.howToPlay.hovered = (currentButtonIndex == 1)
    else
        buttonStates.howToPlay.hovered = isPointInRect(mouseX, mouseY, buttonStates.howToPlay.rect)
    end

    -- How To Play 버튼 호버 사운드
    if buttonStates.howToPlay.hovered and not wasHowToPlayHovered then
        Audio.PlaySFX("buttonHovered")
    end

    local howToPlayTex = buttonStates.howToPlay.hovered and UI_TEXTURES.HOW_TO_PLAY_HOVER or UI_TEXTURES.HOW_TO_PLAY
    HUD:DrawImage(howToPlayTex, howToPlayBtnX, howToPlayY, btnW, btnH)

    -- START 버튼 (가운데)
    local startBtnY = howToPlayY + btnH + btnSpacing + 30
    buttonStates.start.rect = { x = btnX, y = startBtnY, w = btnW, h = btnH }
    local wasStartHovered = buttonStates.start.hovered

    -- 마우스 호버 또는 패드 선택
    if InputManager:IsGamepadConnected(0) then
        buttonStates.start.hovered = (currentButtonIndex == 2)
    else
        buttonStates.start.hovered = isPointInRect(mouseX, mouseY, buttonStates.start.rect)
    end

    -- START 버튼 호버 사운드
    if buttonStates.start.hovered and not wasStartHovered then
        Audio.PlaySFX("buttonHovered")
    end

    local startTex = buttonStates.start.hovered and UI_TEXTURES.START_BUTTON_HOVER or UI_TEXTURES.START_BUTTON
    HUD:DrawImage(startTex, btnX, startBtnY, btnW, btnH)

    -- EXIT 버튼 (아래)
    local exitY = startBtnY + btnH + btnSpacing
    buttonStates.exit.rect = { x = btnX, y = exitY, w = btnW, h = btnH }
    local wasExitHovered = buttonStates.exit.hovered

    -- 마우스 호버 또는 패드 선택
    if InputManager:IsGamepadConnected(0) then
        buttonStates.exit.hovered = (currentButtonIndex == 3)
    else
        buttonStates.exit.hovered = isPointInRect(mouseX, mouseY, buttonStates.exit.rect)
    end

    -- EXIT 버튼 호버 사운드
    if buttonStates.exit.hovered and not wasExitHovered then
        Audio.PlaySFX("buttonHovered")
    end

    local exitTex = buttonStates.exit.hovered and UI_TEXTURES.EXIT_BUTTON_HOVER or UI_TEXTURES.EXIT_BUTTON
    HUD:DrawImage(exitTex, btnX, exitY, btnW, btnH)

    -- How To Play 이미지 표시 (활성화되면)
    if showHowToPlayImage then
        local imgW = 700
        local imgH = 400
        local imgX = howToPlayBtnX + btnW + 80  -- 버튼 오른쪽에 위치
        local imgY = screenH * 0.45  -- 중앙 정렬
        HUD:DrawImage(UI_TEXTURES.HOW_TO_PLAY_IMG, imgX, imgY, imgW, imgH)
    end

    -- ★ 왼쪽 아래 크레딧 텍스트
    HUD:DrawText(
        "게임테크랩 2기 - 국동희, 허준, 홍신화, 허승빈",
        10,  -- X 좌표 (왼쪽에서 10픽셀)
        screenH - 40,  -- Y 좌표 (아래에서 30픽셀)
        20,  -- 폰트 크기
        Color(0.8, 0.8, 0.8, 1)  -- 밝은 회색
    )

    -- 클릭 처리 (마우스 또는 패드)
    if InputManager:IsMouseButtonPressed(0) then
        if buttonStates.start.hovered then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] START clicked")
            GameState.StartGame()
            InputManager:SetCursorVisible(false)  -- 게임 시작 시 커서 숨김
        elseif buttonStates.howToPlay.hovered then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] HOW TO PLAY clicked")
            -- How To Play 이미지 토글
            showHowToPlayImage = not showHowToPlayImage
        elseif buttonStates.exit.hovered then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] EXIT clicked")
            QuitGame()
        end
    end

    -- 패드 A 버튼으로 선택
    if InputManager:IsGamepadConnected(0) and InputManager:IsGamepadButtonPressed(GamepadButton.A, 0) then
        if currentButtonIndex == 1 then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] HOW TO PLAY clicked (Gamepad)")
            showHowToPlayImage = not showHowToPlayImage
        elseif currentButtonIndex == 2 then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] START clicked (Gamepad)")
            GameState.StartGame()
            InputManager:SetCursorVisible(false)
        elseif currentButtonIndex == 3 then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] EXIT clicked (Gamepad)")
            QuitGame()
        end
    end
end

-- ===== GTFO 스타일 HP 바 그리기 헬퍼 함수 =====
-- 세그먼트로 나눠진 HP 바 (GTFO 스타일)
local function drawSegmentedBar(x, y, width, height, fillRatio, segments, fillColor, bgColor, borderColor)
    segments = segments or 10
    local segmentGap = 2
    local totalGap = segmentGap * (segments - 1)
    local segmentWidth = (width - totalGap) / segments

    -- 배경 (전체 바 영역)
    HUD:DrawRect(x - 2, y - 2, width + 4, height + 4, borderColor or Color(0.3, 0.3, 0.3, 0.5))

    -- 각 세그먼트 그리기
    for i = 0, segments - 1 do
        local segX = x + i * (segmentWidth + segmentGap)
        local segmentRatio = i / segments
        local nextSegmentRatio = (i + 1) / segments

        -- 이 세그먼트가 채워져야 하는지 확인
        if fillRatio >= nextSegmentRatio then
            -- 완전히 채워진 세그먼트
            HUD:DrawRect(segX, y, segmentWidth, height, fillColor)
        elseif fillRatio > segmentRatio then
            -- 부분적으로 채워진 세그먼트
            local partialFill = (fillRatio - segmentRatio) / (nextSegmentRatio - segmentRatio)
            HUD:DrawRect(segX, y, segmentWidth, height, bgColor)
            HUD:DrawRect(segX, y, segmentWidth * partialFill, height, fillColor)
        else
            -- 빈 세그먼트
            HUD:DrawRect(segX, y, segmentWidth, height, bgColor)
        end
    end
end

-- GTFO 스타일 단순 바 (세그먼트 없이)
local function drawSimpleBar(x, y, width, height, fillRatio, fillColor, bgColor)
    -- 배경
    HUD:DrawRect(x, y, width, height, bgColor)
    -- 채워진 부분
    if fillRatio > 0 then
        HUD:DrawRect(x, y, width * fillRatio, height, fillColor)
    end
end

-- 양쪽에서 중앙으로 감소하는 HP 바 (이미지 참고 형식)
-- [====== GAP ======] 형식 - 중앙에 텍스트 공간 확보
local function drawCenterHPBar(centerX, y, totalWidth, height, fillRatio, fillColor, bgColor, centerGap)
    centerGap = centerGap or 0  -- 중앙 간격 (텍스트용)
    local halfGap = centerGap / 2
    local barWidth = (totalWidth - centerGap) / 2  -- 각 바의 너비
    local filledBarWidth = barWidth * fillRatio

    -- 왼쪽 바 (왼쪽 끝에서 중앙 방향으로)
    local leftBarX = centerX - halfGap - barWidth
    HUD:DrawRect(leftBarX, y, barWidth, height, bgColor)
    if fillRatio > 0 then
        -- 왼쪽 바는 오른쪽 끝(중앙 근처)에서부터 채워짐
        HUD:DrawRect(centerX - halfGap - filledBarWidth, y, filledBarWidth, height, fillColor)
    end

    -- 오른쪽 바 (중앙에서 오른쪽 끝 방향으로)
    local rightBarX = centerX + halfGap
    HUD:DrawRect(rightBarX, y, barWidth, height, bgColor)
    if fillRatio > 0 then
        -- 오른쪽 바는 왼쪽 끝(중앙 근처)에서부터 채워짐
        HUD:DrawRect(centerX + halfGap, y, filledBarWidth, height, fillColor)
    end
end

-- 세로 바 (아드레날린 부스트용)
local function drawVerticalBar(x, y, width, height, fillRatio, fillColor, bgColor)
    -- 배경
    HUD:DrawRect(x, y, width, height, bgColor)
    -- 채워진 부분 (아래에서 위로)
    if fillRatio > 0 then
        local filledHeight = height * fillRatio
        HUD:DrawRect(x, y + height - filledHeight, width, filledHeight, fillColor)
    end
end

-- ===== 게임 HUD 렌더링 (미니멀 스타일 - 2배 사이즈) =====
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
    local totalAmmo = AmmoManager.GetTotalAmmo()

    -- HP 비율 계산 (테마 색상용)
    local hpRatio = math.max(0, math.min(1, currentHP / maxHP))

    -- 테마 색상 (HP에 따라 흰색 -> 붉은색)
    local themeColor = getThemeColor(hpRatio, 1.0)
    local themeDimColor = getThemeColor(hpRatio, 0.6)

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

    -- ===== 좌측 상단: 거리/킬/점수 (미니멀, 2배 사이즈) =====
    local leftMargin = 40
    local topMargin = 30

    -- 거리 표시 (큰 숫자)
    HUD:DrawText(
        ScoreManager.FormatDistance(hudState.displayedDistance),
        leftMargin, topMargin,
        48,  -- 2배
        themeColor
    )

    -- 킬수 & 점수 (수평 배치, 간격 줄임)
    local statsY = topMargin + 60
    HUD:DrawText("KILLS " .. ScoreManager.FormatNumber(kills), leftMargin, statsY, 24, themeColor)
    HUD:DrawText("SCORE " .. ScoreManager.FormatNumber(score), leftMargin + 120, statsY, 24, themeColor)

    -- ===== 상단 중앙: 보스 정보 (수평 배치, 2배 사이즈) =====
    local bossInfo = nil
    local timeToNextBoss = nil
    local isBossAlive = false

    if bossManagerRef then
        isBossAlive = bossManagerRef.current_boss ~= nil
        if isBossAlive and bossManagerRef.current_boss then
            local bossScript = bossManagerRef.current_boss:GetScript()
            if bossScript then
                bossInfo = {
                    hp = bossScript.GetCurrentHP and bossScript.GetCurrentHP() or 0,
                    maxHp = bossScript.GetMaxHP and bossScript.GetMaxHP() or 100,
                    damage = bossScript.GetDamageValue and bossScript.GetDamageValue() or 0
                }
            end
        else
            timeToNextBoss = bossManagerRef.next_spawn_time - bossManagerRef.current_spawn_time
        end
    end

    local topCenterX = screenW / 2
    local topCenterY = 30

    if isBossAlive and bossInfo then
        -- 보스 HP 비율
        local bossHpRatio = math.max(0, math.min(1, bossInfo.hp / bossInfo.maxHp))

        -- 보스 아이콘 (크게 강조) - 상단 중앙
        local iconSize = 64  -- 2배 이상
        HUD:DrawImage(UI_TEXTURES.BOSS_ICON, topCenterX - iconSize / 2, topCenterY, iconSize, iconSize)

        -- 보스 HP 바 (양쪽에서 중앙으로 감소, 중앙에 텍스트)
        local bossBarW = 500
        local bossBarH = 8
        local bossBarY = topCenterY + iconSize + 15
        local bossTextGap = 70  -- 텍스트를 위한 중앙 간격
        local bossHpColor = Color(1, 0.2 + 0.3 * bossHpRatio, 0.2, 1)
        local bossHpBgColor = Color(0.3, 0.3, 0.3, 0.5)
        drawCenterHPBar(topCenterX, bossBarY, bossBarW, bossBarH, bossHpRatio, bossHpColor, bossHpBgColor, bossTextGap)

        -- 보스 HP 퍼센트 (두 바 사이 중앙)
        local bossHpPercent = math.floor(bossHpRatio * 100)
        HUD:DrawText(bossHpPercent .. "%", topCenterX - 25, bossBarY - 14, 24, Color(1, 1, 1, 1))

        -- 보스 정보 수평 배치 (HP 아이콘 - 수치 | ATK 아이콘 - 수치)
        local infoY = bossBarY + 15
        local hpIconSize = 36  -- 2배
        local atkIconSize = 36

        -- HP 정보 (왼쪽)
        local hpInfoX = topCenterX - 180
        HUD:DrawImage(UI_TEXTURES.BOSS_HP_ICON, hpInfoX, infoY, hpIconSize, hpIconSize)
        HUD:DrawText(math.floor(bossInfo.hp) .. "/" .. math.floor(bossInfo.maxHp), hpInfoX + hpIconSize + 8, infoY - 3, 22, Color(1, 0.8, 0.8, 1))

        -- ATK 정보 (오른쪽)
        local atkInfoX = topCenterX + 50
        HUD:DrawImage(UI_TEXTURES.BOSS_ATK_ICON, atkInfoX, infoY, atkIconSize, atkIconSize)
        HUD:DrawText(tostring(math.floor(bossInfo.damage)), atkInfoX + atkIconSize + 8, infoY - 3, 22, Color(1, 0.5, 0.5, 1))

    elseif timeToNextBoss and timeToNextBoss > 0 then
        -- 보스 등장까지 남은 시간 (미니멀)
        HUD:DrawText("NEXT BOSS  " .. string.format("%.0f", timeToNextBoss) .. "s", topCenterX - 80, topCenterY + 10, 28, themeColor)
    end

    -- ===== 하단 중앙: 플레이어 HP 바 (양쪽에서 중앙으로 감소, 1.5배 길이) =====
    local playerBarW = 750   -- 1.5배 길이 (500 * 1.5)
    local playerBarH = 6
    local playerBarY = screenH - 70
    local hpTextGap = 120    -- 아이콘 + 텍스트를 위한 중앙 간격

    -- HP 바 (양쪽에서 중앙으로 감소, 중앙에 아이콘+텍스트 간격)
    local hpBarColor = hpRatio > 0.3 and themeColor or Color(1, 0.3, 0.3, 1)
    local hpBarBgColor = Color(0.3, 0.3, 0.3, 0.5)
    drawCenterHPBar(topCenterX, playerBarY, playerBarW, playerBarH, hpRatio, hpBarColor, hpBarBgColor, hpTextGap)

    -- 플레이어 HP 아이콘 (바 중앙, 텍스트 왼쪽)
    local hpIconSize = 36
    local hpIconX = topCenterX - 55
    local hpIconY = playerBarY - hpIconSize / 2 + playerBarH / 2
    HUD:DrawImage(UI_TEXTURES.PLAYER_HP_ICON, hpIconX, hpIconY, hpIconSize, hpIconSize)

    -- HP 퍼센트 텍스트 (아이콘 오른쪽, 3px 위로)
    local hpPercent = math.floor(hpRatio * 100)
    local hpTextColor = hpRatio > 0.3 and themeColor or Color(1, 0.3, 0.3, 1)
    HUD:DrawText(hpPercent .. "%", hpIconX + hpIconSize + 8, playerBarY - 18, 28, hpTextColor)

    -- ===== 우측 하단: 탄약 정보 (아이콘 + 수치 수평 배치) =====
    local ammoIconSize = 48
    local ammoY = screenH - 80
    local ammoIconX = screenW - 220

    -- 탄약 아이콘
    HUD:DrawImage(UI_TEXTURES.PLAYER_AMMO_ICON, ammoIconX, ammoY - ammoIconSize / 2, ammoIconSize, ammoIconSize)

    -- 탄약 수치 (아이콘 오른쪽, 수평 배치, 3px 위로)
    local ammoColor = ammo <= 10 and Color(1, 0.3, 0.3, 1) or themeColor
    local ammoTextX = ammoIconX + ammoIconSize + 10
    local ammoTextY = ammoY - 21
    HUD:DrawText(tostring(ammo) .. " / " .. tostring(totalAmmo), ammoTextX, ammoTextY, 28, ammoColor)

    -- ===== 우측: 아드레날린 부스트 바 (세로) =====
    local adrenalineRatio = 0
    if playerRef then
        local playerScript = playerRef:GetScript()
        if playerScript and playerScript.GetAdrenalineBoostRatio then
            adrenalineRatio = playerScript.GetAdrenalineBoostRatio()
        end
    end

    if adrenalineRatio > 0 then
        local boostBarX = screenW - 60
        local boostBarY = screenH / 2 - 100
        local boostBarW = 12      -- 2배
        local boostBarH = 200     -- 2배

        -- 아드레날린 아이콘 (바 위)
        local adrIconSize = 48
        HUD:DrawImage(UI_TEXTURES.ADRENALINE_ICON, boostBarX - adrIconSize / 2 + boostBarW / 2, boostBarY - adrIconSize - 10, adrIconSize, adrIconSize)

        -- 세로 바
        local boostColor = Color(0.2, 1, 0.4, 1)  -- 녹색
        local boostBgColor = Color(0.1, 0.3, 0.15, 0.6)
        drawVerticalBar(boostBarX, boostBarY, boostBarW, boostBarH, adrenalineRatio, boostColor, boostBgColor)
    end

    -- ===== 크로스헤어 (미니멀, 2배 사이즈) =====
    local centerOffsetY = 15
    local centerX = screenW / 2
    local centerY = screenH / 2 - centerOffsetY

    if not isADS then
        local crossGap = 10     -- 2배
        local lineLen = 20      -- 2배
        local lineThick = 3     -- 2배

        -- 상하좌우 라인
        HUD:DrawRect(centerX - lineThick/2, centerY - crossGap - lineLen, lineThick, lineLen, themeColor)
        HUD:DrawRect(centerX - lineThick/2, centerY + crossGap, lineThick, lineLen, themeColor)
        HUD:DrawRect(centerX - crossGap - lineLen, centerY - lineThick/2, lineLen, lineThick, themeColor)
        HUD:DrawRect(centerX + crossGap, centerY - lineThick/2, lineLen, lineThick, themeColor)

        -- 중앙 점
        HUD:DrawRect(centerX - 2, centerY - 2, 4, 4, themeColor)
    end

    -- 헤드샷 피드백
    if headshotTimer > 0 then
        headshotTimer = headshotTimer - dt
        local hsSize = HEADSHOT_SIZE * 2  -- 2배
        HUD:DrawImage(
            UI_TEXTURES.HEADSHOT,
            centerX - hsSize / 2,
            centerY - hsSize / 2,
            hsSize,
            hsSize
        )
    end

    -- 일반 샷 피드백
    if shotTimer > 0 and headshotTimer <= 0 then
        shotTimer = shotTimer - dt
        local shotSize = SHOT_SIZE * 2  -- 2배
        HUD:DrawImage(
            UI_TEXTURES.SHOT,
            centerX - shotSize / 2,
            centerY - shotSize / 2,
            shotSize,
            shotSize
        )
    end
end

-- ===== HUD 상태 초기화 =====
function M.ResetHUD()
    hudState.displayedDistance = 0
    hudState.targetDistance = 0
    currentButtonIndex = 1
    lastDpadVertical = 0
end

-- ===== 게임 오버 화면 렌더링 =====
function M.UpdateGameOverScreen()
    if not HUD or not HUD:IsVisible() then return end

    local screenW = HUD:GetScreenWidth()
    local screenH = HUD:GetScreenHeight()
    local mouseX, mouseY = getMouseInHUDSpace()

    -- 반투명 검은 배경
    HUD:DrawRect(0, 0, screenW, screenH, Color(0, 0, 0, 0.7))

    -- "GAME OVER" 텍스트 (화면 상단)
    local gameOverText = "GAME OVER"
    local gameOverY = screenH * 0.15
    HUD:DrawText(gameOverText, screenW / 2 - 150, gameOverY, 72, Color(1, 0.2, 0.2, 1))

    -- 최종 점수 표시
    local distance = ScoreManager.GetDistance()
    local kills = ScoreManager.GetKillCount()
    local score = ScoreManager.GetScore()

    local statsY = screenH * 0.3
    local statsSpacing = 40
    HUD:DrawText("Distance: " .. ScoreManager.FormatDistance(distance), screenW / 2 - 100, statsY, 40, Color(1, 1, 1, 1))
    HUD:DrawText("Kills: " .. ScoreManager.FormatNumber(kills), screenW / 2 - 100, statsY + statsSpacing, 40, Color(1, 1, 1, 1))
    HUD:DrawText("Score: " .. ScoreManager.FormatNumber(score), screenW / 2 - 100, statsY + statsSpacing * 2, 40, Color(1, 1, 1, 1))

    -- 버튼 크기 설정
    local btnW = 400
    local btnH = 200
    local btnSpacing = -40
    local startY = screenH * 0.4
    local btnX = (screenW - btnW) / 2

    -- ===== 패드 십자키 네비게이션 =====
    if InputManager:IsGamepadConnected(0) then
        local dpadUp = InputManager:IsGamepadButtonPressed(GamepadButton.DPadUp, 0)
        local dpadDown = InputManager:IsGamepadButtonPressed(GamepadButton.DPadDown, 0)

        -- 버튼 전환 (방향 입력이 새로 들어왔을 때만)
        if dpadUp and lastDpadVertical <= 0 then
            currentButtonIndex = 1
            Audio.PlaySFX("buttonHovered")
        elseif dpadDown and lastDpadVertical >= 0 then
            currentButtonIndex = 2
            Audio.PlaySFX("buttonHovered")
        end

        lastDpadVertical = (dpadUp and 1) or (dpadDown and -1) or 0
    end

    -- RESTART 버튼
    local restartY = startY
    buttonStates.restart.rect = { x = btnX, y = restartY, w = btnW, h = btnH }
    local wasRestartHovered = buttonStates.restart.hovered

    -- 마우스 호버 또는 패드 선택
    if InputManager:IsGamepadConnected(0) then
        buttonStates.restart.hovered = (currentButtonIndex == 1)
    else
        buttonStates.restart.hovered = isPointInRect(mouseX, mouseY, buttonStates.restart.rect)
    end

    if buttonStates.restart.hovered and not wasRestartHovered then
        Audio.PlaySFX("buttonHovered")
    end

    -- RESTART 버튼 이미지
    local restartTex = buttonStates.restart.hovered and UI_TEXTURES.RESTART_BUTTON_HOVER or UI_TEXTURES.RESTART_BUTTON
    HUD:DrawImage(restartTex, btnX, restartY, btnW, btnH)

    -- EXIT 버튼
    local exitY = restartY + btnH + btnSpacing
    buttonStates.gameOverExit.rect = { x = btnX, y = exitY, w = btnW, h = btnH }
    local wasExitHovered = buttonStates.gameOverExit.hovered

    -- 마우스 호버 또는 패드 선택
    if InputManager:IsGamepadConnected(0) then
        buttonStates.gameOverExit.hovered = (currentButtonIndex == 2)
    else
        buttonStates.gameOverExit.hovered = isPointInRect(mouseX, mouseY, buttonStates.gameOverExit.rect)
    end

    if buttonStates.gameOverExit.hovered and not wasExitHovered then
        Audio.PlaySFX("buttonHovered")
    end

    -- EXIT 버튼 이미지
    local exitTex = buttonStates.gameOverExit.hovered and UI_TEXTURES.EXIT_BUTTON_HOVER or UI_TEXTURES.EXIT_BUTTON
    HUD:DrawImage(exitTex, btnX, exitY, btnW, btnH)

    -- 클릭 처리 (마우스 또는 패드)
    if InputManager:IsMouseButtonPressed(0) then
        if buttonStates.restart.hovered then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] RESTART clicked")
            GameState.ShowStartScreen()
        elseif buttonStates.gameOverExit.hovered then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] EXIT clicked")
            QuitGame()
        end
    end

    -- 패드 A 버튼으로 선택
    if InputManager:IsGamepadConnected(0) and InputManager:IsGamepadButtonPressed(GamepadButton.A, 0) then
        if currentButtonIndex == 1 then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] RESTART clicked (Gamepad)")
            GameState.ShowStartScreen()
        elseif currentButtonIndex == 2 then
            Audio.PlaySFX("buttonClicked")
            print("[UIManager] EXIT clicked (Gamepad)")
            QuitGame()
        end
    end
end

-- ===== 메인 업데이트 (상태에 따라 다른 UI 표시) =====
function M.Update(dt)
    local state = GameState.GetState()

    if state == GameState.States.START then
        M.UpdateStartScreen()
    elseif state == GameState.States.PLAYING then
        M.UpdateGameHUD(dt)
    elseif state == GameState.States.DEAD then
        M.UpdateGameOverScreen()
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

--- ADS 상태 설정 (줌 중일 때 크로스헤어 숨김)
function M.SetADS(bIsADS)
    isADS = bIsADS
end

return M
