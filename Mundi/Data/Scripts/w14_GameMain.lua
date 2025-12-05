-- w14_GameMain.lua
-- 게임 메인 스크립트 (UI Actor에 붙임)
-- 필요: UBillboardComponent

local GameState = require("Game/w14_GameStateManager")
local UI = require("Game/w14_UIManager")

function BeginPlay()
    print("=== Game Main Start ===")

    -- UI 초기화 (UIActor를 자동으로 찾음)
    UI.Init()

    -- 시작 화면 표시
    GameState.ShowStartScreen()
end

function Tick(dt)
    -- UI 위치 업데이트 (카메라 앞에 유지)
    UI.Update()

    -- 입력 처리
    HandleInput()
end

function HandleInput()
    -- 스페이스바: 상태 전환
    if InputManager:IsKeyPressed(' ') then
        if GameState.IsStart() then
            GameState.StartGame()
        elseif GameState.IsEnd() then
            GameState.ShowStartScreen()
        end
    end

    -- ESC: 게임 중 종료
    if InputManager:IsKeyPressed('Q') then
        if GameState.IsPlaying() then
            GameState.EndGame()
        end
    end
end
