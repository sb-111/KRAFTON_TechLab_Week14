local PlayerAnimClass = require("Player/w14_PlayerAnim")
local PlayerInputClass = require("Player/w14_PlayerInput")
local PlayerSlowClass = require("Player/w14_PlayerSlow")
local PlayerKnockBackClass = require("Player/w14_PlayerKnockBack")
local ObstacleConfig = require("w14_ObstacleConfig")
local MonsterConfig = require("w14_MonsterConfig")
local GameState = require("Game/w14_GameStateManager")
local Audio = require("Game/w14_AudioManager")
local AmmoManager = require("Game/w14_AmmoManager")
local ScoreManager = require("Game/w14_ScoreManager")
local HPManager = require("Game/w14_HPManager")
local Particle = require("Game/w14_ParticleManager")
local UI = require("Game/w14_UIManager")

local PlayerAnim = nil
local PlayerInput = nil
local PlayerSlow = nil
local PlayerKnockBack = nil

local PlayerCamera = nil
local Mesh = nil
local Gun = nil
local MuzzleParticle = nil
local BoxCollider = nil

local bIsStarted = false

local ShootCoolTime = 0.1
local IsShootCool = false
local MovementSpeed = 10.0

function BeginPlay()
    Obj:SetPhysicsState(false)
    Obj:SetPhysicsState(true)

    Audio.Init()
    local result = Audio.RegisterSFX("gunshot", "GunShotActor")
    local result = Audio.RegisterSFX("gunreload", "GunReloadActor")
    local result = Audio.RegisterSFX("gundryfire", "GunDryFireActor")

    Particle.Init()
    -- 피 파티클 등록 (Body/Head × Red/Green)
    Particle.Register("blood_body_red", "Data/Prefabs/w14_Blood_Body_Red.prefab", 20, 1.0)
    --Particle.Register("blood_body_green", "Data/Prefabs/w14_Blood_Body_Green.prefab", 20, 1.0) -- 지금 없음 (색만 바꾸면 될듯)
    Particle.Register("blood_head_red", "Data/Prefabs/w14_Blood_Head_Red.prefab", 15, 1.2)
    --Particle.Register("blood_head_green", "Data/Prefabs/w14_Blood_Head_Green.prefab", 15, 1.2) -- 지금 없음

    Mesh = GetComponent(Obj, "USkeletalMeshComponent")
    Gun = GetComponent(Obj, "UStaticMeshComponent")
    MuzzleParticle = GetComponent(Obj, "UParticleSystemComponent")
    BoxCollider = GetComponent(Obj, "UBoxComponent")

    Gun:SetupAttachment(Mesh, "mixamorig6:RightHand")

    PlayerAnim = PlayerAnimClass:new(Obj)
    PlayerInput = PlayerInputClass:new(Obj)
    PlayerSlow = PlayerSlowClass:new(Obj)
    PlayerKnockBack = PlayerKnockBackClass:new(Obj)

    PlayerCamera = GetComponent(Obj, "UCameraComponent")

    Reset()
end

function StartGame()
    if PlayerCamera then
        GetCameraManager():SetViewTargetWithBlend(PlayerCamera, 1.0)
    end
    
    StartCoroutine(ToRun)
end

function Reset()
    Obj.Location = Vector(0, 0, 1.3)
    bIsStarted = false
    HPManager.Reset()
    PlayerAnim:Reset()
end

function Tick(Delta)
    Particle.Tick(Delta)

    -- 게임 오버 상태에서는 플레이어 동작 중지
    if GameState.IsDead() then
        return
    end

    if bIsStarted then
        PlayerInput:Update(Delta)
        Rotate(Delta)

        -- 사용자 임의로 위아래로 움직이고 싶을 때 디버그용
        -- local Forward = 0.01 * PlayerInput.VerticalInput

        local Forward = Delta * 10 * PlayerSlow:GetSpeedMultiplier()
        local MoveAmount = 0
        if math.abs(PlayerInput.HorizontalInput) > 0 then
            MoveAmount = PlayerInput.HorizontalInput * MovementSpeed * Delta * PlayerSlow:GetSpeedMultiplier()
        end

        -- KnockBack 이동량 추가 (플레이어 조작과 상쇄됨)
        local KnockBackAmount = PlayerKnockBack:Update(Delta)
        MoveAmount = MoveAmount + KnockBackAmount

        Obj.Location = Obj.Location + Vector(Forward, MoveAmount, 0)


        local bCanFire = AmmoManager.GetCurrentAmmo() > 0 and not AmmoManager.IsReloading()
        PlayerAnim:Update(Delta, PlayerInput.ShootTrigger, bCanFire)
        if PlayerInput.ShootTrigger and not IsShootCool and not AmmoManager.IsReloading() then
            Shoot()
        end

        -- R키 재장전
        if PlayerInput.ReloadTrigger then
            TryReload()
        end
    end
end

--- 재장전 시도 (AmmoManager에게 위임)
function TryReload()
    if AmmoManager.StartReload() then
        PlayerAnim:StartReload()  -- ← 애니메이션 시작
        Audio.PlaySFX("gunreload")
        StartCoroutine(ReloadCoroutine)
    end
end

function ReloadCoroutine()
    coroutine.yield("wait_time", AmmoManager.GetReloadDuration())
    AmmoManager.CompleteReload()
end

function ToRun()
    coroutine.yield("wait_time", 1)
    bIsStarted = true
end

local MouseSensitivity = 0.1 -- 감도 설정 (필요에 따라 조절)
local CurrentYaw = 0.0       -- 좌우 회전 (Z축) 누적값
local CurrentPitch = 0.0     -- 상하 회전 (Y축) 누적값

-- 부드러운 회전
local TargetYaw = 0.0
local TargetPitch = 0.0
local CurrentYaw = 0.0
local CurrentPitch = 0.0

-- 보간 속도 (높을수록 빠릿하고, 낮을수록 부드럽지만 반응이 느림)
local RotationSmoothSpeed = 20.0 

function Rotate(DeltaTime)
    local InputDelta = PlayerInput.RotateVector
    
    TargetYaw = TargetYaw + (InputDelta.X * MouseSensitivity)
    TargetPitch = TargetPitch - (InputDelta.Y * MouseSensitivity)
    
    if TargetPitch > 70.0 then TargetPitch = 70.0
    elseif TargetPitch < -70.0 then TargetPitch = -70.0 end

    local SmoothFactor = math.min(RotationSmoothSpeed * DeltaTime, 1.0)
    
    CurrentYaw = CurrentYaw + (TargetYaw - CurrentYaw) * SmoothFactor
    CurrentPitch = CurrentPitch + (TargetPitch - CurrentPitch) * SmoothFactor
    
    local NewRotation = Vector(0, CurrentPitch, CurrentYaw)
    Obj.Rotation = NewRotation
end

function Shoot()
    StartCoroutine(ShootCoolEnd)
    IsShootCool = true
    
    -- 탄약 체크 및 소비
    if not AmmoManager.UseAmmo(1) then
        Audio.PlaySFX("gundryfire")
        return
    end

    PlayerAnim:Fire()

    MuzzleParticle:Activate()
    Audio.PlaySFX("gunshot")

    local CamPos = PlayerCamera:GetWorldLocation()
    local CamDir = PlayerCamera:GetForward()
    local MaxDist = 10000.0
    -- 레이캐스트
    local bHit, HitResult = Physics.Raycast(CamPos + CamDir, CamDir, MaxDist)

    local TargetPoint = nil
    if bHit then
        -- 벽이나 적에 맞았으면 그곳이 목표점
        TargetPoint = HitResult.Point
        local BulletDecal = SpawnPrefab("Data/Prefabs/w14_BulletDecal.prefab")
        BulletDecal.Location = TargetPoint

        local Normal = HitResult.Normal
        local Nz = Normal.Z
        if Nz > 1.0 then Nz = 1.0 elseif Nz < -1.0 then Nz = -1.0 end
        
        local Pitch = math.deg(math.asin(Nz))
        local Yaw = math.deg(math.atan(Normal.Y, Normal.X))
        local Roll = math.random(0, 360)
        
        BulletDecal.Rotation = Vector(Roll, Pitch, Yaw)

        -- [몬스터 피격 로직]
        if HitResult.Actor and MonsterConfig.IsMonsterTag(HitResult.Actor.Tag) then
            local monsterScript = HitResult.Actor:GetScript()
            
            if monsterScript then
                -- 1. 기본 데미지 설정
                local finalDamage = 10
                local isHeadshot = false

                if HitResult.Name == "Head" then
                    finalDamage = finalDamage * 2   -- 데미지 2배
                    isHeadshot = true
                    print("!!! HEADSHOT !!!")
                    UI.ShowHeadshotFeedback()
                end

                -- 피 색상과 부위에 따른 파티클 스폰
                local bloodColor = MonsterConfig.GetBloodColor(HitResult.Actor.Tag)
                local particleName = isHeadshot
                    and ("blood_head_" .. bloodColor)
                    or ("blood_body_" .. bloodColor)
                print("[DEBUG] Spawning particle: " .. particleName .. " at " .. tostring(TargetPoint))
                local spawnResult = Particle.Spawn(particleName, TargetPoint)
                if spawnResult then
                    print("[DEBUG] Particle spawned successfully")
                else
                    print("[DEBUG] Particle spawn FAILED - pool exhausted?")
                end

                -- 3. 데미지 적용
                if monsterScript.GetDamage then
                    monsterScript.GetDamage(finalDamage)
                end

                -- 4. 헤드샷일 경우 진동 피드백 실행
                if isHeadshot and InputManager:IsGamepadConnected(0) then
                    InputManager:SetGamepadVibration(0.8, 0.8, 0)
                    StartCoroutine(VibrateFeedback)
                end
            end
        end
        print("You shoot: " .. tostring(HitResult.Actor.Name) .. " / Part: " .. tostring(HitResult.Name))
    else
        -- 허공을 쐈으면 사거리 끝 지점을 목표점으로 설정
        TargetPoint = CamPos + (CamDir * MaxDist)
    end

    local MuzzlePos = MuzzleParticle:GetWorldLocation()

    -- 총구에서 목표점까지의 벡터 계산 (목표점 - 시작점)
    local DirVec = TargetPoint - MuzzlePos
    DirVec:Normalize()
    local RealBulletDir = DirVec
end

function VibrateFeedback()
    coroutine.yield("wait_time", 0.15) -- 0.15초 동안 '징-'
    if InputManager:IsGamepadConnected(0) then
        InputManager:SetGamepadVibration(0, 0, 0) -- 진동 끄기
    end
end

function ShootCoolEnd()
    coroutine.yield("wait_time", ShootCoolTime)
    IsShootCool = false
end

-- 충돌 처리: 장애물/아이템/몹
function OnBeginOverlap(OtherActor)
    if not OtherActor then return end

    -- 장애물 충돌 처리
    if OtherActor.Tag == "obstacle" and PlayerSlow then
        local obstacleName = OtherActor.Name
        local cfg = ObstacleConfig[obstacleName] or ObstacleConfig.Default

        print("[OnBeginOverlap] 장애물 충돌: " .. obstacleName .. " (speedMult:" .. cfg.speedMult .. ", duration:" .. cfg.duration .. ")")

        -- PlayerSlow 모듈로 속도 감소 적용 (카메라 셰이크 + 자동 복구 포함)
        PlayerSlow:ApplySlow(cfg.speedMult, cfg.duration)

        -- KnockBack 적용: 플레이어가 장애물 왼쪽에 있으면 왼쪽으로, 오른쪽에 있으면 오른쪽으로 밀어냄
        if PlayerKnockBack and cfg.knockBackStrength then
            local playerY = Obj.Location.Y
            local obstacleY = OtherActor.Location.Y
            local direction = (playerY < obstacleY) and -1 or 1
            PlayerKnockBack:ApplyKnockBack(direction, cfg.knockBackStrength)
            print("[OnBeginOverlap] KnockBack 적용: direction=" .. direction .. ", strength=" .. cfg.knockBackStrength)
        end

        -- 재장전 중이면 취소
        if AmmoManager.IsReloading() then
            Audio.StopSFX("gunreload")
            AmmoManager.CancelReload()
            PlayerAnim:CancelReload()  -- ← 애니메이션 취소
        end
    end

    -- 몹과 충돌 처리 (Tag 기반 데미지)
    if MonsterConfig.IsMonsterTag(OtherActor.Tag) then
        local damage = MonsterConfig.GetDamage(OtherActor.Tag)
        local died = HPManager.TakeDamage(damage)

        print("[OnBeginOverlap] 몹 충돌! Tag: " .. OtherActor.Tag .. ", 데미지: " .. damage)

        -- KnockBack 적용: 플레이어가 몹 왼쪽에 있으면 왼쪽으로, 오른쪽에 있으면 오른쪽으로 밀어냄
        if PlayerKnockBack then
            local knockBackStrength = MonsterConfig.GetKnockBackStrength(OtherActor.Tag)
            local playerY = Obj.Location.Y
            local monsterY = OtherActor.Location.Y
            local direction = (playerY < monsterY) and -1 or 1
            PlayerKnockBack:ApplyKnockBack(direction, knockBackStrength)
            print("[OnBeginOverlap] KnockBack 적용: direction=" .. direction .. ", strength=" .. knockBackStrength)
        end

        if died then
            -- TODO: 게임 오버 처리
            print("[Player] Game Over!")
        end
    end

    -- 탄약 아이템 획득
    if OtherActor.Tag == "AmmoItem" then
        local ammoAmount = AmmoManager.GetMaxAmmo()  -- 한 번 장전할 때 채우는 탄약량 (30발)
        AmmoManager.AddAmmo(ammoAmount)
        print("[OnBeginOverlap] 탄약 획득! +" .. ammoAmount)
        -- 아이템 비활성화
        OtherActor.bIsActive = false
    end

    -- 회복 아이템 획득 (AidKit)
    if OtherActor.Tag == "AidKit" then
        HPManager.Heal(5)
        print("[OnBeginOverlap] AidKit 획득! HP +5")
        -- 아이템 비활성화
        OtherActor.bIsActive = false
    end

    -- 아드레날린 아이템 획득 (Adrenalin) - 10초 슬로모
    if OtherActor.Tag == "Adrenalin" then
        SetSlomo(10.0, 0.5)  -- 10초 동안 0.5배속 (슬로우 모션)
        print("[OnBeginOverlap] Adrenalin 획득! 10초간 슬로모션")
        -- 아이템 비활성화
        OtherActor.bIsActive = false
    end
end