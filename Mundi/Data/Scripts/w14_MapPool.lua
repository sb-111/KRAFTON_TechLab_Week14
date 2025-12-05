--- @type Pool
local Pool = require("w14_ObjectPool")

--- @class MapPool : Pool
--- @field player userdata
local MapPool = Pool:new()

--- Map은 player의 위치를 기준으로 spawn, despawn되므로 플레이어 객체를 등록
MapPool.player = nil

--- MapPool이 추적할 플레이어 설정
--- @param player userdata 추적할 플레이어
--- @return void
function MapPool:set_player_to_trace(player)
    self.player = player
    self:set_despawn_condition_checker(
        function(head_obj)
            if head_obj and head_obj.Location and self.player then
                return head_obj.Location.X < self.player.Location.X - 50.0
            end
            return false
        end
    )
end

return MapPool
