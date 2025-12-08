--- @type GeneralObjectManager
local GeneralObjectManagerClass = require("w14_GeneralObjectManager")
local BiomeConfig = require("w14_BiomeConfig")
local MapConfig = require("w14_MapConfig")
local Math = require("w14_Math")

--- @class BiomeManager
--- @field player userdata 추적할 플레이어
--- @field object_placer ObjectPlacer 오브젝트 배치 도구
--- @field biome_managers table<number, GeneralObjectManager> 바이옴별 ObstacleManager
--- @field current_biome_index number 현재 활성화된 바이옴 인덱스
--- @field next_biome_change_x number 다음 바이옴 전환 위치 (X 좌표)
--- @field biome_change_interval number 바이옴 전환 간격 (X 좌표 거리)
local BiomeManager = {}
BiomeManager.__index = BiomeManager

--- BiomeManager 인스턴스 생성
--- @param player userdata 플레이어 객체
--- @param object_placer ObjectPlacer 오브젝트 배치 도구
--- @param biome_change_interval number|nil 바이옴 전환 간격 (기본값: map_chunk_x_size * 10)
--- @return BiomeManager
function BiomeManager:new(player, object_placer, biome_change_interval)
    local instance = {
        player = player,
        object_placer = object_placer,
        biome_managers = {},
        current_biome_index = 1,
        next_biome_change_x = 0,
        biome_change_interval = biome_change_interval or (MapConfig.map_chunk_x_size * 10)
    }
    setmetatable(instance, BiomeManager)

    -- BiomeConfig에서 바이옴들 초기화
    instance:_initialize_biomes()

    -- 첫 번째 바이옴 활성화
    if #instance.biome_managers > 0 then
        instance.current_biome_index = Math:RandomIntInRange(1, #instance.biome_managers)
        instance.next_biome_change_x = instance.biome_change_interval
        print("[BiomeManager] Starting biome: " .. instance:get_current_biome_name())
    end

    return instance
end

--- BiomeConfig 기반으로 바이옴들 초기화
function BiomeManager:_initialize_biomes()
    for i, biome_data in ipairs(BiomeConfig.biomes) do
        local manager = GeneralObjectManagerClass:new(self.object_placer, self.player)

        -- 해당 바이옴의 장애물들 등록
        for _, obstacle in ipairs(biome_data.obstacles) do
            manager:add_object(
                obstacle.prefab,
                BiomeConfig.pool_size,
                BiomeConfig.pool_standby_location,
                obstacle.spawn_num or BiomeConfig.default_spawn_num,
                obstacle.radius or BiomeConfig.default_radius,
                0.5
            )
        end

        self.biome_managers[i] = {
            name = biome_data.name,
            manager = manager,
            active = false
        }
    end
end

--- 현재 바이옴 이름 반환
--- @return string
function BiomeManager:get_current_biome_name()
    if self.biome_managers[self.current_biome_index] then
        return self.biome_managers[self.current_biome_index].name
    end
    return "Unknown"
end

--- 바이옴 전환 체크 및 처리
function BiomeManager:_check_biome_change()
    if not self.player then return end

    if self.player.Location.X >= self.next_biome_change_x then
        -- 다음 바이옴으로 전환 (랜덤, 현재와 다른 바이옴)
        local new_index = self.current_biome_index
        if #self.biome_managers > 1 then
            while new_index == self.current_biome_index do
                new_index = Math:RandomIntInRange(1, #self.biome_managers)
            end
        end

        local old_name = self:get_current_biome_name()
        self.current_biome_index = new_index
        self.next_biome_change_x = self.next_biome_change_x + self.biome_change_interval

        print("[BiomeManager] Biome changed: " .. old_name .. " -> " .. self:get_current_biome_name())
    end
end

--- 매 프레임 호출
function BiomeManager:Tick()
    if not self.player then return end

    -- 바이옴 전환 체크
    self:_check_biome_change()

    -- 현재 활성화된 바이옴의 매니저만 Tick 호출
    if self.biome_managers[self.current_biome_index] then
        self.biome_managers[self.current_biome_index].manager:Tick()
    end
end

--- 모든 바이옴 매니저 정리 (게임 재시작 시 호출)
function BiomeManager:reset()
    for _, biome in ipairs(self.biome_managers) do
        if biome.manager and biome.manager.despawn_all then
            biome.manager:despawn_all()
        end
    end
    self.current_biome_index = 1
    self.next_biome_change_x = 0
end

return BiomeManager