--- @type GeneralObjectManager
local GeneralObjectManagerClass = require("w14_GeneralObjectManager")
local StageConfig = require("w14_StageConfig")
local MapConfig = require("w14_MapConfig")
local Math = require("w14_Math")

--- @class StageManager
--- @field player userdata 추적할 플레이어
--- @field object_placer ObjectPlacer 오브젝트 배치 도구
--- @field stage_managers table<number, GeneralObjectManager> 스테이지별 ObstacleManager
--- @field current_stage_index number 현재 활성화된 스테이지 인덱스
--- @field next_stage_change_x number 다음 스테이지 전환 위치 (X 좌표)
--- @field stage_change_interval number 스테이지 전환 간격 (X 좌표 거리)
local StageManager = {}
StageManager.__index = StageManager

--- StageManager 인스턴스 생성
--- @param player userdata 플레이어 객체
--- @param object_placer ObjectPlacer 오브젝트 배치 도구
--- @param stage_change_interval number|nil 스테이지 전환 간격 (기본값: map_chunk_x_size * 10)
--- @return StageManager
function StageManager:new(player, object_placer, stage_change_interval)
    local instance = {
        player = player,
        object_placer = object_placer,
        stage_managers = {},
        current_stage_index = 1,
        next_stage_change_x = 0,
        stage_change_interval = stage_change_interval or (MapConfig.map_chunk_x_size * 10)
    }
    setmetatable(instance, StageManager)

    -- StageConfig에서 스테이지들 초기화
    instance:_initialize_stages()

    -- 첫 번째 스테이지 활성화
    if #instance.stage_managers > 0 then
        instance.current_stage_index = Math:RandomIntInRange(1, #instance.stage_managers)
        instance.next_stage_change_x = instance.stage_change_interval
        print("[StageManager] Starting stage: " .. instance:get_current_stage_name())
    end

    return instance
end

--- StageConfig 기반으로 스테이지들 초기화
function StageManager:_initialize_stages()
    for i, stage_data in ipairs(StageConfig.stages) do
        local manager = GeneralObjectManagerClass:new(self.object_placer, self.player)

        -- 해당 스테이지의 장애물들 등록
        for _, obstacle in ipairs(stage_data.obstacles) do
            manager:add_object(
                obstacle.prefab,
                StageConfig.pool_size,
                StageConfig.pool_standby_location,
                stage_data.base_spawn_num or StageConfig.default_spawn_num,
                obstacle.radius or StageConfig.default_radius,
                0.5
            )
        end

        self.stage_managers[i] = {
            name = stage_data.name,
            manager = manager,
            base_spawn_num = stage_data.base_spawn_num or StageConfig.default_spawn_num,
            active = false
        }
    end
end

--- 현재 스테이지 이름 반환
--- @return string
function StageManager:get_current_stage_name()
    if self.stage_managers[self.current_stage_index] then
        return self.stage_managers[self.current_stage_index].name
    end
    return "Unknown"
end

--- 거리 기반 난이도 배수 계산
--- @return number 현재 적용할 스폰 개수 배수
function StageManager:_calculate_difficulty_multiplier()
    if not self.player then return 1.0 end

    local distance = self.player.Location.X
    local config = StageConfig.difficulty

    -- 거리에 따른 배수 계산
    local intervals_passed = math.floor(distance / config.scale_interval)
    local multiplier = 1.0 + (intervals_passed * (config.spawn_increment / StageConfig.default_spawn_num))

    -- 최대 배수 제한
    return math.min(multiplier, config.max_multiplier)
end

--- 현재 스테이지의 스폰 개수 업데이트 (난이도 적용)
function StageManager:_update_spawn_counts()
    local current_stage = self.stage_managers[self.current_stage_index]
    if not current_stage then return end

    local multiplier = self:_calculate_difficulty_multiplier()
    local base_num = current_stage.base_spawn_num
    local new_spawn_num = math.floor(base_num * multiplier)

    -- GeneralObjectManager의 objects_spawn_nums 업데이트
    local manager = current_stage.manager
    for name, _ in pairs(manager.objects_spawn_nums) do
        manager.objects_spawn_nums[name] = new_spawn_num
    end
end

--- 스테이지 전환 체크 및 처리
function StageManager:_check_stage_change()
    if not self.player then return end

    if self.player.Location.X >= self.next_stage_change_x then
        -- 다음 스테이지로 전환 (랜덤, 현재와 다른 스테이지)
        local new_index = self.current_stage_index
        if #self.stage_managers > 1 then
            while new_index == self.current_stage_index do
                new_index = Math:RandomIntInRange(1, #self.stage_managers)
            end
        end

        local old_name = self:get_current_stage_name()
        self.current_stage_index = new_index
        self.next_stage_change_x = self.next_stage_change_x + self.stage_change_interval

        local multiplier = self:_calculate_difficulty_multiplier()
        print("[StageManager] Stage changed: " .. old_name .. " -> " .. self:get_current_stage_name() .. " (difficulty x" .. string.format("%.1f", multiplier) .. ")")
    end
end

--- 매 프레임 호출
function StageManager:Tick()
    if not self.player then return end

    -- 스테이지 전환 체크
    self:_check_stage_change()

    -- 난이도에 따른 스폰 개수 업데이트
    self:_update_spawn_counts()

    -- 현재 활성화된 스테이지의 매니저만 Tick 호출
    if self.stage_managers[self.current_stage_index] then
        self.stage_managers[self.current_stage_index].manager:Tick()
    end
end

--- 모든 스테이지 매니저 정리 (게임 재시작 시 호출)
function StageManager:destroy()
    for _, stage in ipairs(self.stage_managers) do
        if stage.manager and stage.manager.destroy then
            stage.manager:destroy()
        end
    end
    self.stage_managers = {}
    self.current_stage_index = 1
    self.next_stage_change_x = 0
end

return StageManager
