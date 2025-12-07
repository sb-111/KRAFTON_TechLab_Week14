local PlayerAnimClass = require("Player/w14_PlayerAnim")
local PlayerInputClass = require("Player/w14_PlayerInput")
local PlayerSlowClass = require("Player/w14_PlayerSlow")
local ObstacleConfig = require("w14_ObstacleConfig")
local Audio = require("Game/w14_AudioManager")
local PlayerAnim = nil
local PlayerInput = nil
local PlayerSlow = nil

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

        local Forward = 0.005 * PlayerSlow:GetSpeedMultiplier()
        local MoveAmount = 0
        if math.abs(PlayerInput.HorizontalInput) > 0 then
            MoveAmount = PlayerInput.HorizontalInput * MovementSpeed * Delta * PlayerSlow:GetSpeedMultiplier()
        end
        Obj.Location = Obj.Location + Vector(Forward, MoveAmount, 0)

        PlayerAnim:Update(Delta, PlayerInput.ShootTrigger)
        if PlayerInput.ShootTrigger and not IsShootCool then
            Shoot()
        end
    end
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
        --if HitResult.BoneName then
        --    local BoneStr = HitResult.BoneName:ToString()
        --    
        --    print(BoneStr)
        --    -- 본 이름에 "Head"가 포함되어 있으면 (예: mixamorig6:Head)
        --    if string.find(BoneStr, "Head") then
        --        print("HEADSHOT! [Vibration Triggered]")
        --        
        --        if InputManager:IsGamepadConnected(0) then
        --            -- (LeftMotor, RightMotor, Index)
        --            InputManager:SetGamepadVibration(0.3, 0.5, 0)
        --            
        --            -- 진동 끄기 예약
        --            StartCoroutine(VibrateFeedback)
        --        end
        --    end
        --end

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

        if HitResult.Actor and HitResult.Actor.Name == "monster" then
            Bullet = SpawnPrefab("Data/Prefabs/w14_Bullet0.prefab")
            Bullet.Location = TargetPoint
        end

        print("You shoot" .. HitResult.Actor.Name .. HitResult.Actor.Tag)
    else
        -- 허공을 쐈으면 사거리 끝 지점을 목표점으로 설정
        TargetPoint = CamPos + (CamDir * MaxDist)
    end

    local MuzzlePos = MuzzleParticle:GetWorldLocation()
    
    -- 총구에서 목표점까지의 벡터 계산 (목표점 - 시작점)
    local DirVec = TargetPoint - MuzzlePos
    DirVec:Normalize()
    local RealBulletDir = DirVec

    -- ApplyDamage(HitResult.Actor)
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

-- 충돌 처리: 장애물과 충돌 시 속도 감소
function OnBeginOverlap(OtherActor)
    if not OtherActor then return end

    -- 장애물 충돌 처리
    if OtherActor.Tag == "obstacle" and PlayerSlow then
        local obstacleName = OtherActor.Name
        local cfg = ObstacleConfig[obstacleName] or ObstacleConfig.Default

        print("[OnBeginOverlap] 장애물 충돌: " .. obstacleName .. " (speedMult:" .. cfg.speedMult .. ", duration:" .. cfg.duration .. ")")

        -- PlayerSlow 모듈로 속도 감소 적용 (카메라 셰이크 + 자동 복구 포함)
        PlayerSlow:ApplySlow(cfg.speedMult, cfg.duration)
    end
end