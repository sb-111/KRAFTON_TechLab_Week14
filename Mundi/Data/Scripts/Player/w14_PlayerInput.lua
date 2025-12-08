local PlayerInput = {}

function PlayerInput:new(Obj)        
    local Instance = setmetatable({}, self)
    self.__index = self

    Instance.HorizontalInput = 0
    Instance.VerticalInput = 0 -- 디버그 용
    Instance.RotateVector = Vector(0, 0, 0)
    Instance.ShootTrigger = false
    Instance.ReloadTrigger = false
    Instance.ZoomTrigger = false  -- 우클릭 줌
    Instance.DeadZone = 0.2 -- 패드 데드존
    Instance.GamepadSensitivity = 1000.0
    return Instance
end

function PlayerInput:Update(DT)
    self.HorizontalInput = 0
    self.VerticalInput = 0 -- 디버그 용
    self.ShootTrigger = false
    self.ReloadTrigger = false

    if InputManager:IsKeyDown('A') then 
        self.HorizontalInput = -1 
    elseif InputManager:IsKeyDown('D') then 
        self.HorizontalInput = 1
    elseif InputManager:IsKeyDown('S') then -- 디버그 용
        self.VerticalInput = -1 -- 디버그 용
    elseif InputManager:IsKeyDown('W') then -- 디버그 용
        self.VerticalInput = 1 -- 디버그 용
    end

    if InputManager:IsKeyPressed('R') then
        self.ReloadTrigger = true
    end

    if InputManager:IsGamepadConnected(0) then
        local stickInput = InputManager:GetGamepadLeftStickX(0)
        if math.abs(stickInput) > self.DeadZone then
            self.HorizontalInput = stickInput
        end

        local reloadValue = InputManager:GetGamepadLeftTrigger(0)
        if reloadValue > 0.5 then
            self.ReloadTrigger = true
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
    local isMouseShooting = InputManager:IsMouseButtonDown(0)
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

    -- 줌 입력 (우클릭)
    local isMouseZooming = InputManager:IsMouseButtonDown(1)
    local isGamepadZooming = false

    if InputManager:IsGamepadConnected(0) then
        if InputManager:IsGamepadButtonDown(GamepadButton.LeftShoulder, 0) then
            isGamepadZooming = true
        end
    end

    if isMouseZooming or isGamepadZooming then
        self.ZoomTrigger = true
    else
        self.ZoomTrigger = false
    end
end

return PlayerInput
