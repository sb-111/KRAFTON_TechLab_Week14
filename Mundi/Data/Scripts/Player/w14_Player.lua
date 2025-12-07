local PlayerAnimClass = require("Player/w14_PlayerAnim")
local PlayerInputClass = require("Player/w14_PlayerInput")
local PlayerSlowClass = require("Player/w14_PlayerSlow")
local PlayerHPClass = require("Player/w14_PlayerHP")
local ObstacleConfig = require("w14_ObstacleConfig")
local MonsterConfig = require("w14_MonsterConfig")
local Audio = require("Game/w14_AudioManager")
local AmmoManager = require("Game/w14_AmmoManager")
local ScoreManager = require("Game/w14_ScoreManager")

local PlayerAnim = nil
local PlayerInput = nil
local PlayerSlow = nil
local PlayerHP = nil

local PlayerCamera = nil
local Mesh = nil
local Gun = nil
local MuzzleParticle = nil

local bIsStarted = false

local ShootCoolTime = 0.1
local IsShootCool = false
local MovementSpeed = 10.0

function BeginPlay()
    Audio.Init()
    local result = Audio.RegisterSFX("gunshot", "GunShotActor")

    Mesh = GetComponent(Obj, "USkeletalMeshComponent")
    Gun = GetComponent(Obj, "UStaticMeshComponent")
    MuzzleParticle = GetComponent(Obj, "UParticleSystemComponent")

    Gun:SetupAttachment(Mesh, "mixamorig6:RightHand")

    PlayerAnim = PlayerAnimClass:new(Obj)
    PlayerInput = PlayerInputClass:new(Obj)
    PlayerSlow = PlayerSlowClass:new(Obj)
    PlayerHP = PlayerHPClass:new(Obj)
    PlayerHP:Reset()
    StartCoroutine(ToRun)

    PlayerCamera = GetComponent(Obj, "UCameraComponent")
    if PlayerCamera then
        GetCameraManager():SetViewTarget(PlayerCamera)
    end

    bIsStarted = false
end

function Tick(Delta)
    if bIsStarted then
        PlayerInput:Update(Delta)
        Rotate(Delta)

        -- 사용자 임의로 위아래로 움직이고 싶을 때 디버그용
        local Forward = 0.01 * PlayerInput.VerticalInput
        
        -- local Forward = 0.01 * PlayerSlow:GetSpeedMultiplier()
        local MoveAmount = 0
        if math.abs(PlayerInput.HorizontalInput) > 0 then
            MoveAmount = PlayerInput.HorizontalInput * MovementSpeed * Delta * PlayerSlow:GetSpeedMultiplier()
        end
        Obj.Location = Obj.Location + Vector(Forward, MoveAmount, 0)

        PlayerAnim:Update(Delta, PlayerInput.ShootTrigger)
        if PlayerInput.ShootTrigger and not IsShootCool and not AmmoManager.IsReloading() then
            Shoot()
        end

        -- R키 재장전
        if InputManager:IsKeyPressed('R') then
            TryReload()
        end
    end
end

--- 재장전 시도 (AmmoManager에게 위임)
function TryReload()
    if AmmoManager.StartReload() then
        -- TODO: 재장전 애니메이션 재생
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
    -- 탄약 체크 및 소비
    if not AmmoManager.UseAmmo(1) then
        -- TODO: 탄약 부족 효과음 재생
        return
    end

    StartCoroutine(ShootCoolEnd)
    IsShootCool = true
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

        if HitResult.Actor and MonsterConfig.IsMonsterTag(HitResult.Actor.Tag) then
            Bullet = SpawnPrefab("Data/Prefabs/w14_Bullet0.prefab")
            -- Bullet.Location = Vector(TargetPoint.X, TargetPoint.Y - 3, TargetPoint.Z)
            Bullet.Location = TargetPoint
        end

        print("You shoot " .. HitResult.Actor.Name .. " " .. HitResult.Actor.Tag)
    else
        -- 허공을 쐈으면 사거리 끝 지점을 목표점으로 설정
        TargetPoint = CamPos + (CamDir * MaxDist)
    end

    local MuzzlePos = MuzzleParticle:GetWorldLocation()

    -- 총구에서 목표점까지의 벡터 계산 (목표점 - 시작점)
    local DirVec = TargetPoint - MuzzlePos
    DirVec:Normalize()
    local RealBulletDir = DirVec

    -- TODO: 킬 감지는 몬스터 시스템 완료 후 연동 필요
    -- 방법 1: DamageManager 패턴 (몬스터가 등록, 플레이어가 데미지 요청)
    -- 방법 2: 몬스터 스크립트에서 OnHit 이벤트 구현
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

        -- 재장전 중이면 취소
        AmmoManager.CancelReload()
    end

    -- 몹과 충돌 처리 (Tag 기반 데미지)
    if MonsterConfig.IsMonsterTag(OtherActor.Tag) then
        local damage = MonsterConfig.GetDamage(OtherActor.Tag)
        local died = PlayerHP:TakeDamage(damage)

        print("[OnBeginOverlap] 몹 충돌! Tag: " .. OtherActor.Tag .. ", 데미지: " .. damage)

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
        -- 아이템 제거 (오브젝트 풀로 반환하거나 삭제)
        if OtherActor.ReturnToPool then
            OtherActor:ReturnToPool()
        end
    end
end