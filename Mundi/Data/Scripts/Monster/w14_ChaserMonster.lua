--- ChaserMonster 클래스
--- @class ChaserMonster
--- @field stat MobStat 스탯 관리 객체
--- @field obj userdata 게임 오브젝트
--- @field anim_instance userdata 애니메이션 상태 머신
--- @field is_permanently_idle boolean 영구 Idle 상태 여부
local MobStat = require("Monster/w14_MobStat")

local ChaserMonster = {}
ChaserMonster.__index = ChaserMonster

--- 애니메이션 재생 시간 상수
local AnimDurations = {
    WalkDuration = 2.0,        -- Walk는 루프
    AttackDuration = 2.6,      -- 공격 애니메이션 길이
    DamagedDuration = 2.0,     -- 피격 애니메이션 길이
    DyingDuration = 1.66
}

--- ChaserMonster 인스턴스를 생성합니다.
--- @return ChaserMonster
function ChaserMonster:new()
    local instance = {}
    instance.stat = MobStat:new()
    instance.obj = nil
    instance.anim_instance = nil
    instance.is_permanently_idle = false
    setmetatable(instance, ChaserMonster)
    return instance
end

--- 월드에서 플레이어를 찾아 위치를 반환합니다.
--- @return userdata 플레이어 위치 (찾지 못하면 Vector(0, 0, 0))
function ChaserMonster:FindPlayerPosition()
    local playerObj = FindObjectByName("player")
    if playerObj ~= nil then
        return playerObj.Location
    end
    return Vector(0, 0, 0)
end

--- ChaserMonster를 초기화합니다 (스탯 설정 및 애니메이션 상태 머신 설정).
--- @param obj userdata 게임 오브젝트
--- @param move_speed number 이동 속도
--- @param health_point number 체력
--- @param attack_point number 공격력
--- @param attack_range number 공격 범위
--- @return void
function ChaserMonster:Initialize(obj, move_speed, health_point, attack_point, attack_range)
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
    self.anim_instance:AddState("Idle", "Data/GameJamAnim/Monster/ChaserZombieIdle_mixamo.com", 1.0, true)
    self.anim_instance:AddState("Walk", "Data/GameJamAnim/Monster/ChaserZombieWalk_mixamo.com", 1.0, true)
    self.anim_instance:AddState("Attack", "Data/GameJamAnim/Monster/ChaserZombieAttack_mixamo.com", 1.0, false)
    self.anim_instance:AddState("Damaged", "Data/GameJamAnim/Monster/ChaserZombieDamaged_mixamo.com", 1.0, false)
    self.anim_instance:AddState("Die", "Data/GameJamAnim/Monster/ChaserZombieDying_mixamo.com", 1.0, false)

    -- 초기 상태 설정 (Walk로 시작)
    self.anim_instance:SetState("Walk", 0)
end

--- 데미지를 받습니다.
--- @param damage_amount number 받을 데미지량
--- @return void
function ChaserMonster:GetDamage(damage_amount)
    local died = self.stat:TakeDamage(damage_amount)

    if died then
        self.anim_instance:SetState("Die", 0)
    else
        self.anim_instance:SetState("Damaged", 0)
    end
end

--- 피격 상태(Damaged 또는 Die)인지 확인합니다.
--- @return boolean
function ChaserMonster:IsDamagedState()
    return self.anim_instance:GetCurrentStateName() == "Die" or
            (self.anim_instance:GetCurrentStateName() == "Damaged" and
            self.anim_instance:GetStateTime("Damaged") < AnimDurations.DamagedDuration)
end

--- 공격 중인 상태인지 확인합니다.
--- @return boolean
function ChaserMonster:IsAttackingState()
    return self.anim_instance:GetCurrentStateName() == "Attack" and
            self.anim_instance:GetStateTime("Attack") < AnimDurations.AttackDuration
end

--- 애니메이션 상태를 업데이트합니다 (Attack, Damaged가 끝나면 Walk 또는 Idle로 복귀).
--- @return void
function ChaserMonster:UpdateAnimationState()
    local current_state = self.anim_instance:GetCurrentStateName()
    local current_time = self.anim_instance:GetStateTime(current_state)

    -- Attack 애니메이션이 끝나면 복귀
    if current_state == "Attack" and current_time >= AnimDurations.AttackDuration then
        if self.is_permanently_idle then
            self.anim_instance:SetState("Idle", 0)
        else
            self.anim_instance:SetState("Walk", 0)
        end
        return
    end

    -- Damaged 애니메이션이 끝나면 복귀
    if current_state == "Damaged" and current_time >= AnimDurations.DamagedDuration then
        if self.is_permanently_idle then
            self.anim_instance:SetState("Idle", 0)
        else
            self.anim_instance:SetState("Walk", 0)
        end
        return
    end
end

--- ChaserMonster를 리셋합니다 (Respawn 시 사용).
--- @return void
function ChaserMonster:Reset()
    -- 스탯 리셋
    self.stat:Reset()

    -- Idle 상태 플래그 리셋
    self.is_permanently_idle = false

    -- 애니메이션 상태 리셋 (Walk로 시작)
    if self.anim_instance then
        self.anim_instance:SetState("Walk", 0)
    end
end

--- 플레이어 위치를 확인하고 공격 범위 내에 있으면 공격합니다.
--- @return void
function ChaserMonster:CheckPlayerPositionAndAttack()
    local player_pos = self:FindPlayerPosition()
    local offset = self.obj.Location - player_pos
    local distance_squared = offset.X * offset.X + offset.Y * offset.Y
    local attack_range_squared = self.stat.attack_range * self.stat.attack_range
    local is_damaging = self:IsDamagedState()
    local is_attacking = self:IsAttackingState()

    -- 공격 범위 내에 있고, 피격 상태가 아니며, 공격 중이 아닐 때만 공격
    if (distance_squared < attack_range_squared and not is_damaging and not is_attacking) then
        self.anim_instance:SetState("Attack", 0)
    end
end

--- 영구 Idle 상태로 전환합니다.
--- @return void
function ChaserMonster:SetPermanentIdle()
    if not self.is_permanently_idle then
        self.is_permanently_idle = true
        if self.anim_instance then
            self.anim_instance:SetState("Idle", 0)
        end
    end
end

--- 플레이어를 향해 이동하고 회전합니다.
--- @param Delta number 델타 타임
--- @return void
function ChaserMonster:MoveToPlayer(Delta)
    -- 이미 영구 Idle 상태면 이동하지 않음
    if self.is_permanently_idle then
        return
    end

    local player_pos = self:FindPlayerPosition()

    -- 플레이어를 찾지 못했으면 이동 안함
    if player_pos.X == 0 and player_pos.Y == 0 and player_pos.Z == 0 then
        return
    end

    -- 플레이어 x 좌표가 ChaserMonster x 좌표보다 크면 영구 Idle 전환
    if player_pos.X > self.obj.Location.X then
        self:SetPermanentIdle()
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

return ChaserMonster
