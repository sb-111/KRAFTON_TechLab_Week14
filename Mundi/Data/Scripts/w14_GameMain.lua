-- w14_GameMain.lua
-- 게임 메인 스크립트 (UI Actor에 붙임)
-- 필요: UBillboardComponent

local GameState = require("Game/w14_GameStateManager")
local UI = require("Game/w14_UIManager")
local Audio = require("Game/w14_AudioManager")
local MapConfig = require("w14_MapConfig")
local ControlManager = require("w14_ControlManager")
local MapManagerClass = require("w14_MapManager")
local GeneralObjectManagerClass = require("w14_GeneralObjectManager")
local ObjectPlacerClass = require("w14_ObjectPlacer")
local ObstacleConfig = require("w14_ObstacleConfig")
local PlayerSlowClass = require("Player/w14_PlayerSlow")

local MapManager = nil
local ObstacleManager = nil
local ItemManager = nil
local ObjectPlacer = nil
local PlayerSlow = nil

function BeginPlay()
    print("=== Game Main Start ===")

    -- 디버그: GameMain에서 직접 FindObjectByName 테스트
    local testActor = FindObjectByName("BGMActor")
    if testActor then
        print("[DEBUG] GameMain: BGMActor found directly!")
    else
        print("[DEBUG] GameMain: BGMActor NOT found directly!")
    end

    -- UI 초기화 (UIActor를 자동으로 찾음)
    UI.Init()
--    GameState.OnStateChange(function()
--      print("콜백 테스트")
--    end)

    -- 오디오 초기화
    -- 씬에 필요한 Actor:
    --   BGMActor (AudioComponent 1개 + Sounds[0]에 BGM)
    --   ShotActor (AudioComponent 여러 개 + Sounds[0]에 효과음)
    Audio.Init()
    Audio.RegisterBGM("BGM", "BGMActor")      -- Looping 자동 설정
    Audio.RegisterSFX("Shot", "ShotSFXActor")    -- AutoPlay 자동 비활성화

    -- 시작 화면 표시
    GameState.ShowStartScreen()

    -- PlayerSlow 모듈 초기화
    PlayerSlow = PlayerSlowClass:new(Obj)

    -- ControlManager에 플레이어 등록
    ControlManager:set_player_to_trace(Obj)

    MapManager = MapManagerClass:new()
    
    -- MapManager에 플레이어 등록
    MapManager:set_player_to_trace(Obj)
    -- MapManager 초기화
    MapManager:add_biom(
            "Data/Prefabs/test_map_chunk_0.prefab",
            100,
            Vector(-1000, 0, 0)
    )
    MapManager:add_biom(
            "Data/Prefabs/test_map_chunk_1.prefab",
            100,
            Vector(-1000, 0, 0)
    )
    MapManager:add_biom(
            "Data/Prefabs/test_map_chunk_2.prefab",
            100,
            Vector(-1000, 0, 0)
    )
    
    -- ObjectPlacer 인스턴스 생성
    --- 초기 한정으로 ObjectPlacer의 소환 위치를 플레이어로부터 떨어지게 둔다.
    ObjectPlacer = ObjectPlacerClass:new(
            MapConfig.map_chunk_y_size,
            MapConfig.map_chunk_x_size * 0.5,
            MapConfig.map_chunk_x_size * 1.5,
            Obj.Location.Y,
            1000
    )

    -- ObstacleManager 생성 (GeneralObjectManager 사용)
    ObstacleManager = GeneralObjectManagerClass:new(ObjectPlacer)
    ObstacleManager:set_player_to_trace(Obj)
    ObstacleManager:add_object(
            "Data/Prefabs/Obstacle_Tree.prefab",
            500,                    -- pool_size
            Vector(-2000, 0, 0),    -- pool_standby_location
            3,                     -- spawn_num
            5                       -- radius
    )

    -- ItemManager 생성 (GeneralObjectManager 사용)
    ItemManager = GeneralObjectManagerClass:new(ObjectPlacer)
    ItemManager:set_player_to_trace(Obj)
    ItemManager:add_object(
            "Data/Prefabs/test_item.prefab",
            300,                    -- pool_size
            Vector(-2000, 100, 0),  -- pool_standby_location
            5,                      -- spawn_num (적게)
            3                       -- radius
    )

end

function Tick(dt)
    -- UI 위치 업데이트 (카메라 앞에 유지)
    UI.Update()

    -- 입력 처리
    HandleInput()

    -- PlayerSlow 속도 배율을 ControlManager에 동기화
    if PlayerSlow then
        ControlManager:SetSpeedMultiplier(PlayerSlow:GetSpeedMultiplier())
    end

    -- 플레이어 조작
    ControlManager:Control(dt)

    -- 맵 업데이트
    if MapManager then
        MapManager:Tick()
    end

    if ObjectPlacer and Obj.Location.X >= ObjectPlacer.area_height_offset then
        ObjectPlacer:update_area(
                MapConfig.map_chunk_y_size,                              -- area_width (Y축)
                MapConfig.map_chunk_x_size,                              -- area_height (X축)
                Obj.Location.Y,                                          -- area_width_offset (Y축)
                Obj.Location.X + MapConfig.map_chunk_x_size * 2.5  -- area_height_offset (X축)
        )
    end

    if ObstacleManager then
        ObstacleManager:Tick()
    end

    if ItemManager then
        ItemManager:Tick()
    end
end

function HandleInput()
    -- 스페이스바: 상태 전환
    if InputManager:IsKeyPressed(' ') then
        if GameState.IsStart() then
            GameState.StartGame()
            Audio.PlayBGM("BGM")  -- 게임 시작 시 BGM 재생
        elseif GameState.IsEnd() then
            GameState.ShowStartScreen()
        end
    end

    -- ESC: 게임 중 종료
    if InputManager:IsKeyPressed('Q') then
        if GameState.IsPlaying() then
            GameState.EndGame()
            Audio.StopBGM("BGM")  -- 게임 종료 시 BGM 정지
        end
    end
end

-- 충돌 처리: 장애물과 충돌 시 속도 감소
function OnBeginOverlap(OtherActor)
    print("[OnBeginOverlap] test0")
    if not OtherActor then print("[OnBeginOverlap] test") return end

    -- 디버그: 충돌한 Actor 정보 출력
    print("[OnBeginOverlap] OnBeginOverlap - Name: " .. tostring(OtherActor.Name) .. ", Tag: " .. tostring(OtherActor.Tag))

    -- 장애물 충돌 처리
    if OtherActor.Tag == "obstacle" and PlayerSlow then
        local obstacleName = OtherActor.Name
        local cfg = ObstacleConfig[obstacleName] or ObstacleConfig.Default

        print("[OnBeginOverlap] 장애물 충돌: " .. obstacleName .. " (speedMult:" .. cfg.speedMult .. ", duration:" .. cfg.duration .. ")")

        -- PlayerSlow 모듈로 속도 감소 적용 (카메라 셰이크 + 자동 복구 포함)
        PlayerSlow:ApplySlow(cfg.speedMult, cfg.duration)
    end
end

function OnEndOverlap(OtherActor)
    -- 필요시 추가
end
