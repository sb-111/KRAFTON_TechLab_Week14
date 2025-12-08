local DifficultyManagerClass = require("w14_DifficultyManager")

--- BossMonsterManager 클래스
--- 보스 몬스터의 스폰 및 난이도 관리
--- @class BossMonsterManager
local BossMonsterManager = {}
BossMonsterManager.__index = BossMonsterManager

--- BossMonsterManager 인스턴스를 생성합니다.
--- @return BossMonsterManager
function BossMonsterManager:new(
        spawn_time_difficulty_min,
        spawn_time_difficulty_max,
        spawn_time_difficulty_interval,
        hp_difficulty_min,
        hp_difficulty_max,
        hp_difficulty_interval,
        damage_difficulty_min,
        damage_difficulty_max,
        damage_difficulty_interval
)
    local instance = {}
    instance.current_spawn_time = 0
    instance.next_spawn_time = spawn_time_difficulty_min  -- 초기 스폰 시간 설정
    instance.spawn_time_difficulty_manager = DifficultyManagerClass:new(
            spawn_time_difficulty_min,
            spawn_time_difficulty_max,
            spawn_time_difficulty_interval
    )
    instance.hp_difficulty_manager = DifficultyManagerClass:new(
            hp_difficulty_min,
            hp_difficulty_max,
            hp_difficulty_interval
    )
    instance.damage_difficulty_manager = DifficultyManagerClass:new(
            damage_difficulty_min,
            damage_difficulty_max,
            damage_difficulty_interval
    )
    instance.current_boss = nil
    setmetatable(instance, BossMonsterManager)
    return instance
end

function BossMonsterManager:Tick(delta)
    -- 현재 보스가 생존해 있으면 파괴 여부 체크
    if self.current_boss then
        local boss_script = self.current_boss:GetScript()

        if not boss_script then
            return
        end

        if boss_script.ShouldDestroy() then
            DestroyObject(self.current_boss)
            self.current_boss = nil
        else
            return  -- 보스가 살아있으면 새 보스 스폰 안 함
        end
    end

    -- 난이도 매니저 업데이트
    local new_spawn_time = self.spawn_time_difficulty_manager:Tick(delta)
    local new_hp = self.hp_difficulty_manager:Tick(delta)
    local new_damage = self.damage_difficulty_manager:Tick(delta)

    -- 스폰 타이머 업데이트
    self.current_spawn_time = self.current_spawn_time + delta
    if self.current_spawn_time > self.next_spawn_time then
        -- 스폰 타이머 리셋 및 다음 스폰 시간 설정
        self.current_spawn_time = 0
        self.next_spawn_time = new_spawn_time

        -- 보스 스폰
        self.current_boss = SpawnPrefab("Data/Prefabs/w14_BossMonster.prefab")
        local boss_script = self.current_boss:GetScript()

        if not boss_script then
            return
        end

        boss_script.Initialize(new_hp, new_damage)
    end
end

function BossMonsterManager:reset()
    self.spawn_time_difficulty_manager:reset()
    self.hp_difficulty_manager:reset()
    self.damage_difficulty_manager:reset()

    self.current_spawn_time = 0

    if self.current_boss then
        DestroyObject(self.current_boss)
        self.current_boss = nil
    end
end

return BossMonsterManager