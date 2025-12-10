-- w14_GameMain.lua
-- 게임 메인 스크립트 (UI Actor에 붙임)
-- 필요: UBillboardComponent

local GameState = require("Game/w14_GameStateManager")
local UI = require("Game/w14_UIManager")
local Audio = require("Game/w14_AudioManager")
local ScoreManager = require("Game/w14_ScoreManager")
local AmmoManager = require("Game/w14_AmmoManager")
local HPManager = require("Game/w14_HPManager")
local MapConfig = require("w14_MapConfig")
local MapManagerClass = require("w14_MapManager")
local GeneralObjectManagerClass = require("w14_GeneralObjectManager")
local ObjectPlacerClass = require("w14_ObjectPlacer")
local StageManagerClass = require("w14_StageManager")
local DifficultyManagerClass = require("w14_DifficultyManager")
local BossMonsterManagerClass = require("Monster/w14_BossMonsterManager")

local MapManager = nil
local GameStageManager = nil  -- StageManager.instance로 외부 접근 가능
local ItemManager = nil
local MonsterManager = nil
local ObjectPlacer = nil
local Player = nil
local PlayerScript = nil
local StartUICam = nil
local BasicMonsterDifficultyManager = nil
local ChaserMonsterDifficultyManager = nil
local BoomerMonsterDifficultyManager = nil
local BossMonsterManager = nil
local IntroCamera = nil

--- 게임 정리 함수 (재시작 전에 호출)
local function CleanupGame()
    -- 매니저들 정리
    if MapManager and MapManager.reset then
        MapManager:reset()
    end

    if StageManager and StageManager.reset then
        StageManager:reset()
    end

    if ItemManager and ItemManager.reset then
        ItemManager:reset()
    end

    if MonsterManager and MonsterManager.reset then
        MonsterManager:reset()
    end

    if BasicMonsterDifficultyManager then
        BasicMonsterDifficultyManager:reset()
    end
    
    if ChaserMonsterDifficultyManager then
        ChaserMonsterDifficultyManager:reset()
    end

    if BoomerMonsterDifficultyManager then
        BoomerMonsterDifficultyManager:reset()
    end

    if BossMonsterManager then
        BossMonsterManager:reset()
    end

    
    if GameStageManager then
        GameStageManager:reset()
    end
    
    PlayerScript.Reset()
end

function BeginPlay()
    print("=== Game Main Start ===")

    StartUICam = GetCameraManager():GetCamera()
    IntroCamera = GetComponent(Obj, "UCameraComponent")

    GetCameraManager():SetViewTarget(IntroCamera)

    -- 플레이어 생성
    Player = SpawnPrefab("Data/Prefabs/w14_Player.prefab")
    PlayerScript = Player:GetScript()
    PlayerScript:Reset()

    -- MapManager 초기화
    MapManager = MapManagerClass:new(Player)
    
    MapManager:add_biom(
            "Data/Prefabs/w14_chunk_30_1x8_1.prefab",
            10,
            Vector(-1000, 0, 0)
    )
    MapManager:add_biom(
            "Data/Prefabs/w14_chunk_30_1x8_2.prefab",
            10,
            Vector(-1000, 0, 0)
    )
    MapManager:add_biom(
            "Data/Prefabs/w14_chunk_30_1x8_3.prefab",
            10,
            Vector(-1000, 0, 0)
    )
    MapManager:add_biom(
            "Data/Prefabs/w14_chunk_30_1x8_4.prefab",
            10,
            Vector(-1000, 0, 0)
    )
    MapManager:add_biom(
            "Data/Prefabs/w14_chunk_30_1x8_5.prefab",
            10,
            Vector(-1000, 0, 0)
    )
    MapManager:add_biom(
            "Data/Prefabs/w14_chunk_30_1x8_6.prefab",
            10,
            Vector(-1000, 0, 0)
    )
    MapManager:add_biom(
            "Data/Prefabs/w14_chunk_30_1x8_7.prefab",
            10,
            Vector(-1000, 0, 0)
    )
    MapManager:add_biom(
            "Data/Prefabs/w14_chunk_30_1x8_8.prefab",
            10,
            Vector(-1000, 0, 0)
    )

    -- ObjectPlacer 인스턴스 생성
    --- 초기 한정으로 ObjectPlacer의 소환 위치를 플레이어로부터 떨어지게 둔다.
    ObjectPlacer = ObjectPlacerClass:new(
            MapConfig.map_chunk_y_size * 0.5,
            MapConfig.map_chunk_x_size,
            MapConfig.map_chunk_x_size * 1.5,
            Obj.Location.Y,
            1000
    )

    -- StageManager 생성 (스테이지별 장애물 관리)
    -- 스테이지 설정은 w14_StageConfig.lua에서 관리
    GameStageManager = StageManagerClass:new(Player, ObjectPlacer)

    -- ItemManager 생성 (GeneralObjectManager 사용, 스테이지와 무관)
    ItemManager = GeneralObjectManagerClass:new(ObjectPlacer, Player)
    ItemManager:add_object(
            "Data/Prefabs/w14_AmmoItem.prefab",
            5,                    -- pool_size
            Vector(-2000, 100, 0),  -- pool_standby_location
            1,                      -- spawn_num (적게)
            3,                      -- radius
            0.8                     -- 물체 스폰 z 위치
    )

    ItemManager:add_object(
            "Data/Prefabs/w14_AidKit.prefab",
            5,                    -- pool_size
            Vector(-2000, 100, 0),  -- pool_standby_location
            1,                      -- spawn_num (적게)
            3,                      -- radius
            0.8                     -- 물체 스폰 z 위치
    )

    ItemManager:add_object(
            "Data/Prefabs/w14_Adrenalin.prefab",
            5,                    -- pool_size
            Vector(-2000, 100, 0),  -- pool_standby_location
            1,                      -- spawn_num (적게)
            3,                      -- radius
            0.8                     -- 물체 스폰 z 위치
    )

    ItemManager:add_object(
            "Data/Prefabs/w14_AirstrikeItem.prefab",
            5,                      -- pool_size (희귀 아이템)
            Vector(-2000, 100, 0),  -- pool_standby_location
            1,                      -- spawn_num (매우 적게)
            3,                      -- radius
            0.8                     -- 물체 스폰 z 위치
    )

    MonsterManager = GeneralObjectManagerClass:new(ObjectPlacer, Player)
    MonsterManager:add_object(
            "Data/Prefabs/w14_BasicMonster.prefab",
            40,
            Vector(-2000, 60, 0),  -- pool_standby_location
            10,                     -- spawn_num (적게)
            2,                      -- radius
            0.25                     -- 물체 스폰 z 위치
    )
    MonsterManager:add_object(
            "Data/Prefabs/w14_ChaserMonster.prefab",
            30,
            Vector(-2000, 40, 0),  -- pool_standby_location
            5,                      -- spawn_num (기본보다 적게)
            2.5,                    -- radius
            0.25                     -- 물체 스폰 z 위치
    )
    MonsterManager:add_object(
            "Data/Prefabs/w14_BoomerMonster.prefab",
            15,
            Vector(-2000, 20, 0),  -- pool_standby_location
            2,                      -- spawn_num (희귀)
            3,                      -- radius
            0.25                    -- 물체 스폰 z 위치
    )

    BasicMonsterDifficultyManager = DifficultyManagerClass:new(10, 30, 60)
    ChaserMonsterDifficultyManager = DifficultyManagerClass:new(5, 24, 60)
    BoomerMonsterDifficultyManager = DifficultyManagerClass:new(2, 10, 90)

    -- BossMonsterManager 생성
    -- 파라미터: 스폰시간(min, max, interval), HP(min, max, interval), 데미지(min, max, interval), 발사체개수(min, max, interval)
    BossMonsterManager = BossMonsterManagerClass:new(
            20, 15, 120,    -- 스폰 시간: 30초에서 15초까지, 120초 동안 감소
            100, 300, 180,  -- HP: 100에서 300까지, 180초 동안 증가
            10, 30, 180,    -- 데미지: 10에서 30까지, 180초 동안 증가
            1, 5, 120       -- 발사체 개수: 1개에서 5개까지, 120초 동안 증가
    )

    -- UI 초기화 (UIActor를 자동으로 찾음)
    UI.Init()

    -- 오디오 초기화
    -- 씬에 필요한 Actor:
    --   BGMActor (AudioComponent 1개 + Sounds[0]에 BGM)
    --   ShotActor (AudioComponent 여러 개 + Sounds[0]에 효과음)
    Audio.Init()
    Audio.RegisterBGM("BGM", "BGMActor")      -- Looping 자동 설정
    Audio.RegisterSFX("Shot", "ShotSFXActor")    -- AutoPlay 자동 비활성화

    -- 상태 변경 콜백 등록
    GameState.OnStateChange(GameState.States.START, GameReset)
    GameState.OnStateChange(GameState.States.PLAYING, GameStart)
    GameState.OnStateChange(GameState.States.DEAD, function()
        InputManager:SetCursorVisible(true)  -- 게임 오버 화면에서 커서 표시
        Audio.StopBGM("BGM")
        print("[GameMain] Game Over")
    end)

    -- 플레이어 사망 시 게임 오버 상태로 전환
    HPManager.OnDeath(function()
        GameState.PlayerDied()
    end)

    StartCoroutine(CinematicToStart)
end

function CinematicToStart()
    coroutine.yield("wait_time", 7)
    GetCameraManager():StartFade(0.5, 0, 1, Color(0, 0, 0, 1), 0)
    coroutine.yield("wait_time", 0.5)

    -- 0.5초 FadeOut
    GameReset()
    
    --0.5초 FadeIn
    GetCameraManager():StartFade(0.5, 1, 0, Color(0, 0, 0, 1), 0)
    coroutine.yield("wait_time", 0.5)
end

function GameReset()    
    EnableDepthOfField()
    InputManager:SetCursorVisible(true)
    -- 기존 게임 정리 (재시작 시)
    CleanupGame()
    
    GetCameraManager():SetViewTarget(StartUICam)
    
    -- HUD 상태 초기화 (슬롯머신 애니메이션 리셋)
    UI.ResetHUD()

    -- 점수/탄약/HP 매니저 초기화
    ScoreManager.Reset()
    AmmoManager.Reset()
    HPManager.Reset()

    -- 시작 화면 표시
    GameState.ShowStartScreen()
end

function GameStart()
    DisableDepthOfField()
    -- 플레이어에게 시작 알림
    PlayerScript.StartGame()

    Audio.Reset()
    Audio.RegisterDefaults() -- 기본 오디오 등록
    Audio.PlayBGM("BGM")  -- 게임 시작 시 BGM 재생
end

function Tick(dt)
    -- HUD 프레임 시작 (D2D 렌더링 준비)
    UI.BeginHUDFrame()

    -- 입력 처리
    HandleInput()

    -- 거리 업데이트 (ScoreManager가 관리)
    if GameState.IsPlaying() and Player then
        ScoreManager.SetDistance(Player.Location.X)
    end

    -- UI 업데이트 (모든 상태에서 호출되어야 함)
    UI.Update(dt)

    -- 게임 오버 상태에서는 게임 로직 업데이트 스킵
    if GameState.IsDead() then
        return
    end

    if GameState.IsPlaying() then
        -- 맵 업데이트
        if MapManager then
            MapManager:Tick()
        end

        if ObjectPlacer and Player then
            ObjectPlacer:update_area(
                    -- 플레이어 근처의 좁은 영역에만 소환
                    MapConfig.map_chunk_y_size * 0.5,                              -- area_width (Y축)
                    MapConfig.map_chunk_x_size,                              -- area_height (X축)
                    Player.Location.Y,                                          -- area_width_offset (Y축)
                    Player.Location.X + MapConfig.map_chunk_x_size * 1.5  -- area_height_offset (X축)
            )
        end

        -- StageManager가 스테이지별 장애물 관리
        if GameStageManager then
            GameStageManager:Tick()
        end

        if ItemManager then
            ItemManager:Tick()
        end

        if BasicMonsterDifficultyManager then
            local new_spawn_num = BasicMonsterDifficultyManager:Tick(dt)
            MonsterManager:set_spawn_num("Data/Prefabs/w14_BasicMonster.prefab", new_spawn_num)
        end

        if ChaserMonsterDifficultyManager then
            local new_spawn_num = ChaserMonsterDifficultyManager:Tick(dt)
            MonsterManager:set_spawn_num("Data/Prefabs/w14_ChaserMonster.prefab", new_spawn_num)
        end

        if BoomerMonsterDifficultyManager then
            local new_spawn_num = BoomerMonsterDifficultyManager:Tick(dt)
            MonsterManager:set_spawn_num("Data/Prefabs/w14_BoomerMonster.prefab", new_spawn_num)
        end

        if MonsterManager then
            MonsterManager:Tick()
        end

        if BossMonsterManager then
            BossMonsterManager:Tick(dt)
        end
    end
end

function HandleInput()
    -- 스페이스바: 시작 화면에서 게임 시작
    if InputManager:IsKeyPressed(' ') then
        if GameState.IsStart() then
            GameState.StartGame()
            InputManager:SetCursorVisible(false)  -- 게임 시작 시 커서 숨김
        end
    end

    -- ESC: 게임 중 커서 토글
    if InputManager:IsKeyPressed(27) then  -- 27 = ESC 키코드
        if GameState.IsPlaying() then
            InputManager:SetCursorVisible(true)  -- 커서 표시
        end
    end

    -- 마우스 클릭: 커서 숨김 (게임 중일 때)
    if InputManager:IsMouseButtonPressed(0) then
        if GameState.IsPlaying() then
            InputManager:SetCursorVisible(false)  -- 커서 숨김
        end
    end

    -- Q: 게임 중 종료 (게임 오버 화면으로 이동)
    if InputManager:IsKeyPressed('Q') then
        if GameState.IsPlaying() then
            GameState.EndGame()  -- DEAD 콜백에서 커서 표시, BGM 정지 처리됨
        end
    end
end
