--[[
    w14_BossMonster.lua

    보스 몬스터 클래스
    - 플레이어 주변을 떠다니며(Floating) 일정 쿨타임마다 공격(Attack)
    - 이동: 플레이어 위치 기준 랜덤 오프셋 방향으로 이동
    - 공격: 쿨타임 기반 원거리 공격 (발사체 미구현)
]]

local MobStat = require("Monster/w14_MobStat")
local Math = require("w14_Math")
local Audio = require("Game/w14_AudioManager")

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
--- @field projectile_count number 한 번 공격 시 발사체 개수 (발사체는 자체 Tick에서 처리)
--- @field projectile_speed number 발사체 이동 속도
local BossMonster = {}
BossMonster.__index = BossMonster
BossMonster.Const = Const

--- 애니메이션 재생 시간 상수 (초)
local AnimDurations = {
    FloatingDuration = 3.66,    -- Floating 애니메이션 길이
    AttackDuration = 2.3,       -- Attack 애니메이션 길이
    BeamBreathDuration = 4.0   -- 빔 공격 애니메이션 길이
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
    instance.projectile_count = 1       -- 기본 발사체 개수
    instance.projectile_speed = 8       -- 기본 발사체 속도

    -- 빔 파티클 컴포넌트 캐싱
    instance.beam_warning_particle = nil
    instance.beam_particle = nil

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
                instance.anim_instance:AddState("BeamBreath", "Data/GameJamAnim/Monster/BossBeamBreath_mixamo.com", 1.0, false)

                instance.anim_instance:AddSoundNotify("Attack", 0.01, "AttackSound", "Data/Audio/BossAttack.wav", 10.0, 40.0)
                instance.anim_instance:SetState("Floating", 0)
            end
        end

        -- 파티클 컴포넌트 가져오기 (GetComponents로 배열 가져와서 필터링)
        local particle_comps = GetComponents(obj, "UParticleSystemComponent")
        if particle_comps and type(particle_comps) == "table" then
            for i = 1, #particle_comps do
                local comp = particle_comps[i]
                if comp and comp.ObjectName then
                    local name = tostring(comp.ObjectName)
                    if name and string.find(name, "Warning") then
                        instance.beam_warning_particle = comp
                    elseif name and string.find(name, "BossBeamBreath") and not string.find(name, "Warning") then
                        instance.beam_particle = comp
                    end
                end
            end
        end
    end

    return instance
end

--- BossMonster 스탯을 초기화합니다.
--- @param health_point number 체력
--- @param attack_point number 공격력
--- @param projectile_count number 발사체 개수 (옵션)
--- @return void
function BossMonster:Initialize(health_point, attack_point, projectile_count)
    -- 스탯 초기화 (move_speed는 실시간 갱신, attack_range는 원거리라 불필요)
    self.stat:Initialize(0, health_point, attack_point, 0)

    -- 발사체 개수 설정
    if projectile_count then
        self.projectile_count = projectile_count
    end
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
        -- RagDoll 처리 (물리 시뮬레이션 활성화)
        self.stat.is_dead = true
        local mesh = GetComponent(self.obj, "USkeletalMeshComponent")
        if mesh then
            mesh:SetSimulatePhysics(true)
        end
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

--- 발사체를 스폰합니다.
--- 발사체는 자체적으로 플레이어 추적 및 파괴를 처리합니다.
--- @return void
function BossMonster:SpawnProjectiles()
    local boss_pos = self.obj.Location
    local count = self.projectile_count

    -- 발사체 간 이격 거리 설정
    local spacing_y = 3.0  -- Y축 간격
    local spacing_z = 2.0  -- Z축 간격

    -- 발사체들의 총 범위 계산 (중앙 기준 대칭 배치)
    local total_spread_y = spacing_y * (count - 1)
    local start_offset_y = -total_spread_y / 2

    for i = 1, count do
        -- 균등 이격 배치 (Y축 기준)
        local offset_y = start_offset_y + spacing_y * (i - 1)
        -- Z축은 약간의 랜덤 변화 추가
        local offset_z = Math:RandomInRange(-0.5, 0.5)
        local spawn_pos = boss_pos + Vector(-2, offset_y, offset_z)

        local projectile = SpawnPrefab("Data/Prefabs/w14_BossProjectile.prefab")
        if projectile then
            projectile.Location = spawn_pos

            local proj_script = projectile:GetScript()
            if proj_script and proj_script.Initialize then
                proj_script.Initialize(self.projectile_speed, self.stat.attack_point)
            end
        end
    end
end

--- 매 프레임 공격 쿨타임 체크 및 공격 실행
--- @param delta number 델타 타임
--- @return void
function BossMonster:Attack()
    -- 발사체 소환
    self:SpawnProjectiles()
    self.anim_instance:SetState("Attack", 0)
end

--- 빔 공격 타겟 위치를 계산합니다 (플레이어 전방 + 랜덤 오프셋)
--- @return userdata 타겟 위치 벡터
function BossMonster:CalculateBeamTargetPosition()
    local player_pos = self:FindPlayerPosition()

    -- 플레이어 전방 예측 (플레이어가 앞으로 달린다고 가정)
    local forward_prediction = 8.0  -- 전방 예측 거리

    -- 랜덤 오프셋 추가
    local random_x = Math:RandomInRange(3.0, 5.0)   -- 전후 랜덤
    local random_y = Math:RandomInRange(-1.5, 1.5)   -- 좌우 랜덤
    local random_z = 0  -- 바닥에 맞춤

    local target_pos = Vector(
        player_pos.X + forward_prediction + random_x,
        player_pos.Y + random_y,
        player_pos.Z + random_z
    )

    return target_pos
end

--- 빔 공격 특수 패턴
--- 경고 파티클 -> 0.5초 후 빔 공격 파티클 + 히트박스 소환
--- @return void
function BossMonster:BeamBreath()
    self.anim_instance:SetState("BeamBreath", 0)

    -- 캐싱된 파티클 컴포넌트 사용
    local warning_particle = self.beam_warning_particle
    local beam_particle = self.beam_particle

    if not warning_particle or not beam_particle then
        print("[BossMonster] BeamBreath: Particle components not found")
        return
    end

    -- 타겟 위치 계산
    local target_pos = self:CalculateBeamTargetPosition()

    -- 두 파티클의 타겟 위치 설정
    warning_particle.BeamTargetPosition = target_pos
    beam_particle.BeamTargetPosition = target_pos

    -- 경고 파티클 활성화
    warning_particle.bIsActive = true

    -- 경고 음성 재생
    Audio.PlaySFX("BossBeamBreathWarning")

    -- 코루틴으로 타이밍 처리
    StartCoroutine(function()
        -- 0.5초 대기 (경고 표시)
        coroutine.yield("wait_time", 1)

        -- 경고 파티클 비활성화
        warning_particle.bIsActive = false

        -- 빔 파티클 활성화
        beam_particle.bIsActive = true

        -- 히트박스 소환 및 공격력 전달
        local hitbox = SpawnPrefab("Data/Prefabs/w14_BossBeamBreathHitBox.prefab")
        if hitbox then
            hitbox.Location = target_pos

            -- 히트박스에 보스 공격력 전달
            local hitbox_script = hitbox:GetScript()
            if hitbox_script and hitbox_script.Initialize then
                hitbox_script.Initialize(self.stat.attack_point)
            end
        end

        -- 빔 효과 음성
        Audio.PlaySFX("BossBeamBreath")

        -- 빔 지속 시간 대기
        coroutine.yield("wait_time", 2.0)

        -- 빔 파티클 비활성화
        beam_particle.bIsActive = false
    end)
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
    if (current_state == "Attack" and current_time >= AnimDurations.AttackDuration) or
            (current_state == "BeamBreath" and current_time >= AnimDurations.BeamBreathDuration) then
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
    
    self.attack_cool_time = self.attack_cool_time + delta
    if (self.attack_cool_time > self.next_attack_time) then
        local PatternNum = Math:RandomIntInRange(0, 1)

        if PatternNum == 0 then
            -- 발사체 소환
            self:Attack()           -- 공격 쿨타임 체크 및 공격
        elseif PatternNum == 1 then
            self:BeamBreath()
        end

        self.attack_cool_time = 0
        self:GetNextAttackCooltime()
    end
end

return BossMonster