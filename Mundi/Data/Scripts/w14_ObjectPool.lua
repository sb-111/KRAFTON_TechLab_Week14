--- @type Queue
local Queue = require("w14_Queue")

--- @class Pool
--- @field name string
--- @field spawned Queue
--- @field despawned Queue
--- @field pool_size number
--- @field pool_standby_location userdata
--- @field spawn_condition_checker function
--- @field despawn_condition_checker function
--- @field spawn_location_getter function
local Pool = {}
Pool.__index = Pool

--- Pool 인스턴스를 생성합니다.
--- @return Pool
function Pool:new()
    -- 인스턴스 생성 (필드 수정 제약 없음)
    local instance = {
        name = nil,
        spawned = Queue:new(),
        despawned = Queue:new(),
        pool_size = nil,
        pool_standby_location = nil,
        spawn_condition_checker = nil,
        despawn_condition_checker = nil,
        spawn_location_getter = nil
    }
    setmetatable(instance, Pool)
    return instance
end

--- Initialize pool
--- @param prefab_path userdata c++의 actor 객체 타입. pool에서 관리할 object의 prefab path
--- @param name string pool의 이름
--- @param pool_size number pool에서 관리할 object의 수
--- @param pool_standby_location userdata c++의 FVector 타입 pool에서 관리하는 오브젝트가 비활성 상태에서 대기중일 때의 위치
--- @return void
function Pool:initialize(
        prefab_path,
        name,
        pool_size,
        pool_standby_location
)
    self.name = name
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

--- spawn_condition_checker를 설정
--- @param condition function despawned의 head가 spawned가 되는 조건. 반드시 bool을 반환하는 function을 저장해야 한다.
--- @return void
function Pool:set_spawn_condition_checker(condition)
    self.spawn_condition_checker = condition
end

--- despawn_condition_checker를 설정
--- @param condition function spawned의 head가 despawned가 되는 조건. 반드시 bool을 반환하는 function을 저장해야 한다.
--- @return void
function Pool:set_despawn_condition_checker(condition)
    self.despawn_condition_checker = condition
end

--- spawn_location_getter 설정
--- @param getter function 다음 스폰 위치를 반환하는 함수. 반드시 Vector를 반환하는 함수를 저장해야 한다.
--- @return void
function Pool:set_spawn_location_getter(getter)
    self.spawn_location_getter = getter
end

--- despawned에서 object를 하나 꺼내 활성으로 전환 후 spawned에 넣는다.
--- @return void
function Pool:spawn()
    if self.despawned:is_empty() then
        print("[Pool:spawn][error] despawned is empty.")
        return
    else
        local spawned = self.despawned:pop()
        spawned.bIsActive = true
        spawned.Location = self.spawn_location_getter()
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
        despawned.bIsActive = false
        despawned.Location = self.pool_standby_location
        self.despawned:push(despawned)
    end
end

--- spawned의 head에 대해 despawn 조건을 확인한 후 조건을 충족하면 despawn한다.
--- @return void
function Pool:check_despawn_condition_and_retrieve()
    if not self.despawn_condition_checker then
        return
    end
    while self.despawn_condition_checker() do
        self:despawn()
    end
end

--- despawned의 head에 대해 spawn 조건을 확인한 후 조건을 충족하면 spawn한다.
--- @return void
function Pool:check_spawn_condition_and_release()
    if not self.spawn_condition_checker then
        return
    end
    while self.spawn_condition_checker() do
        self:spawn()
    end
end

--- 매 프레임 호출되어 spawn/despawn 조건을 확인하고 처리한다.
--- @return void
function Pool:Tick()
    self:check_despawn_condition_and_retrieve()
    self:check_spawn_condition_and_release()
end

-- 모든 메서드 정의 후 클래스 프로토타입 보호
-- (__newindex는 클래스 수정만 방지, 인스턴스는 자유롭게 수정 가능)
do
    local Pool_mt = {
        __index = Pool,
        __newindex = function(_, key, value)
            print("[error] Attempt to modify Pool class [%s] with value [%s]", key, tostring(value))
        end
    }
    setmetatable(Pool, Pool_mt)
end

return Pool