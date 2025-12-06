-- w14_ParticlePool.lua
-- 파티클 풀링 클래스
--
--------------------------------------------------------------------------------
-- [설명]
--------------------------------------------------------------------------------
-- 프리팹 기반 파티클 Actor를 풀링하여 관리하는 클래스
-- 인스턴스화하여 사용하며, 각 풀은 하나의 파티클 타입을 담당
--
--------------------------------------------------------------------------------
-- [사용법]
--------------------------------------------------------------------------------
-- local ParticlePool = require("w14_ParticlePool")
--
-- local muzzlePool = ParticlePool:new()
-- muzzlePool:initialize("Prefabs/MuzzleFlash.prefab", 5, 0.5)
--
-- function OnShoot(pos)
--     muzzlePool:spawn(pos)
-- end
--
-- function Tick(dt)
--     muzzlePool:Tick(dt)
-- end
--
--------------------------------------------------------------------------------
-- [API]
--------------------------------------------------------------------------------
-- ParticlePool:new()                              -- 새 풀 인스턴스 생성
-- ParticlePool:initialize(prefab, size, lifetime) -- 프리팹으로 풀 초기화
-- ParticlePool:spawn(position)                    -- 위치에 파티클 생성
-- ParticlePool:despawn(entry)                     -- 파티클 회수
-- ParticlePool:Tick(dt)                           -- 매 프레임 업데이트
-- ParticlePool:stopAll()                          -- 모든 파티클 정지
-- ParticlePool:setOnSpawn(callback)               -- spawn 시 콜백 등록
-- ParticlePool:setOnDespawn(callback)             -- despawn 시 콜백 등록
--------------------------------------------------------------------------------

--- @class ParticlePool
--- @field actors table[] {actor, comp, isActive, elapsed}[] 풀링된 파티클 엔트리 배열. 각 엔트리는 {actor=Actor객체, comp=ParticleSystemComponent, isActive=활성여부, elapsed=경과시간}
--- @field pool_size number initialize()에서 생성 요청한 풀 크기
--- @field standby_location userdata 비활성 파티클이 대기하는 위치 (화면 밖 Vector(0,-9999,0))
--- @field lifetime number 파티클 자동 회수까지의 시간 (초). spawn 후 이 시간 지나면 자동 despawn
--- @field onSpawnCallback function|nil spawn 시 호출되는 콜백 (entry, position)
--- @field onDespawnCallback function|nil despawn 시 호출되는 콜백 (entry)
local ParticlePool = {}
ParticlePool.__index = ParticlePool

--- ParticlePool 인스턴스 생성
--- @return ParticlePool
function ParticlePool:new()
    local instance = {
        actors = {},
        pool_size = 0,
        standby_location = Vector(0, -9999, 0),
        lifetime = 2.0,
        onSpawnCallback = nil,
        onDespawnCallback = nil,
    }
    setmetatable(instance, {__index = ParticlePool})
    return instance
end

--- 풀 초기화: 프리팹에서 Actor 생성
--- @param prefab_path string 프리팹 경로
--- @param pool_size number 풀 크기
--- @param lifetime number|nil 파티클 수명 (초), 기본값 2
function ParticlePool:initialize(prefab_path, pool_size, lifetime)
    self.pool_size = pool_size
    self.lifetime = lifetime or 2.0

    for i = 1, pool_size do
        local actor = SpawnPrefab(prefab_path)
        if actor then
            local comp = GetComponent(actor, "UParticleSystemComponent")
            if comp then
                -- 초기 상태: 비활성, 대기 위치
                actor.Location = self.standby_location
                comp:Deactivate()

                local entry = {
                    actor = actor,
                    comp = comp,
                    isActive = false,
                    elapsed = 0,
                }
                table.insert(self.actors, entry)
            else
                print("[ParticlePool] No ParticleSystemComponent on prefab: " .. prefab_path)
            end
        else
            print("[ParticlePool] Failed to spawn prefab: " .. prefab_path)
        end
    end

    print("[ParticlePool] Initialized with " .. #self.actors .. " particles (lifetime: " .. self.lifetime .. "s)")
end

--- 위치에 파티클 생성 (비활성 → 활성)
--- @param position userdata Vector 위치
--- @return table|nil 생성된 entry 또는 nil
function ParticlePool:spawn(position)
    -- 비활성 Actor 찾기
    for _, entry in ipairs(self.actors) do
        if not entry.isActive then
            -- 위치 이동
            entry.actor.Location = position
            -- 파티클 활성화
            entry.comp:Activate()
            entry.isActive = true
            entry.elapsed = 0

            -- onSpawn 콜백 호출 (rawget으로 메타테이블 우회)
            local onSpawn = rawget(self, "onSpawnCallback")
            if onSpawn then
                onSpawn(entry, position)
            end

            return entry
        end
    end

    -- 풀 부족
    print("[ParticlePool] Pool exhausted! Consider increasing pool size.")
    return nil
end

--- 파티클 회수 (활성 → 비활성)
--- @param entry table 회수할 entry
function ParticlePool:despawn(entry)
    if not entry or not entry.isActive then
        return
    end

    -- onDespawn 콜백 호출 (rawget으로 메타테이블 우회)
    local onDespawn = rawget(self, "onDespawnCallback")
    if onDespawn then
        onDespawn(entry)
    end

    entry.comp:Deactivate()
    entry.actor.Location = self.standby_location
    entry.isActive = false
    entry.elapsed = 0
end

--- 매 프레임 업데이트: 수명 체크 후 자동 회수
--- @param dt number deltaTime
function ParticlePool:Tick(dt)
    for _, entry in ipairs(self.actors) do
        if entry.isActive then
            entry.elapsed = entry.elapsed + dt
            -- 수명 초과 시 자동 비활성화
            if entry.elapsed >= self.lifetime then
                self:despawn(entry)
            end
        end
    end
end

--- spawn 콜백 등록
--- @param callback function 콜백 함수 (entry, position)
function ParticlePool:setOnSpawn(callback)
    self.onSpawnCallback = callback
end

--- despawn 콜백 등록
--- @param callback function 콜백 함수 (entry)
function ParticlePool:setOnDespawn(callback)
    self.onDespawnCallback = callback
end

--- 모든 파티클 정지
function ParticlePool:stopAll()
    for _, entry in ipairs(self.actors) do
        if entry.isActive then
            self:despawn(entry)
        end
    end
end

--- 현재 활성 파티클 수 반환
--- @return number
function ParticlePool:getActiveCount()
    local count = 0
    for _, entry in ipairs(self.actors) do
        if entry.isActive then
            count = count + 1
        end
    end
    return count
end

--- 풀 크기 반환
--- @return number
function ParticlePool:getPoolSize()
    return #self.actors
end

-- 클래스 프로토타입 보호
do
    local ParticlePool_mt = {
        __index = ParticlePool,
        __newindex = function(_, key, value)
            print("[error] Attempt to modify ParticlePool class [" .. tostring(key) .. "]")
        end
    }
    setmetatable(ParticlePool, ParticlePool_mt)
end

return ParticlePool
