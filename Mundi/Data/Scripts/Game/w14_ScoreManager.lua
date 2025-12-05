-- Game/w14_ScoreManager.lua
-- 점수 관리 모듈

local M = {}

-- 현재 점수
local distance = 0          -- 이동 거리 (m)
local killCount = 0         -- 처치한 몬스터 수

-- 최고 기록
local bestDistance = 0
local bestKillCount = 0

-- 콜백
local onDistanceChangeCallbacks = {}
local onKillCountChangeCallbacks = {}

-- ============ 거리 관련 ============

function M.GetDistance()
    return distance
end

function M.GetBestDistance()
    return bestDistance
end

function M.AddDistance(amount)
    distance = distance + amount

    -- 최고 기록 갱신
    if distance > bestDistance then
        bestDistance = distance
    end

    -- 콜백 호출
    for _, callback in ipairs(onDistanceChangeCallbacks) do
        callback(distance, bestDistance)
    end
end

function M.SetDistance(value)
    distance = value

    if distance > bestDistance then
        bestDistance = distance
    end

    for _, callback in ipairs(onDistanceChangeCallbacks) do
        callback(distance, bestDistance)
    end
end

-- ============ 킬 카운트 관련 ============

function M.GetKillCount()
    return killCount
end

function M.GetBestKillCount()
    return bestKillCount
end

function M.AddKill(amount)
    amount = amount or 1
    killCount = killCount + amount

    if killCount > bestKillCount then
        bestKillCount = killCount
    end

    for _, callback in ipairs(onKillCountChangeCallbacks) do
        callback(killCount, bestKillCount)
    end
end

-- ============ 콜백 등록 ============

function M.OnDistanceChange(callback)
    table.insert(onDistanceChangeCallbacks, callback)
end

function M.OnKillCountChange(callback)
    table.insert(onKillCountChangeCallbacks, callback)
end

-- ============ 리셋 ============

function M.Reset()
    distance = 0
    killCount = 0
    -- 최고 기록은 유지

    print("[ScoreManager] Reset - Distance: 0, Kills: 0")
end

function M.ResetAll()
    distance = 0
    killCount = 0
    bestDistance = 0
    bestKillCount = 0

    print("[ScoreManager] Full Reset")
end

-- ============ 포맷팅 헬퍼 ============

-- 거리를 문자열로 (1234 -> "1,234m")
function M.FormatDistance(value)
    value = value or distance
    local formatted = tostring(math.floor(value))
    -- 천 단위 콤마
    local k
    while true do
        formatted, k = string.gsub(formatted, "^(-?%d+)(%d%d%d)", '%1,%2')
        if k == 0 then break end
    end
    return formatted .. "m"
end

-- ============ 디버그 ============

function M.PrintStatus()
    print(string.format("[ScoreManager] Distance: %s (Best: %s) | Kills: %d (Best: %d)",
        M.FormatDistance(distance),
        M.FormatDistance(bestDistance),
        killCount,
        bestKillCount
    ))
end

return M
