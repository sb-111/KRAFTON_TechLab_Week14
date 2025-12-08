-- Game/w14_GameStateManager.lua
-- 게임 상태 관리 모듈

local M = {}

-- 상태 정의
M.States = {
    INIT = "Init",
    START = "Start",      -- 시작 화면
    PLAYING = "Playing",  -- 게임 중
    PAUSED = "Paused",    -- 일시정지
    DEAD = "Dead",        -- 게임 오버 (플레이어 사망 또는 게임 종료)
}

-- 현재 상태
local currentState = M.States.INIT
local previousState = M.States.INIT

-- 상태별 콜백 저장소
-- { ["Playing"] = {func1, func2}, ["Dead"] = {func3} }
local stateCallbacks = {}

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

    local callbacks = stateCallbacks[newState]
    if callbacks then
        for _, callback in ipairs(callbacks) do
            callback(previousState)
        end
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

function M.IsDead()
    return currentState == M.States.DEAD
end

-- 하위 호환성을 위해 IsEnd()는 IsDead()와 동일하게 동작
function M.IsEnd()
    return currentState == M.States.DEAD
end

-- 사용법: GameState.OnStateChange(GameState.States.DEAD, function(prev) ... end)
function M.OnStateChange(targetState, callback)
    -- 해당 상태에 대한 콜백 리스트가 없으면 생성
    if not stateCallbacks[targetState] then
        stateCallbacks[targetState] = {}
    end
    
    table.insert(stateCallbacks[targetState], callback)
end

-- 편의 함수들
function M.ShowStartScreen()
    M.SetState(M.States.START)
end

function M.StartGame()
    M.SetState(M.States.PLAYING)
end

function M.PlayerDied()
    M.SetState(M.States.DEAD)
end

-- 하위 호환성을 위해 EndGame()은 PlayerDied()와 동일하게 동작
function M.EndGame()
    M.SetState(M.States.DEAD)
end

function M.Reset()
    currentState = M.States.INIT
    previousState = M.States.INIT
    -- 리셋 시 콜백을 비울지 여부는 선택사항 (여기선 유지)
end

return M