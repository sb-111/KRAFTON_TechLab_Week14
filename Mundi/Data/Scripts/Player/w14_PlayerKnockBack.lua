-- Player/w14_PlayerKnockBack.lua
-- KnockBack (밀려남) 상태 관리 모듈

local PlayerKnockBack = {}

--- 새 PlayerKnockBack 인스턴스 생성
--- @param Obj userdata 플레이어 GameObject
--- @return table PlayerKnockBack 인스턴스
function PlayerKnockBack:new(Obj)
    local Instance = setmetatable({}, self)
    self.__index = self

    Instance.Obj = Obj
    Instance.Velocity = 0.0      -- 현재 밀려나는 속도 (Y축 방향, + = 오른쪽, - = 왼쪽)
    Instance.Friction = 8.0      -- 마찰 계수 (높을수록 빠르게 감속)
    Instance.MinVelocity = 0.01  -- 이 속도 이하면 0으로 처리

    return Instance
end

--- KnockBack 적용
--- @param direction number 밀려나는 방향 (-1 = 왼쪽, 1 = 오른쪽)
--- @param strength number 밀려나는 강도 (속도 단위)
function PlayerKnockBack:ApplyKnockBack(direction, strength)
    -- 방향에 따른 속도 추가 (여러 충돌 시 누적됨)
    self.Velocity = self.Velocity + (direction * strength)
end

--- 매 프레임 업데이트: 현재 프레임의 밀려남 양 계산 및 속도 감쇠
--- @param deltaTime number 델타 타임
--- @return number 이번 프레임에서 밀려날 거리 (Y축 이동량)
function PlayerKnockBack:Update(deltaTime)
    if math.abs(self.Velocity) < self.MinVelocity then
        self.Velocity = 0.0
        return 0.0
    end

    -- 현재 속도로 이동량 계산
    local moveAmount = self.Velocity * deltaTime

    -- 마찰에 의한 속도 감쇠 (지수 감쇠)
    local decayFactor = math.exp(-self.Friction * deltaTime)
    self.Velocity = self.Velocity * decayFactor

    return moveAmount
end

--- 현재 KnockBack 속도 반환
--- @return number 현재 속도
function PlayerKnockBack:GetVelocity()
    return self.Velocity
end

--- KnockBack 중인지 확인
--- @return boolean KnockBack 중이면 true
function PlayerKnockBack:IsKnockBacked()
    return math.abs(self.Velocity) >= self.MinVelocity
end

--- 상태 초기화
function PlayerKnockBack:Reset()
    self.Velocity = 0.0
end

--- 마찰 계수 설정
--- @param friction number 마찰 계수 (높을수록 빠르게 감속)
function PlayerKnockBack:SetFriction(friction)
    self.Friction = friction
end

return PlayerKnockBack
