--- Monster 클래스
--- @class Monster
--- @field stat MobStat 스탯 관리 객체
--- @field obj userdata 게임 오브젝트
--- @field anim_instance userdata 애니메이션 상태 머신
local MobStat = require("Monster/w14_MobStat")
local ScoreManager = require("Game/w14_ScoreManager")

local Monster = {}
Monster.__index = Monster

--- 애니메이션 재생 시간 상수
local AnimDurations = {
    IdleDuration = 3.0,
    AttackDuration = 2.6,      -- 공격 애니메이션 길이
    DamagedDuration = 2.0,     -- 피격 애니메이션 길이
    DyingDuration = 1.66
}

--- 피격 애니메이션 쿨타임 (초)
local HIT_COOLDOWN_TIME = 0.2

--- Monster 인스턴스를 생성합니다.
--- @return Monster
function Monster:new()
    local instance = {}
    instance.stat = MobStat:new()
    instance.obj = nil
    instance.anim_instance = nil
    instance.is_hit_cooldown = false
    setmetatable(instance, Monster)
    return instance
end

--- 월드에서 플레이어를 찾아 위치를 반환합니다.
--- @return userdata 플레이어 위치 (찾지 못하면 Vector(0, 0, 0))
function Monster:FindPlayerPosition()
    local playerObj = FindObjectByName("player")
    if playerObj ~= nil then
        return playerObj.Location
    end
    return Vector(0, 0, 0)
end

--- Monster를 초기화합니다 (스탯 설정 및 애니메이션 상태 머신 설정).
--- @param obj userdata 게임 오브젝트
--- @param move_speed number 이동 속도
--- @param health_point number 체력
--- @param attack_point number 공격력
--- @param attack_range number 공격 범위
--- @return void
function Monster:Initialize(obj, move_speed, health_point, attack_point, attack_range)
    if not obj then
        return
    end

    -- 스탯 초기화
    self.stat:Initialize(move_speed, health_point, attack_point, attack_range)

    self.obj = obj
    local mesh = GetComponent(self.obj, "USkeletalMeshComponent")

    if not mesh then
        return
    end

    -- 애니메이션 상태 머신 활성화 및 생성
    mesh:UseStateMachine()
    self.anim_instance = mesh:GetOrCreateStateMachine()
    if not self.anim_instance then
        return
    end

    -- 애니메이션 상태 추가
    self.anim_instance:AddState("Idle", "Data/GameJamAnim/Monster/BasicZombieIdle_mixamo.com", 1.0, true)
    self.anim_instance:AddState("Attack", "Data/GameJamAnim/Monster/BasicZombieAttack_mixamo.com", 1.0, false)
    self.anim_instance:AddState("Damaged", "Data/GameJamAnim/Monster/BasicZombieDamaged_mixamo.com", 1.0, false)
    self.anim_instance:AddState("Die", "Data/GameJamAnim/Monster/BasicZombieDying_mixamo.com", 1.0, false)

    -- Attack 상태에 사운드 노티파이 추가
    self.anim_instance:AddSoundNotify("Attack", 0.01, "AttackSound", "Data/Audio/BasicZombieAttack.wav", 10.0)
    self.anim_instance:AddSoundNotify("Idle", 0.01, "IdleSound", "Data/Audio/BasicZombieIdle.wav", 1.0)
    self.anim_instance:AddSoundNotify("Die", 0.01, "DieSound", "Data/Audio/BasicZombieDeath.wav", 100.0)

    -- 초기 상태 설정
    self.anim_instance:SetState("Idle", 0)

    self.is_hit_cooldown = false
end

--- 데미지를 받습니다.
--- @param damage_amount number 받을 데미지량
--- @return void
function Monster:GetDamage(damage_amount)
    if self.stat.is_dead then
        return
    end
    
    local died = self.stat:TakeDamage(damage_amount)

    if died then
        ScoreManager.AddKill(1)
        self.anim_instance:SetState("Die", 0.2)
        self.stat.is_dead = true
        self.obj:SetPhysicsState(false)
    else
        -- 살아있다면, 피격 쿨타임이 아닐 때만 모션 재생
        if not self.is_hit_cooldown then
            
            self.anim_instance:SetState("Damaged", 0.1)
            self.is_hit_cooldown = true

            StartCoroutine(function()
                coroutine.yield("wait_time", HIT_COOLDOWN_TIME)
                self.is_hit_cooldown = false
            end)
        end
    end
end

--- 피격 상태(Damaged 또는 Die)인지 확인합니다.
--- @return boolean
function Monster:IsDamagedState()
    return self.anim_instance:GetCurrentStateName() == "Die" or
            (self.anim_instance:GetCurrentStateName() == "Damaged" and
            self.anim_instance:GetStateTime("Damaged") < AnimDurations.DamagedDuration)
end

--- 공격 중인 상태인지 확인합니다.
--- @return boolean
function Monster:IsAttackingState()
    return self.anim_instance:GetCurrentStateName() == "Attack" and
            self.anim_instance:GetStateTime("Attack") < AnimDurations.AttackDuration
end

--- 애니메이션 상태를 업데이트합니다 (Attack, Damaged가 끝나면 Idle로 복귀).
--- @return void
function Monster:UpdateAnimationState()
    local current_state = self.anim_instance:GetCurrentStateName()
    local current_time = self.anim_instance:GetStateTime(current_state)

    -- Attack 애니메이션이 끝나면 Idle로 복귀
    if current_state == "Attack" and current_time >= AnimDurations.AttackDuration then
        self.anim_instance:SetState("Idle", 0.2)
        return
    end

    -- Damaged 애니메이션이 끝나면 Idle로 복귀
    if current_state == "Damaged" and current_time >= AnimDurations.DamagedDuration then
        self.anim_instance:SetState("Idle", 0.2)
        return
    end
end

--- Monster를 리셋합니다 (Respawn 시 사용).
--- @return void
function Monster:Reset()
    -- 스탯 리셋
    self.stat:Reset()

    -- 애니메이션 상태 리셋
    if self.anim_instance then
        self.anim_instance:SetState("Idle", 0)
    end
    self.is_hit_cooldown = false
end

--- 플레이어 위치를 확인하고 공격 범위 내에 있으면 공격합니다.
--- @return void
function Monster:CheckPlayerPositionAndAttack()
    local player_pos = self:FindPlayerPosition()
    local offset = self.obj.Location - player_pos
    local distance_squared = offset.X * offset.X + offset.Y * offset.Y
    local attack_range_squared = self.stat.attack_range * self.stat.attack_range
    local is_damaging = self:IsDamagedState()
    local is_attacking = self:IsAttackingState()

    -- 공격 범위 내에 있고, 피격 상태가 아니며, 공격 중이 아닐 때만 공격
    if (distance_squared < attack_range_squared and not is_damaging and not is_attacking) then
        self.anim_instance:SetState("Attack", 0.1)
    -- 그렇지 않으면 idle 상태로 돌아간다.
    --else
    --    self.anim_instance:SetState("Idle", 0)
    end
end

return Monster