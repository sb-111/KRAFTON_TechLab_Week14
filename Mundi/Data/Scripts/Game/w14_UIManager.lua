-- Game/w14_UIManager.lua
-- UI 표시 모듈 (Billboard 기반)

local GameState = require("Game/w14_GameStateManager")

local M = {}

-- 설정
M.Textures = {
    START = "Data/Textures/gamestart.png",
    END = "Data/Textures/gameover.png",
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
    GameState.OnStateChange(function(newState, oldState)
        M.OnStateChanged(newState)
    end)

    -- 현재 상태에 맞게 UI 표시
    M.OnStateChanged(GameState.GetState())

    print("[UIManager] Initialized (Found UIActor)")
    isInitialized = true
    return true
end

-- 상태 변경 시 호출
function M.OnStateChanged(state)
    if state == GameState.States.START then
        M.ShowTexture(M.Textures.START)
    elseif state == GameState.States.END then
        M.ShowTexture(M.Textures.END)
    else
        M.HideUI()
    end
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
    local camForward = camera:GetActorRight()

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

return M
