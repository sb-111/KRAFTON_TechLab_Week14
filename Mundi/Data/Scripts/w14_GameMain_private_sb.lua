-- w14_GameMain.lua
-- 게임 메인 스크립트 (UI Actor에 붙임)
-- 필요: UBillboardComponent

local GameState = require("Game/w14_GameStateManager")
local UI = require("Game/w14_UIManager")
local Audio = require("Game/w14_AudioManager")
local Particle = require("Game/w14_ParticleManager")

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

    -- 파티클 초기화
    -- 씬에 필요: 파티클 프리팹 (Actor + ParticleSystemComponent, bAutoActivate=false)
    Particle.Init()
    -- Particle.Register("muzzle", "Data/Prefabs/MuzzleFlash.prefab", 5, 0.5)
    -- Particle.Register("blood", "Data/Prefabs/BloodSplat.prefab", 10, 1.0)
    Particle.Register("test", "Data/Prefabs/ModuleTest.prefab", 5, 2.0)  -- 테스트용

    -- 시작 화면 표시
    GameState.ShowStartScreen()
end

function Tick(dt)
    -- UI 위치 업데이트 (카메라 앞에 유지)
    UI.Update()

    -- 파티클 업데이트 (수명 관리)
    Particle.Tick(dt)

    -- 입력 처리
    HandleInput()
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

    -- Q: 게임 중 종료
    if InputManager:IsKeyPressed('Q') then
        if GameState.IsPlaying() then
            GameState.EndGame()
            Audio.StopBGM("BGM")  -- 게임 종료 시 BGM 정지
        end
    end

    -- T: SFX 테스트 (게임 중)
    if InputManager:IsKeyPressed('T') then
        if GameState.IsPlaying() then
            Audio.PlaySFX("Shot")
            print("[Test] PlaySFX('Shot')")
        end
    end

    -- P: 파티클 테스트 (게임 중)
    if InputManager:IsKeyPressed('P') then
        if GameState.IsPlaying() then
            -- 카메라 앞 위치에 파티클 생성
            local cam = GetCamera()
            if cam then
                local camPos = cam:GetActorLocation()
                local camFwd = cam:GetActorForward()
                local pos = camPos + camFwd * 30  
                Particle.Spawn("test", pos)
                print("[Test] Particle.Spawn('test')")
            end
        end
    end
end
