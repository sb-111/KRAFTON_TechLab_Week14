-- Game/w14_UIManager.lua
-- UI 표시 모듈 (Billboard 기반)

local GameState = require("Game/w14_GameStateManager")
local ScoreManager = require("Game/w14_ScoreManager")
local AmmoManager = require("Game/w14_AmmoManager")

local M = {}

-- 설정
M.Textures = {
    START = "Data/Textures/w14_gamestart.png",
    END = "Data/Textures/w14_gameover.png",
    NONE = ""
}

-- 내부 상태
local billboardComp = nil
local uiObject = nil
local uiDistance = 5  -- 카메라로부터 거리 (테스트용으로 늘림)
local isInitialized = false
local initAttempts = 0
local maxInitAttempts = 60  -- 약 1초 (60fps 기준)

-- UI Actor 이름 (씬에서 이 이름의 Actor에 Billboard가 있어야 함)
M.UI_ACTOR_NAME = "UIActor"

-- 초기화 (자동으로 호출됨, 수동 호출도 가능)
function M.Init()
    if isInitialized then return true end

    initAttempts = initAttempts + 1

    -- 이름으로 UI Actor 찾기
    uiObject = FindObjectByName(M.UI_ACTOR_NAME)

    if not uiObject then
        -- 일정 시간 후에도 못 찾으면 경고 (한 번만)
        if initAttempts == maxInitAttempts then
            print("[UIManager] WARNING: UIActor not found! (Name: " .. M.UI_ACTOR_NAME .. ")")
            print("[UIManager] UI will not be displayed.")
        end
        return false
    end

    billboardComp = GetComponent(uiObject, "UBillboardComponent")

    if not billboardComp then
        print("[UIManager] UBillboardComponent not found on UIActor!")
        return false
    end

    -- 상태 변경 콜백 등록
    GameState.OnStateChange(GameState.States.START, function() 
        M.ShowTexture(M.Textures.START) 
    end)
    GameState.OnStateChange(GameState.States.END, function() 
        M.ShowTexture(M.Textures.END) 
    end)
    GameState.OnStateChange(GameState.States.INIT, function() M.HideUI() end)
    GameState.OnStateChange(GameState.States.PLAYING, function() M.HideUI() end)
    GameState.OnStateChange(GameState.States.PAUSED, function() M.HideUI() end)
    GameState.OnStateChange(GameState.States.DEAD, function() M.HideUI() end)

    print("[UIManager] Initialized (Found UIActor)")
    isInitialized = true
    return true
end

-- 텍스처 표시
function M.ShowTexture(texturePath)
    if billboardComp then
        billboardComp:SetTexture(texturePath)
        print("[UIManager] Show: " .. texturePath)
    end
end

-- UI 숨기기
function M.HideUI()
    if billboardComp then
        billboardComp:SetTexture("")
    end
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

-- 툴바 높이 (픽셀) - 툴바 아래에 HUD 표시
local TOOLBAR_HEIGHT = 100

-- 탄환 아이콘 경로
local AMMO_ICON_PATH = "Data/Textures/Ammo.png"

-- 크로스헤어 설정
local CROSSHAIR_PATH = "Data/Textures/CrossHair.png"
local CROSSHAIR_SIZE = 40  -- 크로스헤어 크기 (픽셀)

--- HUD 프레임 시작 (매 프레임 Tick 시작 시 호출)
function M.BeginHUDFrame()
    if HUD then
        HUD:BeginFrame()
    end
end

--- 게임 HUD 표시 (ScoreManager, AmmoManager에서 직접 값을 가져옴)
--- @param dt number 델타 타임 (초)
function M.UpdateGameHUD(dt)
    if not HUD or not HUD:IsVisible() then return end

    dt = dt or 0.016  -- 기본값 60fps

    -- ScoreManager에서 값 가져오기
    local distance = ScoreManager.GetDistance()
    local kills = ScoreManager.GetKillCount()
    local score = ScoreManager.GetScore()
    local ammo = AmmoManager.GetCurrentAmmo()

    -- 슬롯머신 애니메이션: 표시 거리를 실제 거리로 서서히 증가
    hudState.targetDistance = distance
    if hudState.displayedDistance < hudState.targetDistance then
        -- 남은 거리에 비례해서 속도 조절 (가까울수록 느리게)
        local diff = hudState.targetDistance - hudState.displayedDistance
        local speed = math.max(hudState.animationSpeed, diff * 2)  -- 최소 50, 차이의 2배
        hudState.displayedDistance = math.min(
            hudState.displayedDistance + speed * dt,
            hudState.targetDistance
        )
    end

    -- 거리 표시 (좌측 상단, 툴바 아래)
    HUD:DrawText(
        ScoreManager.FormatDistance(hudState.displayedDistance),
        20, TOOLBAR_HEIGHT,                  -- 툴바 아래 위치
        36,                                  -- 폰트 크기
        Color(1, 1, 1, 1)                    -- 흰색
    )

    -- 킬수 표시 (거리 아래)
    HUD:DrawText(
        "Kills: " .. ScoreManager.FormatNumber(kills),
        20, TOOLBAR_HEIGHT + 50,
        28,
        Color(1, 1, 1, 1)                    -- 흰색
    )

    -- 점수 표시 (킬수 아래)
    HUD:DrawText(
        "Score: " .. ScoreManager.FormatNumber(score),
        20, TOOLBAR_HEIGHT + 85,
        28,
        Color(1, 1, 1, 1)                    -- 흰색
    )

    -- 탄약 수 표시 (화면 정중앙 상단) - 탄창/예비탄약 형식 (30|120)
    local totalAmmo = AmmoManager.GetTotalAmmo()
    local ammoText = ammo .. " | " .. totalAmmo
    HUD:DrawText(
        ammoText,
        570, TOOLBAR_HEIGHT,                 -- 화면 중앙
        36,                                  -- 거리와 같은 크기
        Color(1, 1, 1, 1)                    -- 흰색
    )

    -- 탄약 아이콘 (탄약 수 오른쪽)
    HUD:DrawImage(
        AMMO_ICON_PATH,
        710, TOOLBAR_HEIGHT + 10,             -- 숫자 오른쪽 (30 | 120 형식에 맞춤)
        40, 40                               -- 아이콘 크기
    )

    -- 크로스헤어 (화면 정중앙)
    -- 상대 좌표 사용: 0.5, 0.5가 중앙, 크기도 상대값으로
    local crosshairRelSize = 0.025  -- 화면 대비 2.5% 크기
    HUD:DrawImageRel(
        CROSSHAIR_PATH,
        0.5 - crosshairRelSize / 2,   -- X: 중앙에서 반 크기만큼 왼쪽
        0.5 - crosshairRelSize / 2,   -- Y: 중앙에서 반 크기만큼 위
        crosshairRelSize,              -- 너비
        crosshairRelSize               -- 높이
    )
end

--- HUD 상태 초기화 (게임 시작 시 호출)
function M.ResetHUD()
    hudState.displayedDistance = 0
    hudState.targetDistance = 0
end

--- 간단한 텍스트 표시 (절대 좌표)
--- @param text string 표시할 텍스트
--- @param x number X 좌표 (픽셀)
--- @param y number Y 좌표 (픽셀)
--- @param size number 폰트 크기 (기본 24)
--- @param r number 빨강 (0~1, 기본 1)
--- @param g number 초록 (0~1, 기본 1)
--- @param b number 파랑 (0~1, 기본 1)
function M.DrawText(text, x, y, size, r, g, b)
    if not HUD then return end
    size = size or 24
    r = r or 1
    g = g or 1
    b = b or 1
    HUD:DrawText(text, x, y, size, Color(r, g, b, 1))
end

--- 간단한 텍스트 표시 (상대 좌표)
--- @param text string 표시할 텍스트
--- @param rx number X 좌표 (0~1)
--- @param ry number Y 좌표 (0~1)
--- @param size number 폰트 크기 (기본 24)
--- @param r number 빨강 (0~1, 기본 1)
--- @param g number 초록 (0~1, 기본 1)
--- @param b number 파랑 (0~1, 기본 1)
function M.DrawTextRel(text, rx, ry, size, r, g, b)
    if not HUD then return end
    size = size or 24
    r = r or 1
    g = g or 1
    b = b or 1
    HUD:DrawTextRel(text, rx, ry, size, Color(r, g, b, 1))
end

--- HUD 표시/숨김 토글
function M.ToggleHUD()
    if HUD then
        HUD:SetVisible(not HUD:IsVisible())
    end
end

--- HUD 화면 크기 설정 (해상도 변경 시 호출)
function M.SetHUDScreenSize(width, height)
    if HUD then
        HUD:SetScreenSize(width, height)
    end
end

return M
