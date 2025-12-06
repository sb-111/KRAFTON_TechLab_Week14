-- Game/w14_ParticleManager.lua
-- 파티클 매니저 (싱글턴)
--
--------------------------------------------------------------------------------
-- [설명]
--------------------------------------------------------------------------------
-- 여러 ParticlePool을 중앙에서 관리하는 싱글턴 모듈
-- 파티클 타입을 이름으로 등록하고, 간단한 API로 파티클을 생성
--
--------------------------------------------------------------------------------
-- [씬 설정]
--------------------------------------------------------------------------------
-- 1. 파티클 프리팹 생성
--    - Actor + ParticleSystemComponent
--    - bAutoActivate = false 권장
--
-- 2. 프리팹 파일 저장
--    - Prefabs/MuzzleFlash.prefab
--    - Prefabs/BloodSplat.prefab
-- * 씬에 파티클 액터가 존재할 필요는 없음. 
--
--------------------------------------------------------------------------------
-- [Lua 사용법]
--------------------------------------------------------------------------------
-- local Particle = require("Game/w14_ParticleManager")
--
-- function BeginPlay()
--     Particle.Init()
--     Particle.Register("muzzle", "Prefabs/MuzzleFlash.prefab", 5, 0.5)
--     Particle.Register("blood", "Prefabs/BloodSplat.prefab", 10, 1.0)
-- end
--
-- function OnShoot(gunPos)
--     Particle.Spawn("muzzle", gunPos)
-- end
--
-- function OnHit(hitPos)
--     Particle.Spawn("blood", hitPos)
-- end
--
-- function Tick(dt)
--     Particle.Tick(dt)
-- end
--
--------------------------------------------------------------------------------
-- [API 요약]
--------------------------------------------------------------------------------
-- Particle.Init()                                  -- 초기화
-- Particle.Register(name, prefab, size, lifetime)  -- 파티클 타입 등록
-- Particle.Spawn(name, position)                   -- 위치에 파티클 생성
-- Particle.Stop(name)                              -- 특정 타입 모두 정지
-- Particle.StopAll()                               -- 전체 정지
-- Particle.Tick(dt)                                -- 매 프레임 업데이트
-- Particle.SetOnSpawn(name, callback)              -- spawn 콜백 등록
-- Particle.SetOnDespawn(name, callback)            -- despawn 콜백 등록
-- Particle.GetPool(name)                           -- 풀 직접 접근 (고급)
--------------------------------------------------------------------------------

local ParticlePool = require("w14_ParticlePool")

local M = {}

-- 파티클 풀 저장소: name -> ParticlePool
local pools = {}

local isInitialized = false

-- ============ 초기화 ============

function M.Init()
    if isInitialized then return true end
    pools = {}
    isInitialized = true
    print("[ParticleManager] Initialized")
    return true
end

-- ============ 등록 ============

--- 파티클 타입 등록
--- @param name string 파티클 이름 (예: "muzzle", "blood")
--- @param prefab_path string 프리팹 경로
--- @param pool_size number 풀 크기 (동시 재생 가능 수)
--- @param lifetime number|nil 파티클 수명 (초), 기본값 2.0
function M.Register(name, prefab_path, pool_size, lifetime)
    if pools[name] then
        print("[ParticleManager] Warning: '" .. name .. "' already registered, overwriting")
    end

    local pool = ParticlePool:new()
    pool:initialize(prefab_path, pool_size, lifetime or 2.0)
    pools[name] = pool

    print("[ParticleManager] Registered '" .. name .. "' (" .. pool_size .. " pooled, " .. (lifetime or 2.0) .. "s lifetime)")
    return true
end

-- ============ 생성/정지 ============

--- 위치에 파티클 생성
--- @param name string 등록된 파티클 이름
--- @param position userdata Vector 위치
--- @return table|nil 생성된 entry
function M.Spawn(name, position)
    local pool = pools[name]
    if not pool then
        print("[ParticleManager] Unknown particle: " .. tostring(name))
        return nil
    end
    return pool:spawn(position)
end

--- 특정 파티클 타입 모두 정지
--- @param name string 파티클 이름
function M.Stop(name)
    local pool = pools[name]
    if pool then
        pool:stopAll()
    end
end

--- 모든 파티클 정지
function M.StopAll()
    for _, pool in pairs(pools) do
        pool:stopAll()
    end
end

-- ============ 콜백 ============

--- spawn 콜백 등록 (파티클 생성 시 자동 호출)
--- @param name string 파티클 이름
--- @param callback function 콜백 함수 (entry, position)
function M.SetOnSpawn(name, callback)
    local pool = pools[name]
    if pool then
        pool:setOnSpawn(callback)
    else
        print("[ParticleManager] Cannot set callback: '" .. name .. "' not registered")
    end
end

--- despawn 콜백 등록 (파티클 회수 시 자동 호출)
--- @param name string 파티클 이름
--- @param callback function 콜백 함수 (entry)
function M.SetOnDespawn(name, callback)
    local pool = pools[name]
    if pool then
        pool:setOnDespawn(callback)
    else
        print("[ParticleManager] Cannot set callback: '" .. name .. "' not registered")
    end
end

-- ============ 업데이트 ============

--- 매 프레임 호출 (모든 풀 업데이트)
--- @param dt number deltaTime
function M.Tick(dt)
    for _, pool in pairs(pools) do
        pool:Tick(dt)
    end
end

-- ============ 유틸리티 ============

--- 풀 직접 접근 (고급 사용)
--- @param name string 파티클 이름
--- @return ParticlePool|nil
function M.GetPool(name)
    return pools[name]
end

--- 등록된 파티클 목록 출력
function M.PrintRegistered()
    print("[ParticleManager] === Registered Particles ===")
    for name, pool in pairs(pools) do
        local active = pool:getActiveCount()
        local total = pool:getPoolSize()
        print("  " .. name .. " (" .. active .. "/" .. total .. " active)")
    end
end

--- 특정 파티클의 활성 개수 반환
--- @param name string 파티클 이름
--- @return number
function M.GetActiveCount(name)
    local pool = pools[name]
    if pool then
        return pool:getActiveCount()
    end
    return 0
end

return M
