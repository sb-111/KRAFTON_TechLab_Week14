local PlayerInput = {}

function PlayerInput:new(Obj)        
    local Instance = setmetatable({}, self)
    self.__index = self

    Instance.HorizontalInput = 0
    Instance.ShootTrigger = false
    Instance.DeadZone = 0.2 -- 패드 데드존
    return Instance
end

function PlayerInput:Update(DT)
    self.HorizontalInput = 0
    self.ShootTrigger = false

    if InputManager:IsKeyDown('A') then 
        self.HorizontalInput = -1 
    elseif InputManager:IsKeyDown('D') then 
        self.HorizontalInput = 1 
    end

    if InputManager:IsGamepadConnected(0) then
        local stickInput = InputManager:GetGamepadLeftStickX(0)
        if math.abs(stickInput) > self.DeadZone then
            self.HorizontalInput = stickInput
        end
    end

    local isMouseShooting = InputManager:IsMouseButtonPressed(0)
    local isGamepadShooting = false

    if InputManager:IsGamepadConnected(0) then
        local triggerValue = InputManager:GetGamepadRightTrigger(0)
        if triggerValue > 0.5 then
            isGamepadShooting = true
        end
    end

    if isMouseShooting or isGamepadShooting then
        self.ShootTrigger = true
    else
        self.ShootTrigger = false
    end
end

return PlayerInput
