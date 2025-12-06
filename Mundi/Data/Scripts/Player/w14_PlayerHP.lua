-- Player/w14_PlayerHP.lua
-- HP 및 피격 처리 모듈

local PlayerHP = {}

--- 새 PlayerHP 인스턴스 생성
--- @param Obj userdata 플레이어 GameObject
--- @return table PlayerHP 인스턴스
function PlayerHP:new(Obj)
    local Instance = setmetatable({}, self)
    self.__index = self

    Instance.Obj = Obj
    Instance.MaxHP = 3
    Instance.CurrentHP = 3
    Instance.bInvincible = false -- 무적 상태
    Instance.bDead = false       -- 죽었는지
    Instance.InvincibleDuration = 1.5 -- 무적 시간

    return Instance
end

--- HP 초기화 (게임 시작/재시작 시 호출)
function PlayerHP:Reset()
    self.CurrentHP = self.MaxHP
    self.bInvincible = false
    self.bDead = false
end

--- 데미지 처리
--- @param damage number 받을 데미지
--- @return boolean 사망 여부 (true면 사망)
function PlayerHP:TakeDamage(damage)
    if self.bInvincible or self.bDead then
        return false
    end

    self.CurrentHP = self.CurrentHP - damage

    -- 피격 카메라 셰이크 (사망보다 약하게)
    GetCameraManager():StartCameraShake(0.15, 0.2, 0.15, 30)

    if self.CurrentHP <= 0 then
        self.CurrentHP = 0
        self.bDead = true
        return true  -- 사망
    end

    self:StartInvincibility()
    return false  -- 생존
end

--- 무적 상태 시작 (TakeDamage에서 실행)
--- 내부 코루틴으로 자동 종료
function PlayerHP:StartInvincibility()
    self.bInvincible = true

    -- 코루틴으로 무적 자동 종료
    StartCoroutine(function()
        coroutine.yield("wait_time", self.InvincibleDuration)
        self:EndInvincibility()
    end)
end

--- 무적 상태 즉시 종료 (필요시 외부에서 호출)
function PlayerHP:EndInvincibility()
    self.bInvincible = false
end

--- 현재 HP 조회
--- @return number CurrentHP 현재 HP
--- @return number MaxHP 최대 HP
function PlayerHP:GetHP()
    return self.CurrentHP, self.MaxHP
end

--- 사망 상태 확인
--- @return boolean 사망 여부
function PlayerHP:IsDead()
    return self.bDead
end

--- 무적 상태 확인
--- @return boolean 무적 여부
function PlayerHP:IsInvincible()
    return self.bInvincible
end

--- 무적 지속 시간 반환
--- @return number 무적 지속 시간 (초)
function PlayerHP:GetInvincibleDuration()
    return self.InvincibleDuration
end

return PlayerHP
