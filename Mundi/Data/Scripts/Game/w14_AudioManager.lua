-- Game/w14_AudioManager.lua
-- 오디오 관리 모듈
--
--------------------------------------------------------------------------------
-- [씬 설정]
--------------------------------------------------------------------------------
-- 1. BGMActor 생성
--    └── AudioComponent 1개 + Sounds[0]에 배경음악
--    (bIsLooping, bAutoPlay는 Lua에서 자동 설정됨)
--
-- 2. 사운드별 Actor 생성 (예: GunShotActor, ExplosionActor)
--    GunShotActor
--    ├── AudioComponent (Sounds[0] = shot.wav)
--    ├── AudioComponent (Sounds[0] = shot.wav)  ← 동시 재생용 복제
--    └── AudioComponent (Sounds[0] = shot.wav)
--
--    * 각 Actor는 하나의 사운드 타입 전담
--    * AudioComponent 개수 = 동시 재생 가능 수
--    * bAutoPlay는 등록 시 자동으로 false 설정됨
--
--------------------------------------------------------------------------------
-- [Lua 사용법]
--------------------------------------------------------------------------------
-- local Audio = require("Game/w14_AudioManager")
--
-- function BeginPlay()
--     Audio.Init()
--
--     Audio.RegisterSFX("gunshot", "GunShotActor")
--     Audio.RegisterSFX("explosion", "ExplosionActor")
--     Audio.RegisterBGM("main", "BGMActor")  -- 자동으로 Looping=true
-- end
--
-- function OnShoot()
--     Audio.PlaySFX("gunshot")
-- end
--
--------------------------------------------------------------------------------
-- [API 요약]
--------------------------------------------------------------------------------
-- Audio.Init()                      -- 초기화
-- Audio.RegisterSFX(name, actor)    -- SFX 등록 (AutoPlay 비활성화)
-- Audio.RegisterBGM(name, actor)    -- BGM 등록 (Looping 활성화)
-- Audio.PlaySFX(name)               -- SFX 재생 (빈 채널 우선)
-- Audio.PlayBGM(name)               -- BGM 재생
-- Audio.StopBGM(name)               -- BGM 정지
-- Audio.StopSFX(name)               -- 특정 SFX 정지
-- Audio.StopAll()                   -- 전체 정지
-- Audio.SetBGMVolume(name, vol)     -- BGM 볼륨 설정
-- Audio.SetSFXVolume(name, vol)     -- SFX 볼륨 설정 (전체 채널)
--------------------------------------------------------------------------------

local M = {}

-- 사운드 풀: name → { comps = {AudioComponent...}, index = 현재 인덱스 }
local sfxPools = {}
local bgmPools = {}

local isInitialized = false

-- ============ 초기화 ============

function M.Init()
    if isInitialized then return true end
    isInitialized = true
    print("[AudioManager] Initialized")
    return true
end

-- ============ 등록 ============

-- SFX 등록
-- @param name: 사용할 이름 (예: "gunshot")
-- @param actorName: 씬의 Actor 이름 (예: "GunShotActor")
function M.RegisterSFX(name, actorName)
    local actor = FindObjectByName(actorName)
    if not actor then
        print("[AudioManager] Actor not found: " .. actorName)
        return false
    end

    local comps = GetComponents(actor, "UAudioComponent")
    if #comps == 0 then
        print("[AudioManager] No AudioComponent on: " .. actorName)
        return false
    end

    -- 모든 채널 설정: AutoPlay 비활성화, Looping 비활성화
    for _, comp in ipairs(comps) do
        comp:SetAutoPlay(false)
        comp:SetLooping(false)
    end

    sfxPools[name] = {
        comps = comps,
        index = 1
    }

    print("[AudioManager] Registered SFX '" .. name .. "' (" .. #comps .. " channels)")
    return true
end

-- BGM 등록
function M.RegisterBGM(name, actorName)
    print("[AudioManager] RegisterBGM called with: " .. actorName)
    print("[AudioManager] FindObjectByName type: " .. type(FindObjectByName))

    local actor = FindObjectByName(actorName)
    print("[AudioManager] FindObjectByName result: " .. tostring(actor))

    if not actor then
        print("[AudioManager] Actor not found: " .. actorName)
        return false
    end

    local comp = GetComponent(actor, "UAudioComponent")
    if not comp then
        print("[AudioManager] No AudioComponent on: " .. actorName)
        return false
    end

    -- BGM 설정: AutoPlay 비활성화, Looping 활성화, 공간 볼륨 비활성화
    comp:SetIs2D(true) -- 2D모드 활성화(공간 볼륨 안사용)
    comp:SetAutoPlay(false)
    comp:SetLooping(true)

    bgmPools[name] = {
        comp = comp
    }

    print("[AudioManager] Registered BGM '" .. name .. "' (Looping enabled)")
    return true
end

-- ============ SFX ============

-- SFX 재생 (빈 채널 우선, 없으면 라운드 로빈)
function M.PlaySFX(name)
    local pool = sfxPools[name]
    if not pool then
        print("[AudioManager] Unknown SFX: " .. tostring(name))
        return
    end

    -- 빈 채널 찾기
    for i, comp in ipairs(pool.comps) do
        if not comp:IsPlaying() then
            comp:Play()
            return
        end
    end

    -- 빈 채널 없으면 라운드 로빈
    local comp = pool.comps[pool.index]
    if comp then
        comp:Play()
    end

    pool.index = pool.index + 1
    if pool.index > #pool.comps then
        pool.index = 1
    end
end

-- 특정 SFX 모두 정지
function M.StopSFX(name)
    local pool = sfxPools[name]
    if not pool then return end

    for _, comp in ipairs(pool.comps) do
        comp:Stop()
    end
end

-- 모든 SFX 정지
function M.StopAllSFX()
    for _, pool in pairs(sfxPools) do
        for _, comp in ipairs(pool.comps) do
            comp:Stop()
        end
    end
end

-- SFX 볼륨 설정 (해당 이름의 모든 채널)
function M.SetSFXVolume(name, volume)
    local pool = sfxPools[name]
    if not pool then return end

    for _, comp in ipairs(pool.comps) do
        comp:SetVolume(volume)
    end
end

-- ============ BGM ============

-- BGM 재생
function M.PlayBGM(name)
    local pool = bgmPools[name]
    if not pool then
        print("[AudioManager] Unknown BGM: " .. tostring(name))
        return
    end

    if pool.comp then
        pool.comp:Play()
    end
end

-- BGM 정지
function M.StopBGM(name)
    if name then
        local pool = bgmPools[name]
        if pool and pool.comp then
            pool.comp:Stop()
        end
    else
        for _, pool in pairs(bgmPools) do
            if pool.comp then
                pool.comp:Stop()
            end
        end
    end
end

-- BGM 볼륨 설정
function M.SetBGMVolume(name, volume)
    local pool = bgmPools[name]
    if pool and pool.comp then
        pool.comp:SetVolume(volume)
    end
end

-- ============ 유틸리티 ============

function M.StopAll()
    M.StopAllSFX()
    M.StopBGM()
end

function M.PrintRegistered()
    print("[AudioManager] === Registered Sounds ===")
    print("SFX:")
    for name, pool in pairs(sfxPools) do
        print("  " .. name .. " (" .. #pool.comps .. " channels)")
    end
    print("BGM:")
    for name, _ in pairs(bgmPools) do
        print("  " .. name)
    end
end

function M.GetChannelCount(name)
    local pool = sfxPools[name]
    if pool then
        return #pool.comps
    end
    return 0
end

return M
