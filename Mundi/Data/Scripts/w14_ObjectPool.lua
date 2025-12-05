--- @type Queue
local Queue = require("w14_Queue")

--- @class Pool
--- @field spawned Queue
--- @field despawned Queue
--- @field pool_size number
--- @field pool_standby_location userdata
--- @field despawn_condition_checker function
local Pool = {}
Pool.__index = Pool
Pool.__newindex = function(_, key, value)
    print(string.format("[error] Attempt to modify constant [%s] with value [%s]", key, tostring(value)))
end

--- Make new Pool instance
--- @return Pool
function Pool:new()
    local obj = setmetatable({}, self)

    --- 아래의 멤버들은 initialize에서 초기화
    obj.spawned = Queue:new()
    obj.despawned = Queue:new()
    obj.pool_size = nil
    obj.pool_standby_location = nil
    obj.despawn_condition_checker = nil
    
    return obj
end

--- Initialize pool
--- @param prefab_path userdata c++의 actor 객체 타입. pool에서 관리할 object의 prefab path
--- @param pool_size number pool에서 관리할 object의 수
--- @param pool_standby_location userdata c++의 FVector 타입 pool에서 관리하는 오브젝트가 비활성 상태에서 대기중일 때의 위치
--- @return void
function Pool:initialize(
        prefab_path,
        pool_size,
        pool_standby_location,
        despawn_condition_checker
)
    self.pool_size = pool_size
    self.pool_standby_location = pool_standby_location

    for i=1, self.pool_size do
        local new_obj = SpawnPrefab(prefab_path)
        if new_obj then
            new_obj.Location = pool_standby_location
            new_obj.bIsActive = false;
            self.despawned:push(new_obj)
        end
    end
end

--- despawn_condition_checker를 설정
--- @param condition function spawned의 head가 despawned가 되는 조건. 반드시 bool을 반환하고 하나의 obj를 param으로 받는 function을 저장해야 한다.
--- @return void
function Pool:set_despawn_condition_checker(condition)
    self.despawn_condition_checker = condition
end

--- despawned에서 object를 하나 꺼내 활성으로 전환 후 spawned에 넣는다.
--- @return void
function Pool:spawn()
    if self.despawned:is_empty() then
        print("[Pool:spawn][error] despawned is empty.")
        return
    else
        local spawned = self.despawned:pop()
        spawned.bIsActive = true;
        self.spawned:push(spawned)
    end
end

--- spawned에서 object를 하나 꺼내 비활성으로 전환 후 despawned에 넣는다.
--- @return void
function Pool:despawn()
    if self.spawned:is_empty() then
        print("[Pool:despawn][error] spawned is empty.")
        return
    else
        local despawned = self.spawned:pop()
        despawned.bIsActive = false;
        self.despawned:push(despawned)
    end
end

--- spawned의 head에 대해 비활성 조건을 확인한 후 조건을 충족하면 비활성한다.
--- @return void
function Pool:check_despawn_condition_and_retrieve()
    while self:despawn_condition_checker(self.spawned:head()) do
        self:despawn();
    end
end

return Pool