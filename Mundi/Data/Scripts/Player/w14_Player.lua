local PlayerAnimClass = require("Player/w14_PlayerAnim")
local PlayerInputClass = require("Player/w14_PlayerInput")
local PlayerCamera = nil
local PlayerAnim = nil
local PlayerInput = nil

local Mesh = nil
local Gun = nil
local MuzzleParticle = nil

local bIsStarted = false

local ShootCoolTime = 0.1
local IsShootCool = false
local MovementSpeed = 10.0

function BeginPlay()
    Mesh = GetComponent(Obj, "USkeletalMeshComponent")
    Gun = GetComponent(Obj, "UStaticMeshComponent")
    MuzzleParticle = GetComponent(Obj, "UParticleSystemComponent")

    Gun:SetupAttachment(Mesh, "mixamorig6:RightHand")

    PlayerAnim = PlayerAnimClass:new(Obj)
    PlayerInput = PlayerInputClass:new(Obj)
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
        Rotate()

        if math.abs(PlayerInput.HorizontalInput) > 0 then
            local MoveAmount = PlayerInput.HorizontalInput * MovementSpeed * Delta
            Obj.Location = Obj.Location + Vector(0, MoveAmount, 0)
        end

        if PlayerInput.ShootTrigger and not IsShootCool then
            Shoot()
        end
    end
end

function ToRun()
    coroutine.yield("wait_time", 1)
    bIsStarted = true
    PlayerAnim:ToRun()
end

local MouseSensitivity = 0.1 -- 감도 설정 (필요에 따라 조절)
local CurrentYaw = 0.0       -- 좌우 회전 (Z축) 누적값
local CurrentPitch = 0.0     -- 상하 회전 (Y축) 누적값

function Rotate()
    local Delta = PlayerInput.RotateVector
    
    CurrentYaw = CurrentYaw + (Delta.X * MouseSensitivity)
    CurrentPitch = CurrentPitch - (Delta.Y * MouseSensitivity)
    
    if CurrentPitch > 70.0 then
        CurrentPitch = 70.0
    elseif CurrentPitch < -70.0 then
        CurrentPitch = -70.0
    end
    local NewRotation = Vector(0, CurrentPitch, CurrentYaw)
    
    Obj.Rotation = NewRotation
end

function Shoot()
    StartCoroutine(ShootCoolEnd)
    IsShootCool = true

    PlayerAnim:ToShoot()    
    local CamPos = PlayerCamera:GetWorldLocation()
    local CamDir = PlayerCamera:GetForward()
    local MaxDist = 10000.0
    -- 레이캐스트
    local bHit, HitResult = Physics.Raycast(CamPos + CamDir * 5, CamDir, MaxDist)
    
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

function ShootCoolEnd()
    coroutine.yield("wait_time", ShootCoolTime)
    PlayerAnim:ToRun()
    IsShootCool = false
end
