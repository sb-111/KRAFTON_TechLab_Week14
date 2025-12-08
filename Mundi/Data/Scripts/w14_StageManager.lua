--- @type GeneralObjectManager
local GeneralObjectManagerClass = require("w14_GeneralObjectManager")
local StageConfig = require("w14_StageConfig")
local Math = require("w14_Math")

--- @class StageManager
--- @field player userdata 추적할 플레이어
--- @field object_placer ObjectPlacer 오브젝트 배치 도구
--- @field obstacle_themes table<number, GeneralObjectManager> 테마별 ObstacleManager
--- @field current_theme_index number 현재 활성화된 장애물 테마 인덱스
--- @field next_theme_change_x number 다음 테마 전환 위치 (X 좌표)
--- @field theme_change_interval number 테마 전환 간격 (X 좌표 거리)
--- @field current_stage number 현재 난이도 스테이지 (1~7)
--- @field next_stage_change_x number 다음 스테이지 전환 위치 (X 좌표)
local StageManager = {}
StageManager.__index = StageManager

-- 현재 활성 인스턴스 (외부에서 접근용)
StageManager.instance = nil

--- StageManager 인스턴스 생성
--- @param player userdata 플레이어 객체
--- @param object_placer ObjectPlacer 오브젝트 배치 도구
--- @return StageManager
function StageManager:new(player, object_placer)
    local instance = {
        player = player,
        object_placer = object_placer,
        obstacle_themes = {},
        current_theme_index = 1,
        next_theme_change_x = 0,
        theme_change_interval = StageConfig.theme_change_interval,
        -- 난이도 스테이지 (1~7)
        current_stage = 1,
        next_stage_change_x = StageConfig.stage_change_interval
    }
    setmetatable(instance, StageManager)

    -- StageConfig에서 장애물 테마들 초기화
    instance:_initialize_themes()

    -- 첫 번째 테마 활성화
    if #instance.obstacle_themes > 0 then
        instance.current_theme_index = Math:RandomIntInRange(1, #instance.obstacle_themes)
        instance.next_theme_change_x = instance.theme_change_interval
        print("[StageManager] Starting theme: " .. instance:_get_current_theme_name() .. ", Stage: " .. instance.current_stage)
    end

    -- 현재 인스턴스 저장 (외부 접근용)
    StageManager.instance = instance

    return instance
end

--- StageConfig 기반으로 장애물 테마들 초기화
function StageManager:_initialize_themes()
    for i, theme_data in ipairs(StageConfig.stages) do
        local manager = GeneralObjectManagerClass:new(self.object_placer, self.player)

        -- 해당 테마의 장애물들 등록
        for _, obstacle in ipairs(theme_data.obstacles) do
            manager:add_object(
                obstacle.prefab,
                StageConfig.pool_size,
                StageConfig.pool_standby_location,
                theme_data.base_spawn_num or StageConfig.default_spawn_num,
                obstacle.radius or StageConfig.default_radius,
                0.25
            )
        end

        self.obstacle_themes[i] = {
            name = theme_data.name,
            manager = manager,
            base_spawn_num = theme_data.base_spawn_num or StageConfig.default_spawn_num,
            active = false
        }
    end
end

--- 현재 난이도 스테이지 이름 반환 (UI 표시용)
--- @return string
function StageManager:get_current_stage_name()
    return "Stage " .. self.current_stage
end

--- 현재 장애물 테마 이름 반환 (내부용)
--- @return string
function StageManager:_get_current_theme_name()
    if self.obstacle_themes[self.current_theme_index] then
        return self.obstacle_themes[self.current_theme_index].name
    end
    return "Unknown"
end

--- 현재 스테이지 기반 난이도 배수 계산
--- @return number 현재 적용할 스폰 개수 배수
function StageManager:_calculate_difficulty_multiplier()
    -- Stage 1 = 1.0배, Stage 7 = 3.0배 (선형 증가)
    local max_stage = StageConfig.difficulty.max_stage
    local max_multiplier = StageConfig.difficulty.max_multiplier
    local multiplier = 1.0 + (self.current_stage - 1) * ((max_multiplier - 1.0) / (max_stage - 1))
    return multiplier
end

--- 현재 테마의 스폰 개수 업데이트 (난이도 적용)
function StageManager:_update_spawn_counts()
    local current_theme = self.obstacle_themes[self.current_theme_index]
    if not current_theme then return end

    local multiplier = self:_calculate_difficulty_multiplier()
    local base_num = current_theme.base_spawn_num
    local new_spawn_num = math.floor(base_num * multiplier)

    -- GeneralObjectManager의 objects_spawn_nums 업데이트
    local manager = current_theme.manager
    for name, _ in pairs(manager.objects_spawn_nums) do
        manager.objects_spawn_nums[name] = new_spawn_num
    end
end

--- 난이도 스테이지 전환 체크 및 처리
function StageManager:_check_stage_change()
    if not self.player then return end

    local max_stage = StageConfig.difficulty.max_stage

    if self.player.Location.X >= self.next_stage_change_x then
        -- 다음 스테이지로 (최대까지)
        if self.current_stage < max_stage then
            local old_stage = self.current_stage
            self.current_stage = self.current_stage + 1
            self.next_stage_change_x = self.next_stage_change_x + StageConfig.stage_change_interval

            local multiplier = self:_calculate_difficulty_multiplier()
            print("[StageManager] Stage up: " .. old_stage .. " -> " .. self.current_stage .. " (difficulty x" .. string.format("%.1f", multiplier) .. ")")
        end
    end
end

--- 장애물 테마 전환 체크 및 처리 (테마 전환 간격마다)
function StageManager:_check_theme_change()
    if not self.player then return end

    if self.player.Location.X >= self.next_theme_change_x then
        -- 다음 테마로 전환 (랜덤, 현재와 다른 테마)
        local new_index = self.current_theme_index
        if #self.obstacle_themes > 1 then
            while new_index == self.current_theme_index do
                new_index = Math:RandomIntInRange(1, #self.obstacle_themes)
            end
        end

        local old_name = self:_get_current_theme_name()
        self.current_theme_index = new_index
        self.next_theme_change_x = self.next_theme_change_x + self.theme_change_interval

        print("[StageManager] Theme changed: " .. old_name .. " -> " .. self:_get_current_theme_name())
    end
end

--- 매 프레임 호출
function StageManager:Tick()
    if not self.player then return end

    -- 난이도 스테이지 전환 체크 (500m마다)
    self:_check_stage_change()

    -- 장애물 테마 전환 체크
    self:_check_theme_change()

    -- 난이도에 따른 스폰 개수 업데이트
    self:_update_spawn_counts()

    -- 현재 활성화된 테마의 매니저만 Tick 호출
    if self.obstacle_themes[self.current_theme_index] then
        self.obstacle_themes[self.current_theme_index].manager:Tick()
    end
end

--- 모든 테마 매니저 정리 (게임 재시작 시 호출)
function StageManager:destroy()
    for _, theme in ipairs(self.obstacle_themes) do
        if theme.manager and theme.manager.destroy then
            theme.manager:destroy()
        end
    end
    self.obstacle_themes = {}
    self.current_theme_index = 1
    self.next_theme_change_x = 0
    self.current_stage = 1
    self.next_stage_change_x = StageConfig.stage_change_interval
end

return StageManager
