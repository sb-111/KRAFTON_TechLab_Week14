local PlayerInput = {}

function PlayerInput:new(Obj)        
    local Instance = setmetatable({}, self)
    self.__index = self

    Instance.HorizontalInput = 0
    Instance.RotateVector = Vector(0, 0, 0)
    Instance.ShootTrigger = false
    Instance.DeadZone = 0.2 -- 패드 데드존
    Instance.GamepadSensitivity = 1000.0
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

    -- 회전 입력 (RotateVector)
    local MouseDelta = InputManager:GetMouseDelta()
    local RotX = MouseDelta.X
    local RotY = -MouseDelta.Y
    if InputManager:IsGamepadConnected(0) then
        local RightStickX = InputManager:GetGamepadRightStickX(0)
        local RightStickY = InputManager:GetGamepadRightStickY(0)
        
        -- 데드존 체크
        if math.abs(RightStickX) > self.DeadZone then
            -- 입력값(-1~1) * 회전속도 * 델타타임
            RotX = RotX + (RightStickX * self.GamepadSensitivity * DT)
        end
        if math.abs(RightStickY) > self.DeadZone then
            RotY = RotY + (RightStickY * self.GamepadSensitivity * DT)
        end
    end
    self.RotateVector = Vector(RotX, RotY, 0)

    -- 발사 입력
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
