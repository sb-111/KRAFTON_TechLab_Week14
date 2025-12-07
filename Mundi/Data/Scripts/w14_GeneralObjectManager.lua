--- @type Pool
local Pool = require("w14_ObjectPool")
local MapConfig = require("w14_MapConfig")
local ObjectPlacer = require("w14_ObjectPlacer")

--- @class GeneralObjectManager
--- @field player userdata 추적할 플레이어 GameObject
--- @field objects table<Pool> 다양한 오브젝트에 대한 각각의 Pool
--- @field objects_spawn_nums table<string, number> 맵에 각각의 오브젝트 종류가 몇개 있어야 하는지 규정
--- @field object_radius table<string, number> 각 오브젝트의 반지름
local GeneralObjectManager = {}
GeneralObjectManager.__index = GeneralObjectManager

--- GeneralObjectManager 인스턴스 생성
--- @param object_placer ObjectPlacer Main에서 생성된 object_placer를 받아온다(다른 배치 도구와 같은 Placer를 공유해야 하기 때문)
--- @return GeneralObjectManager
function GeneralObjectManager:new(object_placer, player)
    local instance = {
        player = player,
        object_placer = object_placer,

        objects = {},
        objects_spawn_nums = {},
        object_radius = {},
    }
    setmetatable(instance, GeneralObjectManager)
    return instance
end

--- 오브젝트 타입 추가
--- @param prefab_path string 오브젝트 Prefab 경로
--- @param pool_size number Pool 크기
--- @param pool_standby_location userdata 비활성 상태 대기 위치
--- @param object_spawn_num number 맵에 존재해야 할 개수
--- @param radius number 오브젝트 반지름
function GeneralObjectManager:add_object(
        prefab_path,
        pool_size,
        pool_standby_location,
        object_spawn_num,
        radius,
        spawn_z_offset
)
    pool_size = pool_size or 100
    pool_standby_location = pool_standby_location or Vector(-2000, 0, 0)
    radius = radius or 1.0

    local new_object = Pool:new()
    --- 오브젝트 이름은 단순히 # + number로 설정
    local name = "#" .. tostring(#self.objects + 1)
    new_object:initialize(prefab_path, name, pool_size, pool_standby_location)

    self.objects_spawn_nums[name] = object_spawn_num
    self.object_radius[name] = radius

    -- Despawn 조건: head가 플레이어 뒤로 맵 크기만큼 떨어지면 despawn
    new_object:set_despawn_condition_checker(
            function()
                local head_obj = new_object.spawned:head()
                if not head_obj or not head_obj.Location or not self.player then
                    return false
                end
                return head_obj.Location.X < self.player.Location.X - MapConfig.map_chunk_x_size
            end
    )

    --- object Spawn 조건 설정
    --- 오브젝트가 맵에 지정된 개수보다 적게 존재하면 추가로 스폰하도록 지정
    new_object:set_spawn_condition_checker(
            function()
                -- Queue의 크기를 얻기 위해 spawned의 길이 계산
                local spawned_count = new_object.spawned.last - new_object.spawned.first + 1
                return self.objects_spawn_nums[new_object.name] > spawned_count
            end
    )

    --- object 소환 위치 설정
    new_object:set_spawn_location_getter(
            function()
                if not self.player then
                    return Vector(0, 0, 0)
                end
                local spawn_xy_location = self.object_placer:place_object(self.object_radius[new_object.name])
                return Vector(spawn_xy_location.X, spawn_xy_location.Y, spawn_z_offset)
            end
    )

    table.insert(self.objects, new_object)
end

function GeneralObjectManager:Tick()
    for i=1, #self.objects do
        self.objects[i]:Tick()
    end
end

--- 모든 오브젝트 풀 정리 (게임 재시작 시 호출)
function GeneralObjectManager:destroy()
    for i=1, #self.objects do
        if self.objects[i] and self.objects[i].destroy then
            self.objects[i]:destroy()
        end
    end
    self.objects = {}
    self.objects_spawn_nums = {}
    self.object_radius = {}
    self.player = nil
    self.object_placer = nil
end

return GeneralObjectManager
