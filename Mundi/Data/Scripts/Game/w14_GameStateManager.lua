-- Game/w14_GameStateManager.lua
-- 게임 상태 관리 모듈

local M = {}

-- 상태 정의
M.States = {
    INIT = "Init",
    START = "Start",      -- 시작 화면
    PLAYING = "Playing",  -- 게임 중
    PAUSED = "Paused",    -- 일시정지
    DEAD = "Dead",        -- 플레이어 사망
    END = "End"           -- 종료 화면
}

-- 현재 상태
local currentState = M.States.INIT
local previousState = M.States.INIT

-- 상태 변경 콜백들
local onStateChangeCallbacks = {}

-- 현재 상태 반환
function M.GetState()
    return currentState
end

-- 이전 상태 반환
function M.GetPreviousState()
    return previousState
end

-- 상태 변경
function M.SetState(newState)
    if currentState == newState then
        return false
    end

    previousState = currentState
    currentState = newState

    print("[GameState] " .. previousState .. " -> " .. newState)

    -- 콜백 호출
    for _, callback in ipairs(onStateChangeCallbacks) do
        callback(newState, previousState)
    end

    return true
end

-- 상태 체크 헬퍼
function M.IsState(state)
    return currentState == state
end

function M.IsPlaying()
    return currentState == M.States.PLAYING
end

function M.IsStart()
    return currentState == M.States.START
end

function M.IsEnd()
    return currentState == M.States.END
end

-- 상태 변경 콜백 등록
function M.OnStateChange(callback)
    table.insert(onStateChangeCallbacks, callback)
end

-- 편의 함수들
function M.ShowStartScreen()
    -- 시작 화면으로 이동
    M.SetState(M.States.START)
end

function M.StartGame()
    -- 플레이 모드로 변경
    M.SetState(M.States.PLAYING)
end

function M.PlayerDied()
    -- 플레이어 사망 상태로 변경
    M.SetState(M.States.DEAD)
end

function M.EndGame()
    -- 종료 화면으로 이동
    M.SetState(M.States.END)
end

function M.Reset()
    -- 초기 상태로 변경
    currentState = M.States.INIT
    previousState = M.States.INIT
end

return M
