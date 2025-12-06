--- @type Pool
local Pool = require("w14_ObjectPool")
local MapConfig = require("w14_MapConfig")
local ObjectPlacer = require("w14_ObjectPlacer")

--- @class ObstacleManager
--- @field player userdata 추적할 플레이어 GameObject
--- @field obstacles table<Pool> 다양한 장애물에 대한 각각의 Pool
--- @field obstacles_spawn_nums table<string, number> 맵에 각각의 장애물 종류가 몇개 있어야 하는지 규정
--- @field obstacle_radius table<string, number> 각 장애물의 반지름
local ObstacleManager = {}
ObstacleManager.__index = ObstacleManager

--- ObstacleManager 인스턴스 생성
--- @param object_placer ObjectPlacer Main에서 생성된 object_placer를 받아온다(다른 배치 도구와 같은 Placer를 공유해야 하기 때문)
--- @return ObstacleManager
function ObstacleManager:new(object_placer)
    local instance = {
        player = nil,
        obstacles = {},
        obstacles_spawn_nums = {},
        obstacle_radius = {},
        object_placer = object_placer
    }
    setmetatable(instance, ObstacleManager)
    return instance
end

--- ObstacleManager가 추적할 플레이어 설정
--- @param player userdata 추적할 플레이어 GameObject
--- @return void
function ObstacleManager:set_player_to_trace(player)
    self.player = player
end

function ObstacleManager:add_obstacle(
        prefab_path,
        pool_size,
        pool_standby_location,
        obstacle_spawn_num,
        radius
)
    pool_size = pool_size or 100
    pool_standby_location = pool_standby_location or Vector(-2000, 0, 0)
    radius = radius or 1.0

    local new_obstacle = Pool:new()
    --- 장애물 이름은 단순히 # + number로 설정
    local name = "#" .. tostring(#self.obstacles + 1)
    new_obstacle:initialize(prefab_path, name, pool_size, pool_standby_location)

    self.obstacles_spawn_nums[name] = obstacle_spawn_num
    self.obstacle_radius[name] = radius

    -- Despawn 조건: head가 플레이어 뒤로 맵 크기만큼 떨어지면 despawn
    new_obstacle:set_despawn_condition_checker(
            function()
                local head_obj = new_obstacle.spawned:head()
                if not head_obj or not head_obj.Location or not self.player then
                    return false
                end
                return head_obj.Location.X < self.player.Location.X - MapConfig.map_chunk_x_size
            end
    )

    --- obstacle Spawn 조건 설정
    --- 장애물이 맵에 지정된 개수보다 적게 존재하면 추가로 스폰하도록 지정
    new_obstacle:set_spawn_condition_checker(
            function()
                -- Queue의 크기를 얻기 위해 spawned의 길이 계산
                local spawned_count = new_obstacle.spawned.last - new_obstacle.spawned.first + 1
                return self.obstacles_spawn_nums[new_obstacle.name] > spawned_count
            end
    )

    --- obstacle 소환 위치 설정
    new_obstacle:set_spawn_location_getter(
            function()
                if not self.player then
                    return Vector(0, 0, 0)
                end
                return self.object_placer:place_object(self.obstacle_radius[new_obstacle.name])
            end
    )

    table.insert(self.obstacles, new_obstacle)
end

function ObstacleManager:Tick()
    for i=1, #self.obstacles do
        self.obstacles[i]:Tick()
    end
end

return ObstacleManager