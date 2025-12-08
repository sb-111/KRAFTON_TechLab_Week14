local PlayerAnimClass = require("Player/w14_PlayerAnim")
local PlayerInputClass = require("Player/w14_PlayerInput")
local PlayerSlowClass = require("Player/w14_PlayerSlow")
local PlayerKnockBackClass = require("Player/w14_PlayerKnockBack")
local PlayerADSClass = require("Player/w14_PlayerADS")
local ObstacleConfig = require("w14_ObstacleConfig")
local MonsterConfig = require("w14_MonsterConfig")
local GameState = require("Game/w14_GameStateManager")
local Audio = require("Game/w14_AudioManager")
local AmmoManager = require("Game/w14_AmmoManager")
local ScoreManager = require("Game/w14_ScoreManager")
local HPManager = require("Game/w14_HPManager")
local Particle = require("Game/w14_ParticleManager")
local UI = require("Game/w14_UIManager")
local AirstrikeManager = require("Game/w14_AirstrikeManager")

local PlayerAnim = nil
local PlayerInput = nil
local PlayerSlow = nil
local PlayerKnockBack = nil
local PlayerADS = nil

local PlayerCamera = nil
local Mesh = nil
local Gun = nil
local MuzzleParticle = nil
local BoxCollider = nil

local bIsStarted = false

local ShootCoolTime = 0.1
local IsShootCool = false
local MovementSpeed = 6.0

function BeginPlay()
    Obj:SetPhysicsState(false)
    Obj:SetPhysicsState(true)

    Audio.Init()
    local result = Audio.RegisterSFX("gunshot", "GunShotActor")
    local result = Audio.RegisterSFX("gunreload", "GunReloadActor")
    local result = Audio.RegisterSFX("gundryfire", "GunDryFireActor")
    local result = Audio.RegisterSFX("GainedAirstrike", "GainedAirstrike")  -- 공중폭격 아이템 획득
    -- local result = Audio.RegisterSFX("Jet", "JetSoundActor")         -- 제트 효과음

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

    -- ADS 시스템 초기화 (카메라, 총, 플레이어, 스켈레탈메시)
    PlayerADS = PlayerADSClass:new(PlayerCamera, Gun, Obj, Mesh)
    PlayerADS:InitIronSight("Data/Prefabs/w14_IronSight.prefab")

    Reset()
end

function StartGame()
    if PlayerCamera then
        GetCameraManager():SetViewTargetWithBlend(PlayerCamera, 1.2)
        RestoreBaseVignette()
    end

    StartCoroutine(ToRun)
end

function Reset()
    Obj.Location = Vector(0, 0, 1.3)
    Obj.Rotation = Vector(0, 0, 0)
    bIsStarted = false
    HPManager.Reset()
    PlayerAnim:Reset()
    ClearGameVignette()

    -- 카메라 회전/리코일 초기화
    TargetYaw = 0.0
    TargetPitch = 0.0
    CurrentYaw = 0.0
    CurrentPitch = 0.0
    CurrentRecoil = 0.0
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

        local Forward = Delta * MovementSpeed * PlayerSlow:GetSpeedMultiplier()
        local MoveAmount = 0
        if math.abs(PlayerInput.HorizontalInput) > 0 then
            MoveAmount = PlayerInput.HorizontalInput * MovementSpeed * Delta * PlayerSlow:GetSpeedMultiplier()
        end

        -- KnockBack 이동량 추가 (플레이어 조작과 상쇄됨)
        local KnockBackAmount = PlayerKnockBack:Update(Delta)
        MoveAmount = MoveAmount + KnockBackAmount

        Obj.Location = Obj.Location + Vector(Forward, MoveAmount, 0)

        -- ADS 업데이트 (FOV, 아이언사이트, 총 가시성)
        if PlayerADS then
            PlayerADS:Update(Delta, PlayerInput.ZoomTrigger)
        end

        -- ADS 보간 중에는 발사 불가
        local bADSCanFire = not PlayerADS or PlayerADS:CanFire()
        local bCanFire = AmmoManager.GetCurrentAmmo() > 0 and not AmmoManager.IsReloading() and bADSCanFire
        PlayerAnim:Update(Delta, PlayerInput.ShootTrigger, bCanFire)
        if PlayerInput.ShootTrigger and not IsShootCool and not AmmoManager.IsReloading() and bADSCanFire then
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

-- 카메라 리코일 설정
local RecoilAmount = 2.0          -- 힙파이어 시 한 발당 Pitch 증가량 (도)
local RecoilADSMult = 0.5         -- ADS 시 반동 배수 (50%)
local RecoilRecoverySpeed = 5.0   -- 리코일 복구 속도
local CurrentRecoil = 0.0         -- 현재 누적 리코일

function Rotate(DeltaTime)
    local InputDelta = PlayerInput.RotateVector

    -- ADS 시 감도 감소
    local SensMult = PlayerADS and PlayerADS:GetSensitivityMultiplier() or 1.0
    TargetYaw = TargetYaw + (InputDelta.X * MouseSensitivity * SensMult)
    TargetPitch = TargetPitch - (InputDelta.Y * MouseSensitivity * SensMult)

    -- 리코일 복구 (쏘지 않을 때 서서히 원래 조준점으로)
    if CurrentRecoil > 0.001 then  -- 부동소수점 오차 방지
        local recovery = RecoilRecoverySpeed * DeltaTime
        if recovery > CurrentRecoil then recovery = CurrentRecoil end
        TargetPitch = TargetPitch + recovery  -- 아래로 복구 (Pitch 증가 = 아래)
        CurrentRecoil = CurrentRecoil - recovery
    else
        CurrentRecoil = 0  -- 작은 값은 0으로 정리
    end

    -- Yaw 제한
    if TargetYaw > 90.0 then TargetYaw = 90.0
    elseif TargetYaw < -90.0 then TargetYaw = -90.0 end

    if TargetPitch > 70.0 then TargetPitch = 70.0
    elseif TargetPitch < -70.0 then TargetPitch = -70.0 end

    local SmoothFactor = math.min(RotationSmoothSpeed * DeltaTime, 1.0)

    CurrentYaw = CurrentYaw + (TargetYaw - CurrentYaw) * SmoothFactor
    CurrentPitch = CurrentPitch + (TargetPitch - CurrentPitch) * SmoothFactor

    local NewRotation = Vector(0, CurrentPitch, CurrentYaw)
    Obj.Rotation = NewRotation
end

-- 리코일 적용 (Shoot에서 호출)
function ApplyRecoil()
    local isADS = PlayerADS and PlayerADS:IsAiming()
    local recoilMult = isADS and RecoilADSMult or 1.0
    local actualRecoil = RecoilAmount * recoilMult

    TargetPitch = TargetPitch - actualRecoil  -- 위로 튀게 (Pitch 감소 = 위)
    CurrentRecoil = CurrentRecoil + actualRecoil
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
    ApplyRecoil()  -- 카메라 리코일 적용

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
                else
                    UI.ShowShotFeedback()
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
    MuzzleParticle:Deactivate()
    IsShootCool = false
end

-- 충돌 처리: 장애물/아이템/몹
function OnBeginOverlap(OtherActor)
    if not OtherActor then return end

    -- 장애물 충돌 처리
    if OtherActor.Tag == "obstacle" and PlayerSlow and not PlayerSlow:IsSlowed() then
        local obstacleName = OtherActor.Name
        local cfg = ObstacleConfig[obstacleName] or ObstacleConfig.Default

        print("[OnBeginOverlap] 장애물 충돌: " .. obstacleName)

        -- 방향 계산
        local playerY = Obj.Location.Y
        local obstacleY = OtherActor.Location.Y
        local direction = (playerY < obstacleY) and -1 or 1

        -- [수정된 호출]
        -- ApplySlow(속도배율, 지속시간, 넉백강도, 넉백방향)
        -- 이제 이 함수가 비네트와 카메라 기울기를 모두 수행합니다.
        PlayerSlow:ApplySlow(cfg.speedMult, cfg.duration, cfg.knockBackStrength, direction)
        PlayObstacleHitVignette(cfg.duration * 0.6)

        -- 물리적 밀려남(위치 이동) 처리는 별도 유지
        if PlayerKnockBack and cfg.knockBackStrength then
            PlayerKnockBack:ApplyKnockBack(direction, cfg.knockBackStrength)
        end
        
        -- 재장전 중이면 취소
        if AmmoManager.IsReloading() then
            Audio.StopSFX("gunreload")
            AmmoManager.CancelReload()
            PlayerAnim:CancelReload()  -- ← 애니메이션 취소
        end
        
        -- 충돌 효과음 출력
        Audio.PlaySFX("ObstacleCollisionScream")
        -- Audio.SetSFXVolume("ObstacleCollisionScream", 50)
        Audio.PlaySFX("ObstacleCollisionSmash")
        -- Audio.SetSFXVolume("ObstacleCollisionSmash", 50)
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
            
            PlayHitVignette(0.5)
        end

        if died then
            -- TODO: 게임 오버 처리
            print("[Player] Game Over!")
        else
            -- 충돌 효과음 출력
            Audio.PlaySFX("MonsterCollisionScream")
            Audio.PlaySFX("MonsterCollisionSmash")
        end
    end

    -- 탄약 아이템 획득
    if OtherActor.Tag == "AmmoItem" then
        local ammoAmount = AmmoManager.GetMaxAmmo()  -- 한 번 장전할 때 채우는 탄약량 (30발)
        AmmoManager.AddAmmo(ammoAmount)
        print("[OnBeginOverlap] 탄약 획득! +" .. ammoAmount)
        -- 아이템 비활성화
        OtherActor.bIsActive = false
        
        Audio.PlaySFX("GainedAmmo")
    end

    -- 회복 아이템 획득 (AidKit)
    if OtherActor.Tag == "AidKit" then
        HPManager.Heal(5)
        print("[OnBeginOverlap] AidKit 획득! HP +5")
        -- 아이템 비활성화
        OtherActor.bIsActive = false

        Audio.PlaySFX("GainedAidKit")
    end

    -- 아드레날린 아이템 획득 (Adrenalin) - 10초 슬로모
    if OtherActor.Tag == "Adrenalin" then
        SetSlomo(10.0, 0.5)  -- 10초 동안 0.5배속 (슬로우 모션)
        print("[OnBeginOverlap] Adrenalin 획득! 10초간 슬로모션")
        -- 아이템 비활성화
        OtherActor.bIsActive = false

        Audio.PlaySFX("GainedAdrenalin")
    end

    -- 공중폭격 아이템 획득 (AirstrikeItem)
    if OtherActor.Tag == "AirstrikeItem" then
        print("[OnBeginOverlap] Airstrike 획득! 공중폭격 발동")
        AirstrikeManager.Execute(Obj.Location)
        OtherActor.bIsActive = false
        Audio.PlaySFX("GainedAirstrike")
    end
end

local BaseVignette = {
    Duration = -1,          -- -1: 무한 지속 (삭제 안 됨)
    Radius = 0.4,
    Softness = 0.2,
    Intensity = 0.6,        -- 평소 어두운 정도 (0.0 ~ 1.0)
    Roundness = 2.0,        -- 원형
    Color = Color(0, 0, 0, 1) -- ★ 검은색
}

-- [함수 1] 게임 시작 시 or 효과 종료 후 복구용
function RestoreBaseVignette()
    local cm = GetCameraManager()
    if cm then
        -- AdjustVignette는 없으면 만들고(Start), 있으면 갱신(Update)합니다.
        -- LastVignetteIdx 하나만 계속 재활용하게 됩니다.
        cm:AdjustVignette(
            BaseVignette.Duration,
            BaseVignette.Radius,
            BaseVignette.Softness,
            BaseVignette.Intensity,
            BaseVignette.Roundness,
            BaseVignette.Color,
            0 -- 우선순위
        )
    end
end

local bIsHitEffectPlaying = false
local bIsObstacleHitEffectPlaying = false

-- [함수 2] 맞았을 때 호출 (적 피해)
function PlayHitVignette(duration)
    if bIsObstacleHitEffectPlaying or bIsHitEffectPlaying then return end
    
    local cm = GetCameraManager()
    if not cm then return end
    
    bIsHitEffectPlaying = true
    
    StartCoroutine(function()
        local elapsedTime = 0
        local deltaTime = 0.016
        
        while elapsedTime < duration do
            coroutine.yield("wait_time", deltaTime)
            elapsedTime = elapsedTime + deltaTime
            
            -- 0 -> 1 -> 0 (사인파)
            local t = math.min(elapsedTime / duration, 1.0)
            local intensity = math.sin(t * math.pi) * 0.5 -- 최대 강도 0.5
            
            cm:AdjustVignette(
                -1,   -- 지속시간은 무한으로 둠
                0.4,  -- Radius (살짝 좁게)
                0.2,  -- Softness
                intensity, 
                2.0, 
                Color(1, 0, 0, 1), -- ★ 빨간색
                0    -- 우선순위
            )
        end
        
        -- 효과가 끝났으면 원래 검은색으로 복구
        RestoreBaseVignette()
        
        bIsHitEffectPlaying = false
    end)
end

-- [함수 2-1] 장애물 맞았을 때 호출 (검은색 진하게)
function PlayObstacleHitVignette(duration)
    if bIsObstacleHitEffectPlaying or bIsHitEffectPlaying then return end
    
    local cm = GetCameraManager()
    if not cm then return end
    
    bIsObstacleHitEffectPlaying = true
    
    StartCoroutine(function()
        local elapsedTime = 0
        local deltaTime = 0.016
        
        while elapsedTime < duration do
            coroutine.yield("wait_time", deltaTime)
            elapsedTime = elapsedTime + deltaTime
            
            -- 0 -> 1 -> 0 (사인파)
            local t = math.min(elapsedTime / duration, 1.0)
            -- BaseVignette.Intensity(0.6) + 사인파로 추가 (0 ~ 0.3)
            local additionalIntensity = math.sin(t * math.pi) * 0.3
            local intensity = BaseVignette.Intensity + additionalIntensity -- 0.6 -> 0.9 -> 0.6
            
            cm:AdjustVignette(
                -1,   -- 지속시간은 무한으로 둠
                0.4,  -- Radius
                0.2,  -- Softness
                intensity, 
                2.0, 
                Color(0, 0, 0, 1), -- ★ 검은색 (빨강 0, 초록 0, 파랑 0)
                1    -- 우선순위 (조금 높게 해서 우선 적용)
            )
        end
        
        -- 효과가 끝났으면 원래 검은색으로 복구
        RestoreBaseVignette()
        
        bIsObstacleHitEffectPlaying = false
    end)
end

-- [함수 3] 게임 오버 / 스테이지 종료 시
function ClearGameVignette()
    local cm = GetCameraManager()
    if cm then
        cm:DeleteVignette() -- C++의 LastVignetteIdx를 찾아서 bEnabled = false 처리
    end
end