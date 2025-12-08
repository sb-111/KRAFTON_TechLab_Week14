--[[
    w14_BossMonster.lua

    보스 몬스터 클래스
    - 플레이어 주변을 떠다니며(Floating) 일정 쿨타임마다 공격(Attack)
    - 이동: 플레이어 위치 기준 랜덤 오프셋 방향으로 이동
    - 공격: 쿨타임 기반 원거리 공격 (발사체 미구현)
]]

local MobStat = require("Monster/w14_MobStat")
local Math = require("w14_Math")

---------------------------------------------------------------------------
-- 상수 정의 (불변 테이블)
---------------------------------------------------------------------------
local Const = {}

local ConstValues = {
    -- 이동 오프셋 범위 (플레이어 기준 상대 위치)
    MOVE_OFFSET_X_MIN = 20,     -- X축 최소 오프셋 (플레이어 앞쪽)
    MOVE_OFFSET_X_MAX = 30,     -- X축 최대 오프셋
    MOVE_OFFSET_Y_MIN = -8,    -- Y축 최소 오프셋 (좌우)
    MOVE_OFFSET_Y_MAX = 8,     -- Y축 최대 오프셋
    MOVE_OFFSET_Z_MIN = 5,     -- Z축 최소 오프셋 (높이)
    MOVE_OFFSET_Z_MAX = 10,     -- Z축 최대 오프셋

    -- 이동 속도 범위
    MIN_SPEED = 15,
    MAX_SPEED = 15,

    -- 공격 쿨타임 범위 (초)
    MIN_ATTACK_COOLTIME = 3,
    MAX_ATTACK_COOLTIME = 10
}

-- 상수 테이블 불변 처리
ConstValues.__index = ConstValues
ConstValues.__newindex = function(_, key, value)
    print("[error] Attempt to modify constant [%s] with value [%s]", key, tostring(value))
end
setmetatable(Const, ConstValues)

---------------------------------------------------------------------------
-- BossMonster 클래스 정의
---------------------------------------------------------------------------
--- @class BossMonster
--- @field stat MobStat 스탯 관리 객체
--- @field obj userdata 게임 오브젝트
--- @field anim_instance userdata 애니메이션 상태 머신
--- @field next_dir userdata 현재 이동 방향 벡터
--- @field movement_time number 현재 이동 경과 시간
--- @field movement_time_total number 목표 지점까지 총 이동 시간
--- @field attack_cool_time number 현재 공격 쿨타임 경과
--- @field next_attack_time number 다음 공격까지 필요한 시간
local BossMonster = {}
BossMonster.__index = BossMonster
BossMonster.Const = Const

--- 애니메이션 재생 시간 상수 (초)
local AnimDurations = {
    FloatingDuration = 3.66,    -- Floating 애니메이션 길이
    AttackDuration = 2.3,       -- Attack 애니메이션 길이
    -- DamagedDuration = 2.0    -- 피격 애니메이션 길이 (미사용)
}

--- BossMonster 인스턴스를 생성합니다 (obj + 애니메이션 초기화 포함).
--- @param obj userdata 게임 오브젝트
--- @return BossMonster
function BossMonster:new(obj)
    local instance = {}
    instance.stat = MobStat:new()
    instance.obj = obj
    
    -- 초기 위치 지정
    obj.Location = self:FindPlayerPosition() + Vector(10, 0, 10)
    
    instance.anim_instance = nil
    instance.next_dir = Vector(1, 0, 0)         -- 초기 이동 방향 (nil 방지)
    instance.movement_time = 0
    instance.movement_time_total = 0
    instance.attack_cool_time = 0
    instance.next_attack_time = Const.MIN_ATTACK_COOLTIME  -- 초기 쿨타임 설정 (즉시 공격 방지)
    setmetatable(instance, BossMonster)

    -- 애니메이션 상태 머신 초기화
    if obj then
        local mesh = GetComponent(obj, "USkeletalMeshComponent")
        if mesh then
            mesh:UseStateMachine()
            instance.anim_instance = mesh:GetOrCreateStateMachine()
            if instance.anim_instance then
                instance.anim_instance:AddState("Floating", "Data/GameJamAnim/Monster/BossFloating_mixamo.com", 1.0, true)
                instance.anim_instance:AddState("Attack", "Data/GameJamAnim/Monster/BossAttack_mixamo.com", 1.0, false)
                instance.anim_instance:AddSoundNotify("Attack", 0.01, "AttackSound", "Data/Audio/BossAttack.wav", 10.0)
                instance.anim_instance:SetState("Floating", 0)
            end
        end
    end

    return instance
end

--- BossMonster 스탯을 초기화합니다.
--- @param health_point number 체력
--- @param attack_point number 공격력
--- @return void
function BossMonster:Initialize(health_point, attack_point)
    -- 스탯 초기화 (move_speed는 실시간 갱신, attack_range는 원거리라 불필요)
    self.stat:Initialize(0, health_point, attack_point, 0)
end

--- 데미지를 받습니다.
--- @param damage_amount number 받을 데미지량
--- @return void
function BossMonster:GetDamage(damage_amount)
    if self.stat.is_dead then
        return
    end

    local died = self.stat:TakeDamage(damage_amount)

    if died then
        -- RagDoll 처리
        self.stat.is_dead = true
        self.obj:SetPhysicsState(false)
    end
end

--- 월드에서 플레이어를 찾아 위치를 반환합니다.
--- @return userdata 플레이어 위치 (찾지 못하면 Vector(0, 0, 0))
function BossMonster:FindPlayerPosition()
    local playerObj = FindObjectByName("player")
    if playerObj ~= nil then
        return playerObj.Location
    end
    return Vector(0, 0, 0)
end

---------------------------------------------------------------------------
-- 이동 관련 함수
---------------------------------------------------------------------------

--- 다음 이동 속도를 랜덤하게 설정합니다.
--- @return void
function BossMonster:GetNextSpeed()
    self.stat.move_speed = Math:RandomInRange(Const.MIN_SPEED, Const.MAX_SPEED)
end

--- 다음 이동 목표 위치와 방향을 계산합니다.
--- 플레이어 위치 기준으로 랜덤 오프셋을 더해 목표 위치 설정
--- @return void
function BossMonster:GetNextPosition()
    self.movement_time = 0
    self:GetNextSpeed()

    -- 플레이어 기준 랜덤 오프셋 계산
    local next_x_offset = Math:RandomInRange(Const.MOVE_OFFSET_X_MIN, Const.MOVE_OFFSET_X_MAX)
    local next_y_offset = Math:RandomInRange(Const.MOVE_OFFSET_Y_MIN, Const.MOVE_OFFSET_Y_MAX)
    local next_z_offset = Math:RandomInRange(Const.MOVE_OFFSET_Z_MIN, Const.MOVE_OFFSET_Z_MAX)

    local player_pos = self:FindPlayerPosition()
    local offset = Vector(next_x_offset, next_y_offset, next_z_offset)

    -- 플레이어 위치 + 오프셋 = 목표 위치
    local target_position = player_pos + offset

    -- 현재 위치에서 목표 위치까지의 방향과 거리 계산
    local direction = target_position - self.obj.Location
    local distance = Math:length_vector(direction)

    -- 이동 방향과 총 이동 시간 계산
    if distance > 0 then
        self.next_dir = Math:normalize_vector(direction)
        self.movement_time_total = distance / self.stat.move_speed
    else
        self.next_dir = Vector(1, 0, 0)
        self.movement_time_total = 0
    end
end

--- 매 프레임 이동 처리
--- 목표 시간에 도달하면 새로운 목표 위치 계산
--- @param delta number 델타 타임
--- @return void
function BossMonster:Move(delta)
    self.movement_time = self.movement_time + delta

    -- 목표 시간 초과 시 새로운 목표 설정
    if (self.movement_time > self.movement_time_total) then
        self:GetNextPosition()
    end

    -- 방향 * 속도 * 시간으로 이동
    self.obj.Location = self.obj.Location + self.next_dir * self.stat.move_speed * delta
end

---------------------------------------------------------------------------
-- 공격 관련 함수
---------------------------------------------------------------------------

--- 다음 공격 쿨타임을 랜덤하게 설정합니다.
--- @return void
function BossMonster:GetNextAttackCooltime()
    self.next_attack_time = Math:RandomInRange(Const.MIN_ATTACK_COOLTIME, Const.MAX_ATTACK_COOLTIME)
end

--- 매 프레임 공격 쿨타임 체크 및 공격 실행
--- @param delta number 델타 타임
--- @return void
function BossMonster:Attack(delta)
    self.attack_cool_time = self.attack_cool_time + delta

    if (self.attack_cool_time > self.next_attack_time) then
        -- TODO: 원거리 발사체 소환 코드 삽입
        self.anim_instance:SetState("Attack", 0)
        self.attack_cool_time = 0
        self:GetNextAttackCooltime()
    end
end

---------------------------------------------------------------------------
-- 애니메이션 상태 관련 함수
---------------------------------------------------------------------------

--- 공격 중인 상태인지 확인합니다.
--- @return boolean Attack 애니메이션 재생 중이면 true
function BossMonster:IsAttackingState()
    return self.anim_instance:GetCurrentStateName() == "Attack" and
            self.anim_instance:GetStateTime("Attack") < AnimDurations.AttackDuration
end

--- 애니메이션 상태를 업데이트합니다.
--- Attack 애니메이션이 끝나면 Floating으로 복귀
--- @return void
function BossMonster:UpdateAnimationState()
    local current_state = self.anim_instance:GetCurrentStateName()
    local current_time = self.anim_instance:GetStateTime(current_state)

    -- Attack 애니메이션이 끝나면 Floating으로 복귀
    if current_state == "Attack" and current_time >= AnimDurations.AttackDuration then
        self.anim_instance:SetState("Floating", 0.2)
        return
    end
end

---------------------------------------------------------------------------
-- 메인 업데이트 함수
---------------------------------------------------------------------------

--- 매 프레임 호출되는 업데이트 함수
--- @param delta number 델타 타임
--- @return void
function BossMonster:Tick(delta)
    -- 사망 시 또는 초기화 안됨 시 업데이트 중지
    if self.stat.is_dead or not self.anim_instance then
        return
    end

    self:UpdateAnimationState()  -- 애니메이션 상태 전환 체크
    self:Move(delta)             -- 이동 처리
    self:Attack(delta)           -- 공격 쿨타임 체크 및 공격
end

return BossMonster