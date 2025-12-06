--- @type Pool
local Pool = require("w14_ObjectPool")
local Math = require("w14_Math")

--- @class MapManager
--- @field player userdata 추적할 플레이어 GameObject
--- @field bioms table<Pool> 맵의 Pool을 저장할 바이옴
--- @field map_chunk_x_size number Map Chunk의 X축 크기
--- @field next_spawn_location number 다음 spawn 위치 (X 좌표)
local MapManager = {}
MapManager.__index = MapManager

--- MapManager 인스턴스 생성
--- @return MapManager
function MapManager:new()
    local instance = {
        player = nil,
        bioms = {},
        biom_name_to_spawn_this_turn = nil,
        map_chunk_x_size = 10,
        next_spawn_location = 0
    }
    setmetatable(instance, MapManager)
    return instance
end

--- MapManager가 추적할 플레이어 설정
--- @param player userdata 추적할 플레이어 GameObject
--- @return void
function MapManager:set_player_to_trace(player)
    self.player = player
end

function MapManager:add_biom(
        prefab_path,
        pool_size,
        pool_standby_location
)
    pool_size = pool_size or 100
    pool_standby_location = pool_standby_location or Vector(-1000, 0, 0)

    local new_biom = Pool:new()
    --- 바이옴 이름은 단순히 # + number로 설정
    local name = "#" .. tostring(#self.bioms + 1)
    new_biom:initialize(prefab_path, name, pool_size, pool_standby_location)

    -- Despawn 조건: head가 플레이어 뒤로 맵 크기의 2배 이상 떨어지면 despawn
    new_biom:set_despawn_condition_checker(
            function()
                local head_obj = new_biom.spawned:head()
                if not head_obj or not head_obj.Location or not self.player then
                    return false
                end
                return head_obj.Location.X < self.player.Location.X - self.map_chunk_x_size * 2.0
            end
    )

    --- biom Spawn 조건 설정
    --- 플레이어가 next_spawn_location에 도달하면 새로운 청크를 spawn하고 next_spawn_location을 전진시킴
    new_biom:set_spawn_condition_checker(
            function()
                local should_spawn = new_biom.name == self.biom_name_to_spawn_this_turn
                if should_spawn then
                    -- 한 번 spawn하면 biom_name_to_spawn_this_turn을 nil로 만들어 다음 체크에서 false가 되도록
                    self.biom_name_to_spawn_this_turn = nil
                end
                return should_spawn
            end
    )

    --- biom chunk 소환 위치 설정
    new_biom:set_spawn_location_getter(
            function()
                if not self.player then
                    return Vector(0, 0, 0)
                end
                -- X축으로 전진하는 맵이므로 next_spawn_location을 X좌표로 사용
                return Vector(self.next_spawn_location, self.player.Location.Y, 0)
            end
    )

    --- 설정이 끝난 biom을 테이블에 추가한다.
    table.insert(self.bioms, new_biom)
end

function MapManager:should_spawn()
    if self.player == nil then
        return false
    end
    return self.player.Location.X >=
            self.next_spawn_location - self.map_chunk_x_size * 0.5
end

function MapManager:Tick()
    if not self.player then
        return
    end

    if not self:should_spawn() then
        for i=1, #self.bioms do
            self.bioms[i]:Tick()
        end
        return
    end

    --- 플레이어가 다음 spawn 위치의 절반 지점에 도달하면 spawn
    self.next_spawn_location = self.next_spawn_location + self.map_chunk_x_size

    --- 현재 소환할 biom을 랜덤으로 정한다.
    local current_spawn_biom_index = Math:RandomIntInRange(1, #self.bioms)
    self.biom_name_to_spawn_this_turn = self.bioms[current_spawn_biom_index].name

    for i=1, #self.bioms do
        self.bioms[i]:Tick()
    end
end

return MapManager
