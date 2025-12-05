-- w14_GameStateTest.lua
-- GameStateManager 모듈 테스트
-- Actor에 붙여서 실행

local GameState = require("Game/w14_GameStateManager")

function BeginPlay()
    print("=== GameStateManager 테스트 ===")

    -- 초기 상태
    print("현재 상태: " .. GameState.GetState())

    -- 상태 변경 콜백 등록
    GameState.OnStateChange(function(newState, oldState)
        print("[콜백] 상태 변경 감지: " .. oldState .. " -> " .. newState)
    end)

    -- 시작 화면으로
    GameState.ShowStartScreen()
    print("IsStart: " .. tostring(GameState.IsStart()))
end

function Tick(dt)
    -- 스페이스바: 상태 전환
    if InputManager:IsKeyPressed(' ') then
        if GameState.IsStart() then
            GameState.StartGame()
        elseif GameState.IsPlaying() then
            GameState.PlayerDied()
        elseif GameState.IsState(GameState.States.DEAD) then
            GameState.EndGame()
        elseif GameState.IsEnd() then
            GameState.ShowStartScreen()  -- End -> Start 직접 전환
        end
    end
end
