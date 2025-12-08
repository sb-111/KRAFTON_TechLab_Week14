-- Game/w14_HPManager.lua
-- 플레이어 HP 관리 모듈

local M = {}

-- HP 설정
local maxHP = 100
local currentHP = 100
local bInvincible = false
local bDead = false
local invincibleDuration = 1.5

-- 콜백
local onHPChangeCallbacks = {}
local onDeathCallbacks = {}

-- ============ HP 조회 ============

function M.GetCurrentHP()
    return currentHP
end

function M.GetMaxHP()
    return maxHP
end

function M.IsDead()
    return bDead
end

function M.IsInvincible()
    return bInvincible
end

-- ============ HP 관리 ============

--- HP 초기화 (게임 시작/재시작 시)
function M.Reset()
    currentHP = maxHP
    bInvincible = false
    bDead = false
    M.NotifyChange()
end

--- 데미지 처리
--- @param damage number 받을 데미지
--- @return boolean 사망 여부 (true면 사망)
function M.TakeDamage(damage)
    if bInvincible or bDead then
        return false
    end

    currentHP = currentHP - damage

    -- 피격 카메라 셰이크
    GetCameraManager():StartCameraShake(0.15, 0.2, 0.15, 30)

    M.NotifyChange()

    if currentHP <= 0 then
        currentHP = 0
        bDead = true
        M.NotifyDeath()
        return true  -- 사망
    end

    M.StartInvincibility()
    return false  -- 생존
end

--- 체력 회복
--- @param amount number 회복량
function M.Heal(amount)
    if bDead then
        return  -- 사망 상태면 회복 불가
    end

    currentHP = currentHP + amount
    if currentHP > maxHP then
        currentHP = maxHP  -- 최대 HP 초과 방지
    end

    M.NotifyChange()
    print(string.format("[HPManager] Healed %d HP (Current: %d/%d)", amount, currentHP, maxHP))
end

--- 무적 상태 시작
function M.StartInvincibility()
    bInvincible = true

    -- 코루틴으로 무적 자동 종료
    StartCoroutine(function()
        coroutine.yield("wait_time", invincibleDuration)
        M.EndInvincibility()
    end)
end

--- 무적 상태 종료
function M.EndInvincibility()
    bInvincible = false
end

-- ============ 콜백 ============

--- HP 변경 콜백 등록
--- @param callback function (currentHP, maxHP)
function M.OnHPChange(callback)
    table.insert(onHPChangeCallbacks, callback)
end

--- 사망 콜백 등록
--- @param callback function ()
function M.OnDeath(callback)
    table.insert(onDeathCallbacks, callback)
end

--- HP 변경 알림 (내부용)
function M.NotifyChange()
    for _, callback in ipairs(onHPChangeCallbacks) do
        callback(currentHP, maxHP)
    end
end

--- 사망 알림 (내부용)
function M.NotifyDeath()
    for _, callback in ipairs(onDeathCallbacks) do
        callback()
    end
end

-- ============ 디버그 ============

function M.PrintStatus()
    print(string.format("[HPManager] %d/%d (Dead: %s, Invincible: %s)",
        currentHP, maxHP, tostring(bDead), tostring(bInvincible)))
end

return M
