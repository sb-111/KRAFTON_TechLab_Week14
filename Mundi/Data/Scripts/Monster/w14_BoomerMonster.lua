local MobStat = require("Monster/w14_MobStat")
local MonsterUtil = require("Monster/w14_MonsterUtil")
local ScoreManager = require("Game/w14_ScoreManager")

local EXPLOSION_PREFAB = "Data/Prefabs/w14_BoomerExplosion.prefab"
local EXPLOSION_CRATER_PREFAB = "Data/Prefabs/w14_BoomerBloodDecal.prefab"

--- BoomerMonster 클래스
--- @class BoomerMonster
--- @field stat MobStat 스탯 관리 객체
--- @field obj userdata 게임 오브젝트
--- @field anim_instance userdata 애니메이션 상태 머신

local BoomerMonster = {}
BoomerMonster.__index = BoomerMonster

--- 애니메이션 재생 시간 상수
local AnimDurations = {
    IdleDuration = 2.0,
    WalkDuration = 2.0,     -- 실측 후 변경 필요
    BoomDuration = 1.0,     -- 공격 애니메이션 길이
    DamagedDuration = 2.0,   -- 피격 애니메이션 길이
    DyingDuration = 3.33
}

--- 피격 애니메이션 쿨타임 (초)
local HIT_COOLDOWN_TIME = 0.2

--- BoomerMonster 인스턴스 생성
--- @return BoomerMonster
function BoomerMonster:new()
    local instance = {}
    
    instance.stat = MobStat:new()
    instance.obj = nil
    instance.anim_instance = nil
    instance.is_idle = false  -- 장애물/몬스터 충돌로 인한 Idle 상태

    setmetatable(instance, BoomerMonster)
    return instance
end 

--- BoomerMonster 초기화
function BoomerMonster:Initialzie(obj, move_speed, health_point, attack_point, attack_range)
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
    self.anim_instance:AddState("Idle", "Data/GameJamAnim/Monster/BoomerZombieIdle_mixamo.com", 1.0, true)
    self.anim_instance:AddState("Walk", "Data/GameJamAnim/Monster/BoomerZombieWalk_mixamo.com", 3.0, true)
    self.anim_instance:AddState("Boom", "Data/GameJamAnim/Monster/BoomerZombieBoom_mixamo.com", 1.0, false)
    self.anim_instance:AddState("Die", "Data/GameJamAnim/Monster/BoomerZombieDying_mixamo.com", 1.0, false)

    -- 사운드 노티파이 추가 (마지막 파라미터: MaxDistance, 0이면 무제한)
    self.anim_instance:AddSoundNotify("Idle", 0.01, "IdleSound", "Data/Audio/BoomerZombieIdle.wav", 3.0, 12.0)
    self.anim_instance:AddSoundNotify("Walk", 0.01, "WalkSound", "Data/Audio/BoomerZombieIdle.wav", 3.0, 12.0)
    self.anim_instance:AddSoundNotify("Boom", 0.01, "BoomSound", "Data/Audio/BoomerZombieBoom.wav", 3.0, 20.0)
    self.anim_instance:AddSoundNotify("Die", 0.01, "DieSound", "Data/Audio/BoomerZombieDying.wav", 100.0, 20.0)

    -- 초기 상태 설정 (Walk로 시작)
    self.anim_instance:SetState("Walk", 0)

    self.is_hit_cooldown = false
end

--- 데미지를 받습니다.
--- @param damage_amount number 받을 데미지량
--- @return void
function BoomerMonster:GetDamage(damage_amount)
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
function BoomerMonster:IsDamagedState()
    return self.anim_instance:GetCurrentStateName() == "Die" or
            (self.anim_instance:GetCurrentStateName() == "Damaged" and
                    self.anim_instance:GetStateTime("Damaged") < AnimDurations.DamagedDuration)
end

--- 공격 중인 상태인지 확인합니다.
--- @return boolean
function BoomerMonster:IsBoomingState()
    return self.anim_instance:GetCurrentStateName() == "Boom" and
            self.anim_instance:GetStateTime("Boom") < AnimDurations.BoomDuration
end

--- 애니메이션 상태를 업데이트합니다 (Attack, Damaged가 끝나면 Walk 또는 Idle로 복귀).
--- @return void
function BoomerMonster:UpdateAnimationState()
    local current_state = self.anim_instance:GetCurrentStateName()
    local current_time = self.anim_instance:GetStateTime(current_state)

    -- Boom 애니메이션이 끝나면 폭발
    if current_state == "Boom" and current_time >= AnimDurations.BoomDuration then
        self:Explode()
        return
    end

    -- Damaged 애니메이션이 끝나면 복귀
    if current_state == "Damaged" and current_time >= AnimDurations.DamagedDuration then
        if self.is_idle then
            self.anim_instance:SetState("Idle", 0.1)
        else
            self.anim_instance:SetState("Walk", 0.1)
        end
        return
    end
end

--- 폭발 처리: 파티클 스폰 후 자신을 비활성화합니다.
--- @return void
function BoomerMonster:Explode()
    -- 폭발 파티클 프리팹 스폰
    local explosion = SpawnPrefab(EXPLOSION_PREFAB)
    if explosion then
        explosion.Location = self.obj.Location
        explosion.bIsActive = true
    end

    -- 폭발 자국 데칼 프리팹 스폰
    local explosion_crater = SpawnPrefab(EXPLOSION_CRATER_PREFAB)
    if explosion_crater then
        explosion_crater.Location = self.obj.Location
        explosion_crater.bIsActive = true
    end

    -- 자신을 비활성화
    self.obj.bIsActive = false
end

--- Idle 상태로 전환합니다 (장애물/다른 몬스터와 충돌 시).
--- @return void
function BoomerMonster:SetIdle()
    if not self.is_idle then
        self.is_idle = true
        if self.anim_instance then
            self.anim_instance:SetState("Idle", 0.1)
        end
    end
end

--- BoomerMonster를 리셋합니다 (Respawn 시 사용).
--- @return void
function BoomerMonster:Reset()
    -- 스탯 리셋
    self.stat:Reset()

    -- Idle 상태 플래그 리셋
    self.is_idle = false

    -- 애니메이션 상태 리셋 (Walk로 시작)
    if self.anim_instance then
        self.anim_instance:SetState("Walk", 0)
    end
    self.is_hit_cooldown = false
end

--- 플레이어 위치를 확인하고 공격 범위 내에 있으면 Boom 공격합니다.
--- @return void
function BoomerMonster:CheckPlayerPositionAndBoom()
    local player_pos = MonsterUtil:FindPlayerPosition()
    local offset = self.obj.Location - player_pos
    local distance_squared = offset.X * offset.X + offset.Y * offset.Y
    local attack_range_squared = self.stat.attack_range * self.stat.attack_range
    local is_damaging = self:IsDamagedState()
    local is_Booming = self:IsBoomingState()

    -- 공격 범위 내에 있고, 피격 상태가 아니며, 공격 중이 아닐 때만 공격
    if (distance_squared < attack_range_squared and not is_damaging and not is_Booming) then
        self.anim_instance:SetState("Boom", 0)
    end
end

--- 플레이어를 향해 이동하고 회전합니다.
--- @param Delta number 델타 타임
--- @return void
function BoomerMonster:MoveToPlayer(Delta)
    -- 이미 Idle 상태면 이동하지 않음
    if self.is_idle then
        return
    end

    local player_pos = MonsterUtil:FindPlayerPosition()

    -- 플레이어를 찾지 못했으면 이동 안함
    if player_pos.X == 0 and player_pos.Y == 0 and player_pos.Z == 0 then
        return
    end

    -- 플레이어 x 좌표가 BoomerMonster x 좌표보다 크면 Idle 전환
    if player_pos.X > self.obj.Location.X then
        self:SetIdle()
        return
    end

    local offset = player_pos - self.obj.Location
    local distance_squared = offset.X * offset.X + offset.Y * offset.Y
    local attack_range_squared = self.stat.attack_range * self.stat.attack_range

    -- 공격 범위 밖이면 플레이어를 향해 이동
    if distance_squared > attack_range_squared then
        local direction = Vector(offset.X, offset.Y, 0)
        direction:Normalize()

        -- 이동
        self.obj.Location = self.obj.Location + direction * self.stat.move_speed * Delta

        -- 몬스터가 플레이어를 바라보도록 회전
        local yaw = math.deg(math.atan(direction.Y, direction.X))
        self.obj.Rotation = Vector(0, 0, yaw)
    end
end

return BoomerMonster