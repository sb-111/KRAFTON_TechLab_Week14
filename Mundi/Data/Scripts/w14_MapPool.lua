--- @type Pool
local Pool = require("w14_ObjectPool")

--- @class MapPool : Pool
--- @field player userdata 추적할 플레이어 GameObject
--- @field map_chunk_x_size number Map Chunk의 X축 크기
--- @field next_spawn_location number 다음 spawn 위치 (X 좌표)
local MapPool = Pool:new()

--- Map은 player의 위치를 기준으로 spawn, despawn되므로 플레이어 객체를 등록
MapPool.player = nil
--- Map Chunk의 크기
MapPool.map_chunk_x_size = 10;
MapPool.next_spawn_location = 0;

--- MapPool이 추적할 플레이어 설정 및 despawn 조건 설정
--- @param player userdata 추적할 플레이어 GameObject
--- @return void
function MapPool:set_player_to_trace(player)
    self.player = player
    -- Despawn 조건: head가 플레이어 뒤로 50유닛 이상 떨어지면 despawn
    self:set_despawn_condition_checker(
        function(head_obj)
            if not head_obj or not head_obj.Location or not self.player then
                return false
            end
            return head_obj.Location.X < self.player.Location.X - self.map_chunk_x_size * 2.0
        end
    )
end

--- MapChunk Spawn 조건 설정 (모듈 로드 시 즉시 실행)
--- 플레이어가 next_spawn_location에 도달하면 새로운 청크를 spawn하고 next_spawn_location을 전진시킴
MapPool:set_spawn_condition_checker(
    function(head_obj)
        if not MapPool.player then
            return false
        end
        -- 플레이어가 다음 spawn 위치의 절반 지점에 도달하면 spawn
        if MapPool.player.Location.X >= MapPool.next_spawn_location - MapPool.map_chunk_x_size * 0.5 then
            MapPool.next_spawn_location = MapPool.next_spawn_location + MapPool.map_chunk_x_size
            return true
        end
        return false
    end
)

MapPool:set_spawn_location_getter(
        function()
            if not MapPool.player then
                return Vector(0, 0, 0)
            end
            -- X축으로 전진하는 맵이므로 next_spawn_location을 X좌표로 사용
            return Vector(MapPool.next_spawn_location, 0, 0)
        end
)

return MapPool
