local PlayerAnim = {}

function PlayerAnim:new(Obj)        
    local Instance = setmetatable({}, self)
    self.__index = self

    local Mesh = GetComponent(Obj, "USkeletalMeshComponent")
    if Mesh then
        Instance.AnimInstance = Mesh:GetOrCreateStateMachine()
        if Instance.AnimInstance then
            Instance.AnimInstance:AddState("Idle", "Data/GameJamAnim/Player/Rifle_Idle_mixamo.com", 1.0, true)
            Instance.AnimInstance:AddState("Run", "Data/GameJamAnim/Player/Rifle_Run_mixamo.com", 1.0, true)
            Instance.AnimInstance:AddState("Shoot", "Data/GameJamAnim/Player/Rifle_Shoot_mixamo.com", 1.0, false)
            Instance.AnimInstance:AddTransitionByName("Idle", "Run", 0.2)
            Instance.AnimInstance:AddTransitionByName("Run", "Shoot", 0)
            Instance.AnimInstance:SetState("Idle", 0)
        end
    end
    return Instance
end

function PlayerAnim:ToRun()
    if self.AnimInstance then
        self.AnimInstance:SetState("Run", 0.2)
    end
end

function PlayerAnim:ToShoot()
    if self.AnimInstance then
        local AnimTime = self.AnimInstance:GetStateTime("Run")
        self.AnimInstance:SetState("Shoot", 0.2)
        --self.AnimInstance:SetStateTime("Shoot", AnimTime)
    end
end

function PlayerAnim:IsAnimPlaying(StateName)
    if self.AnimInstance then
        return self.AnimInstance:GetCurrentStateName() == StateName
    end
    return false
end

function PlayerAnim:IsShootingPlaying()
    return self.AnimInstance:GetStateTime("Shoot") < 1.0
end
return PlayerAnim
 