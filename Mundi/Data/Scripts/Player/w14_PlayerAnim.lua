local PlayerAnim = {}

-- [정리 1] 변수 하나로 통일 및 직관적으로 변경
local AnimDurations = {
    ShootStart = 0.05, -- Start 애니메이션 길이 (이 시간 지나면 Loop로)
    ShootEnd = 0.2     -- End 애니메이션 길이 (이 시간 지나면 Run으로)
}

function PlayerAnim:new(Obj)        
    local Instance = setmetatable({}, self)
    self.__index = self
    Instance.AnimInstance = nil
    Instance.bIsShooting = false

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

            -- [정리 2] AddTransitionByName 삭제
            -- 이유: 아래 Update 함수에서 SetState 할 때 시간을 직접 넣으므로 필요 없음.
            
            -- 초기 상태
            Instance.AnimInstance:SetState("Idle", 0)
        end
    end
    return Instance
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

function PlayerAnim:Update(Delta, bIsFiring)
    if not self.AnimInstance then return end

    local CurrentState = self.AnimInstance:GetCurrentStateName()
    local CurrentTime = self.AnimInstance:GetStateTime(CurrentState)

    -- [1] 사격 버튼 누름
    if bIsFiring then
        self.bIsShooting = true 

        if CurrentState == "Run" or CurrentState == "Idle" then
            -- Start 진입 (0.1초 블렌딩)
            self.AnimInstance:SetState("ShootStart", 0.1)
        
        elseif CurrentState == "ShootStart" then
            -- 시간 지나면 Loop로 자연스럽게 전환
            if CurrentTime >= AnimDurations.ShootStart then
                self.AnimInstance:SetState("ShootLoop", 0.05)
            end
        
        elseif CurrentState == "ShootEnd" then
            -- 캔슬 사격
            self.AnimInstance:SetState("ShootLoop", 0.05)
        end
        -- Loop 상태는 건드리지 않음 (Fire 함수가 처리)

    -- [2] 사격 버튼 뗌
    else
        if CurrentState == "ShootStart" or CurrentState == "ShootLoop" then
            -- 손 떼면 End 재생
            self.AnimInstance:SetState("ShootEnd", 0.1)

        elseif CurrentState == "ShootEnd" then
            -- End 시간 다 됐으면 Run으로 복귀
            if CurrentTime >= AnimDurations.ShootEnd then
                self.bIsShooting = false
                self:ToRun() 
            end

        elseif CurrentState == "Idle" then
            -- 게임 시작 시 Run
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

return PlayerAnim
