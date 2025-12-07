-- w14_GameMain.lua
-- 게임 메인 스크립트 (UI Actor에 붙임)
-- 필요: UBillboardComponent

local GameState = require("Game/w14_GameStateManager")
local UI = require("Game/w14_UIManager")
local Audio = require("Game/w14_AudioManager")
local MapConfig = require("w14_MapConfig")
local MapManagerClass = require("w14_MapManager")
local GeneralObjectManagerClass = require("w14_GeneralObjectManager")
local ObjectPlacerClass = require("w14_ObjectPlacer")
local BiomeManagerClass = require("w14_BiomeManager")

local MapManager = nil
local BiomeManager = nil
local ItemManager = nil
local MonsterManager = nil
local ObjectPlacer = nil
local Player = nil

--- 게임 정리 함수 (재시작 전에 호출)
local function CleanupGame()
    -- 매니저들 정리
    if MapManager and MapManager.destroy then
        MapManager:destroy()
        MapManager = nil
    end

    if BiomeManager and BiomeManager.destroy then
        BiomeManager:destroy()
        BiomeManager = nil
    end

    if ItemManager and ItemManager.destroy then
        ItemManager:destroy()
        ItemManager = nil
    end

    -- 플레이어 삭제
    if Player and DeleteObject then
        DeleteObject(Player)
        Player = nil
    end

    ObjectPlacer = nil
end

function BeginPlay()
    print("=== Game Main Start ===")

    -- UI 초기화 (UIActor를 자동으로 찾음)
    UI.Init()

    -- 오디오 초기화
    -- 씬에 필요한 Actor:
    --   BGMActor (AudioComponent 1개 + Sounds[0]에 BGM)
    --   ShotActor (AudioComponent 여러 개 + Sounds[0]에 효과음)
    Audio.Init()
    Audio.RegisterBGM("BGM", "BGMActor")      -- Looping 자동 설정
    Audio.RegisterSFX("Shot", "ShotSFXActor")    -- AutoPlay 자동 비활성화

    -- 시작 화면 표시
    GameState.OnStateChange(GameState.States.PLAYING, GameStart)
    GameState.OnStateChange(GameState.States.END, GameEnd)
    GameState.ShowStartScreen()
end

function GameStart()
    -- 기존 게임 정리 (재시작 시)
    CleanupGame()

    -- 플레이어 생성
    Player = SpawnPrefab("Data/Prefabs/w14_Player.prefab")
    Player.Location = Vector(0, 0, 1.3)

    -- MapManager 초기화
    MapManager = MapManagerClass:new(Player)
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

    -- BiomeManager 생성 (바이옴별 장애물 관리)
    -- 바이옴 설정은 w14_BiomeConfig.lua에서 관리
    BiomeManager = BiomeManagerClass:new(
            Player,
            ObjectPlacer,
            MapConfig.map_chunk_x_size * 10  -- 바이옴 전환 간격
    )

    -- ItemManager 생성 (GeneralObjectManager 사용, 바이옴과 무관)
    ItemManager = GeneralObjectManagerClass:new(ObjectPlacer, Player)
    ItemManager:add_object(
            "Data/Prefabs/test_item.prefab",
            300,                    -- pool_size
            Vector(-2000, 100, 0),  -- pool_standby_location
            5,                      -- spawn_num (적게)
            3                       -- radius
    )
    
    MonsterManager = GeneralObjectManagerClass:new(ObjectPlacer, Player)
    MonsterManager:add_object(
            "Data/Prefabs/w14_BasicMonster.prefab",
            300,
            Vector(10, 100, 0),  -- pool_standby_location
            10,                     -- spawn_num (적게)
            2                       -- radius
    )
end

function GameEnd()
    -- 게임 종료 시 정리는 다음 게임 시작 시 CleanupGame에서 처리
    print("=== Game End ===")
end

function Tick(dt)
    -- UI 위치 업데이트 (카메라 앞에 유지)
    UI.Update()

    -- 입력 처리
    HandleInput()

    if GameState.IsPlaying() then
        -- 맵 업데이트
        if MapManager then
            MapManager:Tick()
        end

        if ObjectPlacer and Player and Player.Location.X >= ObjectPlacer.area_height_offset then
            ObjectPlacer:update_area(
                    MapConfig.map_chunk_y_size,                              -- area_width (Y축)
                    MapConfig.map_chunk_x_size,                              -- area_height (X축)
                    Player.Location.Y,                                          -- area_width_offset (Y축)
                    Player.Location.X + MapConfig.map_chunk_x_size * 2.5  -- area_height_offset (X축)
            )
        end

        -- BiomeManager가 바이옴별 장애물 관리
        if BiomeManager then
            BiomeManager:Tick()
        end

        if ItemManager then
            ItemManager:Tick()
        end

        if MonsterManager then
            MonsterManager:Tick()
        end
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
