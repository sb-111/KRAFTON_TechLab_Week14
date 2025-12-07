-- Game/w14_AmmoManager.lua
-- 탄약 관리 모듈

local M = {}

-- 탄약 설정
local currentAmmo = 30      -- 현재 탄창 탄약
local maxAmmo = 30          -- 탄창 최대 용량
local totalAmmo = 120       -- 예비 탄약
local maxTotalAmmo = 120    -- 예비 탄약 최대 용량

-- 재장전 상태
local isReloading = false
local reloadDuration = 1.5  -- 재장전 시간 (초)

-- 콜백
local onAmmoChangeCallbacks = {}
local onReloadCallbacks = {}  -- 재장전 완료 콜백

-- ============ 탄약 조회 ============

function M.GetCurrentAmmo()
    return currentAmmo
end

function M.GetMaxAmmo()
    return maxAmmo
end

function M.GetTotalAmmo()
    return totalAmmo
end

function M.GetMaxTotalAmmo()
    return maxTotalAmmo
end

-- ============ 탄약 사용/획득 ============

--- 탄약 소비 (사격 시 호출)
--- @param amount number 소비할 탄약 수 (기본값 1)
--- @return boolean 사격 가능 여부 (false면 탄약 부족)
function M.UseAmmo(amount)
    amount = amount or 1

    if currentAmmo < amount then
        return false  -- 탄약 부족
    end

    currentAmmo = currentAmmo - amount
    M.NotifyChange()
    return true
end

--- 사격 가능 여부
--- @return boolean
function M.CanShoot()
    return currentAmmo > 0
end

--- 재장전 가능 여부 확인
--- @return boolean 재장전 가능하면 true
function M.CanReload()
    if isReloading then return false end           -- 이미 재장전 중
    if currentAmmo >= maxAmmo then return false end -- 탄창 가득 참
    if totalAmmo <= 0 then return false end         -- 예비 탄약 없음
    return true
end

--- 재장전 중인지 확인
--- @return boolean
function M.IsReloading()
    return isReloading
end

--- 재장전 시간 반환
--- @return number 재장전 시간 (초)
function M.GetReloadDuration()
    return reloadDuration
end

--- 재장전 시작 (Player에서 호출, 실제 재장전은 CompleteReload에서)
--- @return boolean 재장전 시작 성공 여부
function M.StartReload()
    if not M.CanReload() then
        return false
    end

    isReloading = true
    print(string.format("[AmmoManager] Reloading... (%d/%d, Reserve: %d)", currentAmmo, maxAmmo, totalAmmo))
    return true
end

--- 재장전 완료 (Player의 코루틴에서 시간 후 호출)
function M.CompleteReload()
    if not isReloading then return end

    local needed = maxAmmo - currentAmmo
    local toLoad = math.min(needed, totalAmmo)

    currentAmmo = currentAmmo + toLoad
    totalAmmo = totalAmmo - toLoad
    isReloading = false

    M.NotifyChange()
    print(string.format("[AmmoManager] Reload complete: %d/%d (Reserve: %d)", currentAmmo, maxAmmo, totalAmmo))

    -- 재장전 완료 콜백 호출
    for _, callback in ipairs(onReloadCallbacks) do
        callback()
    end
end

--- 재장전 취소 (피격 등으로 중단 시)
function M.CancelReload()
    if isReloading then
        isReloading = false
        print("[AmmoManager] Reload cancelled")
    end
end

--- 재장전 (즉시 완료 - 레거시 호환용)
--- @return boolean 재장전 성공 여부
function M.Reload()
    if not M.CanReload() then
        return false
    end

    local needed = maxAmmo - currentAmmo
    local toLoad = math.min(needed, totalAmmo)

    currentAmmo = currentAmmo + toLoad
    totalAmmo = totalAmmo - toLoad

    M.NotifyChange()
    print(string.format("[AmmoManager] Reloaded: %d/%d (Reserve: %d)", currentAmmo, maxAmmo, totalAmmo))
    return true
end

--- 탄약 획득 (아이템 획득 시)
--- @param amount number 획득할 탄약 수
--- @return number 실제로 획득한 탄약 수
function M.AddAmmo(amount)
    amount = amount or 30

    -- 최대치를 초과하지 않도록 제한
    local actualAmount = math.min(amount, maxTotalAmmo - totalAmmo)

    if actualAmount <= 0 then
        print("[AmmoManager] Ammo is already at maximum capacity")
        return 0
    end

    totalAmmo = totalAmmo + actualAmount
    M.NotifyChange()
    print(string.format("[AmmoManager] Ammo added: +%d (Reserve: %d/%d)", actualAmount, totalAmmo, maxTotalAmmo))
    return actualAmount
end

-- ============ 콜백 ============

--- 탄약 변경 콜백 등록
--- @param callback function (currentAmmo, maxAmmo, totalAmmo)
function M.OnAmmoChange(callback)
    table.insert(onAmmoChangeCallbacks, callback)
end

--- 콜백 호출 (내부용)
function M.NotifyChange()
    for _, callback in ipairs(onAmmoChangeCallbacks) do
        callback(currentAmmo, maxAmmo, totalAmmo)
    end
end

-- ============ 리셋 ============

--- 게임 시작 시 초기화
function M.Reset()
    currentAmmo = maxAmmo
    totalAmmo = 120  -- 예비 탄약
    isReloading = false
    print(string.format("[AmmoManager] Reset: %d/%d (Reserve: %d)", currentAmmo, maxAmmo, totalAmmo))
end

--- 설정 변경
--- @param newMaxAmmo number 새 탄창 용량
--- @param newTotalAmmo number 새 예비 탄약
function M.SetConfig(newMaxAmmo, newTotalAmmo)
    maxAmmo = newMaxAmmo or maxAmmo
    totalAmmo = newTotalAmmo or totalAmmo
    currentAmmo = math.min(currentAmmo, maxAmmo)
end

-- ============ 디버그 ============

function M.PrintStatus()
    print(string.format("[AmmoManager] %d/%d (Reserve: %d)", currentAmmo, maxAmmo, totalAmmo))
end

return M
