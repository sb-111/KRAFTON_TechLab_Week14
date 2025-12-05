--- @class PlayerController
--- @field MovementDelta number 이동 속도 배율
--- @field player userdata 추적할 플레이어 GameObject
--- @field ForwardVector userdata 전방 벡터 (1, 0, 0)
--- @field RightVector userdata 우측 벡터 (0, 1, 0)
--- @field UpVector userdata 상방 벡터 (0, 0, 1)
local PlayerController = {}
PlayerController.MovementDelta = 10
PlayerController.player = nil
PlayerController.ForwardVector = Vector(1, 0, 0)
PlayerController.RightVector = Vector(0, 1, 0)
PlayerController.UpVector = Vector(0, 0, 1)

--- 플레이어 GameObject 설정
--- @param player userdata 추적할 플레이어 GameObject
--- @return void
function PlayerController:set_player_to_trace(player)
    self.player = player
end

--- 우측 방향으로 이동
--- @param Delta number 이동량 (음수면 좌측)
--- @return void
function PlayerController:MoveRight(Delta)
    self.player.Location = self.player.Location + self.RightVector * Delta
end

--- 전방 방향으로 이동
--- @param Delta number 이동량 (음수면 후방)
--- @return void
function PlayerController:MoveForward(Delta)
    self.player.Location = self.player.Location + self.ForwardVector * Delta
end

--- 카메라 위치를 플레이어 뒤에 배치
--- @return void
function PlayerController:SetCamera()
    if not self.player then
        return
    end

    local BackDistance = 25.0
    local UpDistance   = 10.0

    local Camera = GetCamera()
    if Camera then
        local CameraLocation = self.player.Location + (self.ForwardVector * -BackDistance) + (self.UpVector * UpDistance)
        Camera:SetLocation(CameraLocation)
    end
end

--- 입력에 따른 플레이어 제어
--- @param Delta number 프레임 시간 (deltaTime)
--- @return void
function PlayerController:Control(Delta)
    self:SetCamera()
    
    --- 이 게임에서는 기본적으로 앞으로 이동
    --- if InputManager:IsKeyDown('W') then self:MoveForward(self.MovementDelta * Delta) end
    --- if InputManager:IsKeyDown('S') then self:MoveForward(-self.MovementDelta * Delta) end
    self:MoveForward(self.MovementDelta * Delta)
    if InputManager:IsKeyDown('A') then self:MoveRight(-self.MovementDelta * Delta) end
    if InputManager:IsKeyDown('D') then self:MoveRight(self.MovementDelta * Delta) end

    --- 현재 사용하지 않는 조작이므로 보류
    --if InputManager:IsKeyPressed('Q') then Die() end -- 죽기를 선택
    --if InputManager:IsKeyPressed(' ') then Jump(MovementDelta) end
    --if InputManager:IsMouseButtonPressed(0) then ShootProjectile() end
end

return PlayerController