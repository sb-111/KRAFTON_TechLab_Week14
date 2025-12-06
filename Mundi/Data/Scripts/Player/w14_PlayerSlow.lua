-- Player/w14_PlayerSlow.lua
-- 속도 감소 상태 관리 모듈

local PlayerSlow = {}

--- 새 PlayerSlow 인스턴스 생성
--- @param Obj userdata 플레이어 GameObject
--- @return table PlayerSlow 인스턴스
function PlayerSlow:new(Obj)
    local Instance = setmetatable({}, self)
    self.__index = self

    Instance.Obj = Obj
    Instance.SpeedMultiplier = 1.0  -- 현재 속도 배율 (1.0 = 정상)
    Instance.bSlowed = false        -- 감속 상태인지

    return Instance
end

--- 속도 감소 적용
--- @param speedMult number 속도 배율 (0.5 = 50% 속도)
--- @param duration number 지속 시간 (초)
function PlayerSlow:ApplySlow(speedMult, duration)
    -- 이미 감속 중이면 더 강한 감속으로 덮어쓰기
    if self.bSlowed and speedMult >= self.SpeedMultiplier then
        return  -- 현재 감속이 더 강하면 무시
    end

    self.SpeedMultiplier = speedMult
    self.bSlowed = true

    -- 카메라 셰이크 (충돌 피드백)
    GetCameraManager():StartCameraShake(0.15, 0.2, 0.15, 30)

    -- 코루틴으로 자동 복구
    local selfRef = self
    local restoreDuration = duration
    StartCoroutine(function()
        coroutine.yield("wait_time", restoreDuration)
        selfRef:EndSlow()
    end)
end

--- 속도 감소 종료
function PlayerSlow:EndSlow()
    self.SpeedMultiplier = 1.0
    self.bSlowed = false
end

--- 현재 속도 배율 반환 (Player 이동 시 사용)
--- @return number 속도 배율 (1.0 = 정상, 0.5 = 50%)
function PlayerSlow:GetSpeedMultiplier()
    return self.SpeedMultiplier
end

--- 감속 상태인지 확인
--- @return boolean 감속 중이면 true
function PlayerSlow:IsSlowed()
    return self.bSlowed
end

--- 상태 초기화
function PlayerSlow:Reset()
    self.SpeedMultiplier = 1.0
    self.bSlowed = false
end

return PlayerSlow
