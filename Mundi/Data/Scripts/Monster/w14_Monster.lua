--- Monster 클래스
--- @class Monster
--- @field move_speed number 이동 속도
--- @field health_point number 체력
--- @field attack_point number 공격력
--- @field attack_range number 공격 범위
--- @field is_dead boolean 사망 여부
--- @field obj userdata 게임 오브젝트
--- @field anim_instance userdata 애니메이션 상태 머신
local Monster = {}
Monster.__index = Monster

--- 애니메이션 재생 시간 상수
local AnimDurations = {
    IdleDuration = 3.0,
    AttackDuration = 2.6,      -- 공격 애니메이션 길이
    DamagedDuration = 2.0,     -- 피격 애니메이션 길이
    DyingDuration = 1.66
}

--- Monster 인스턴스를 생성합니다.
--- @return Monster
function Monster:new()
    local instance = {}
    instance.move_speed = 0
    instance.health_point = 0
    instance.attack_point = 0
    instance.attack_range = 0
    instance.is_dead = false
    instance.obj = nil
    instance.anim_instance = nil
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
        print("[Monster:Initialize][error] The Parameter is invalid.")
        return
    end

    -- 스탯 초기화
    self.move_speed = move_speed or 0
    self.health_point = health_point or 0
    self.attack_point = attack_point or 0
    self.attack_range = attack_range or 0

    self.obj = obj
    print("[Monster:Initialize] Getting SkeletalMeshComponent...")
    local mesh = GetComponent(self.obj, "USkeletalMeshComponent")

    if not mesh then
        print("[Monster:Initialize][error] The mesh is invalid.")
        return
    end
    print("[Monster:Initialize] SkeletalMeshComponent found!")

    -- 애니메이션 상태 머신 활성화 및 생성
    print("[Monster:Initialize] Activating StateMachine...")
    mesh:UseStateMachine()
    print("[Monster:Initialize] Creating StateMachine...")
    self.anim_instance = mesh:GetOrCreateStateMachine()
    if not self.anim_instance then
        print("[Monster:Initialize][error] The anim_instance is invalid.")
        return
    end
    print("[Monster:Initialize] StateMachine created!")

    -- 애니메이션 상태 추가
    print("[Monster:Initialize] Adding animation states...")
    self.anim_instance:AddState("Idle", "Data/GameJamAnim/Monster/BasicZombieIdle_mixamo.com", 1.0, true)
    self.anim_instance:AddState("Attack", "Data/GameJamAnim/Monster/BasicZombieAttack_mixamo.com", 1.0, true)
    self.anim_instance:AddState("Damaged", "Data/GameJamAnim/Monster/BasicZombieDamaged_mixamo.com", 1.0, false)
    self.anim_instance:AddState("Die", "Data/GameJamAnim/Monster/BasicZombieDying_mixamo.com", 1.0, false)

    -- 초기 상태 설정
    print("[Monster:Initialize] Setting initial state to Idle...")
    self.anim_instance:SetState("Idle", 0)

    -- 설정된 상태 확인
    local checkState = self.anim_instance:GetCurrentStateName()
    print("[Monster:Initialize] After SetState, current state: '" .. tostring(checkState) .. "'")
    print("[Monster:Initialize] Monster initialized successfully! HP: " .. self.health_point)
end

--- 데미지를 받습니다.
--- @param damage_amount number 받을 데미지량
--- @return void
function Monster:GetDamage(damage_amount)
    self.health_point = self.health_point - damage_amount
    print("[Monster:GetDamage] Received " .. damage_amount .. " damage. HP: " .. self.health_point)
    if (self.health_point <= 0) then
        print("[Monster:GetDamage] Monster died!")
        self.anim_instance:SetState("Die", 0)
        self.is_dead = true
    else
        print("[Monster:GetDamage] Playing damaged animation")
        self.anim_instance:SetState("Damaged", 0)
    end
end

--- 피격 상태(Damaged 또는 Die)인지 확인합니다.
--- @return boolean
function Monster:IsDamagedState()
    return self.anim_instance:GetCurrentStateName() == "Die" or
            (self.anim_instance:GetCurrentStateName() == "Damaged" and
            self.anim_instance:GetStateTime("Damaged") < AnimDurations.DamagedDuration)
end

--- 플레이어 위치를 확인하고 공격 범위 내에 있으면 공격합니다.
--- @return void
function Monster:CheckPlayerPositionAndAttack()
    local player_pos = self:FindPlayerPosition()
    local offset = self.obj.Location - player_pos
    local distance_squared = offset.X * offset.X + offset.Y * offset.Y
    local attack_range_squared = self.attack_range * self.attack_range
    local is_damaging = self:IsDamagedState()

    -- 공격 범위 내에 있고, 피격 상태가 아니면 공격
    if (distance_squared < attack_range_squared and not is_damaging) then
        print("[Monster:CheckPlayerPositionAndAttack] Player in range! Attacking...")
        self.anim_instance:SetState("Attack", 0)
    end
end

return Monster