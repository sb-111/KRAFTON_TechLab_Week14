-- w14_GameMain.lua
-- 게임 메인 스크립트 (UI Actor에 붙임)
-- 필요: UBillboardComponent

local GameState = require("Game/w14_GameStateManager")
local UI = require("Game/w14_UIManager")
local ControlManager = require("w14_ControlManager")
local MapPool = require("w14_MapPool")

function BeginPlay()
    print("=== Game Main Start ===")

    -- UI 초기화 (UIActor를 자동으로 찾음)
    UI.Init()
--    GameState.OnStateChange(function()
--      print("콜백 테스트")
--    end)

    -- 시작 화면 표시
    GameState.ShowStartScreen()

    -- ControlManager에 플레이어 등록
    ControlManager:set_player_to_trace(Obj)
    
    -- MapPool에 플레이어 등록
    MapPool:set_player_to_trace(Obj)
    -- MapPool 초기화
    MapPool:add_biom(
            "Data/Prefabs/test_map_chunk_0.prefab",
            100,
            Vector(-1000, 0, 0)
    )
    MapPool:add_biom(
            "Data/Prefabs/test_map_chunk_1.prefab",
            100,
            Vector(-1000, 0, 0)
    )
    MapPool:add_biom(
            "Data/Prefabs/test_map_chunk_2.prefab",
            100,
            Vector(-1000, 0, 0)
    )
end

function Tick(dt)
    -- UI 위치 업데이트 (카메라 앞에 유지)
    UI.Update()

    -- 입력 처리
    HandleInput()
    
    -- 플레이어 조작
    ControlManager:Control(dt)
    
    -- 맵 업데이트
    MapPool:Tick()
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
