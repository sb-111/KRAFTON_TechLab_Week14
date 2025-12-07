--- MobStat 클래스
--- 몬스터의 스탯(체력, 공격력, 속도 등)을 관리하는 재사용 가능한 클래스
--- @class MobStat
--- @field move_speed number 이동 속도
--- @field max_health number 최대 체력
--- @field current_health number 현재 체력
--- @field attack_point number 공격력
--- @field attack_range number 공격 범위
--- @field is_dead boolean 사망 여부
local MobStat = {}
MobStat.__index = MobStat

--- MobStat 인스턴스를 생성합니다.
--- @return MobStat
function MobStat:new()
    local instance = {
        move_speed = 0,
        max_health = 0,
        current_health = 0,
        attack_point = 0,
        attack_range = 0,
        is_dead = false
    }
    setmetatable(instance, MobStat)
    return instance
end

--- 스탯을 초기화합니다.
--- @param move_speed number 이동 속도
--- @param max_health number 최대 체력
--- @param attack_point number 공격력
--- @param attack_range number 공격 범위
--- @return void
function MobStat:Initialize(move_speed, max_health, attack_point, attack_range)
    self.move_speed = move_speed or 0
    self.max_health = max_health or 100
    self.current_health = self.max_health
    self.attack_point = attack_point or 0
    self.attack_range = attack_range or 0
    self.is_dead = false
end

--- 스탯을 재설정합니다 (Respawn 시 사용).
--- @return void
function MobStat:Reset()
    self.current_health = self.max_health
    self.is_dead = false
end

--- 데미지를 받습니다.
--- @param damage_amount number 받을 데미지량
--- @return boolean 사망 여부
function MobStat:TakeDamage(damage_amount)
    if self.is_dead then
        return true
    end

    self.current_health = self.current_health - damage_amount

    if self.current_health <= 0 then
        self.current_health = 0
        self.is_dead = true
        return true
    end

    return false
end

--- 현재 체력 비율을 반환합니다 (0.0 ~ 1.0).
--- @return number
function MobStat:GetHealthRatio()
    if self.max_health <= 0 then
        return 0
    end
    return self.current_health / self.max_health
end

return MobStat
