local PlayerAnimClass = require("Player/w14_PlayerAnim")
local PlayerInputClass = require("Player/w14_PlayerInput")
local PlayerAnim = nil
local PlayerInput = nil
local Gun = nil
local Mesh = nil

local MovementSpeed = 10.0

function BeginPlay()
    Mesh = GetComponent(Obj, "USkeletalMeshComponent")
    Gun = GetComponent(Obj, "UStaticMeshComponent")

    Gun:SetupAttachment(Mesh, "mixamorig6:RightHand")

    PlayerAnim = PlayerAnimClass:new(Obj)
    PlayerInput = PlayerInputClass:new(Obj)
    StartCoroutine(ToRun)
end

function Tick(Delta)
    PlayerInput:Update(Delta)

    if math.abs(PlayerInput.HorizontalInput) > 0 then
        local MoveAmount = PlayerInput.HorizontalInput * MovementSpeed * Delta
        Obj.Location = Obj.Location + Vector(0, MoveAmount, 0)
    end

    if PlayerInput.ShootTrigger and not PlayerAnim:IsAnimPlaying("Shoot") then
        Shoot()
    elseif PlayerAnim:IsShootingPlaying() then
        PlayerAnim:ToRun()
    end
end

function ToRun()
    coroutine.yield("wait_time", 1)
    PlayerAnim:ToRun()
end

function Shoot()
    PlayerAnim:ToShoot()
end
