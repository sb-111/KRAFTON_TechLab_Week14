local PlayerAnim = {}

-- [정리 1] 변수 하나로 통일 및 직관적으로 변경
local AnimDurations = {
    ShootStart = 0.05, -- Start 애니메이션 길이 (이 시간 지나면 Loop로)
    ShootEnd = 0.2,     -- End 애니메이션 길이 (이 시간 지나면 Run으로)
    Reload = 1.5
}

function PlayerAnim:new(Obj)        
    local Instance = setmetatable({}, self)
    self.__index = self
    Instance.AnimInstance = nil
    Instance.bIsShooting = false
    Instance.bIsReloading = false

    local Mesh = GetComponent(Obj, "USkeletalMeshComponent")
    if Mesh then
        Instance.AnimInstance = Mesh:GetOrCreateStateMachine()
        if Instance.AnimInstance then
            -- [상태 등록]
            Instance.AnimInstance:AddState("Idle", "Data/GameJamAnim/Player/Rifle_Idle_mixamo.com", 1.0, true)
            Instance.AnimInstance:AddState("Run", "Data/GameJamAnim/Player/Rifle_Run_mixamo.com", 1.0, true)
            
            -- FPS 사격 (Loop 속도 1.0이 적당, 너무 빠르면 재봉틀)
            Instance.AnimInstance:AddState("ShootStart", "Data/GameJamAnim/Player/Rifle_ShootStart_mixamo.com", 1.5, false)
            Instance.AnimInstance:AddState("ShootLoop", "Data/GameJamAnim/Player/Rifle_ShootLoop_mixamo.com", 1.0, true)
            Instance.AnimInstance:AddState("ShootEnd", "Data/GameJamAnim/Player/Rifle_ShootEnd_mixamo.com", 1.5, false)
            Instance.AnimInstance:AddState("Reload", "Data/GameJamAnim/Player/Rifle_Reload_mixamo.com", 1.0, false)

            Instance.AnimInstance:AddSoundNotify("Run", 0.01, "RunSound", "Data/Audio/PlayerFootstep.wav", 10.0)
            Instance.AnimInstance:AddSoundNotify("Run", 0.35, "RunSound", "Data/Audio/PlayerFootstep.wav", 10.0)
            Instance.AnimInstance:AddSoundNotify("Run", 0.35, "BreathingSound", "Data/Audio/PlayerHeavyBreathing.wav", 100.0)
            
            -- 초기 상태
            Instance.AnimInstance:SetState("Idle", 0)
        end
    end
    return Instance
end

function PlayerAnim:Reset()
    -- 초기 상태
    self.AnimInstance:SetState("Idle", 0)
    self.bIsShooting = false
    self.bIsReloading = false
end

function PlayerAnim:StartReload()
    if not self.AnimInstance then return end
    
    local CurrentState = self.AnimInstance:GetCurrentStateName()
    
    -- 사격 중이면 먼저 End로 전환
    if CurrentState == "ShootStart" or CurrentState == "ShootLoop" then
        self.AnimInstance:SetState("ShootEnd", 0.05)
    end
    
    -- Reload 상태로 진입
    self.bIsReloading = true
    self.AnimInstance:SetState("Reload", 0.1)
end

function PlayerAnim:CancelReload()
    if not self.AnimInstance or not self.bIsReloading then return end
    
    self.bIsReloading = false
    self:ToRun()
end

function PlayerAnim:Fire()
    if not self.AnimInstance then return end
    
    local CurrentState = self.AnimInstance:GetCurrentStateName()
    
    -- 사격 관련 상태일 때만 반동 적용 (Run 상태에서 호출 방지)
    if CurrentState == "ShootStart" or CurrentState == "ShootLoop" or CurrentState == "ShootEnd" then
        -- 시간 0으로 설정하여 강제 리셋 (파파파팍!)
        self.AnimInstance:SetState("ShootLoop", 0) 
    end
end

function PlayerAnim:Update(Delta, bIsFiring, bCanFire)
    if not self.AnimInstance then return end

    local CurrentState = self.AnimInstance:GetCurrentStateName()
    local CurrentTime = self.AnimInstance:GetStateTime(CurrentState)

    if CurrentState == "Reload" then
        if CurrentTime >= AnimDurations.Reload then
            self.bIsReloading = false
            self:ToRun()
        end
        return
    end

     if self.bIsReloading then
        return
    end

    -- ShootEnd는 항상 처리
    if CurrentState == "ShootEnd" then
        if CurrentTime >= AnimDurations.ShootEnd then
            self.bIsShooting = false
            self:ToRun()
        end
        return
    end

    -- 사격 중이면 무조건 End로 전환 (탄약 관계없이)
    if not bIsFiring and (CurrentState == "ShootStart" or CurrentState == "ShootLoop") then
        self.AnimInstance:SetState("ShootEnd", 0.1)
        return
    end

    -- 탄약 부족하면 사격 상태 진입 불가
    if not bCanFire then
        if CurrentState == "ShootStart" or CurrentState == "ShootLoop" then
            self.AnimInstance:SetState("ShootEnd", 0.1)  -- 즉시 End로!
        elseif CurrentState == "Idle" then
            self:ToRun()
        end
        return
    end

    -- [1] 사격 버튼 누름 (탄약 있을 때만)
    if bIsFiring then
        self.bIsShooting = true

        if CurrentState == "Run" or CurrentState == "Idle" then
            self.AnimInstance:SetState("ShootStart", 0.1)
        
        elseif CurrentState == "ShootStart" then
            if CurrentTime >= AnimDurations.ShootStart then
                self.AnimInstance:SetState("ShootLoop", 0.05)
            end
        end

    -- [2] 사격 버튼 뗌
    else
        if CurrentState == "Idle" then
            self:ToRun()
        
        elseif CurrentState == "Run" then
            self.bIsShooting = false
        end
    end
end

function PlayerAnim:ToRun()
    local CurrentState = self.AnimInstance:GetCurrentStateName()
    if CurrentState ~= "Run" then
        self.AnimInstance:SetState("Run", 0.2)
    end
end

function PlayerAnim:IsShootingPlaying() return self.bIsShooting end
function PlayerAnim:IsReloading() return self.bIsReloading end

return PlayerAnim
