-- Game/w14_AirstrikeManager.lua
-- 공중폭격 지원 시스템

local AirstrikeManager = {}

local CONFIG = {
    bombDistance = 70,   -- 플레이어 정면 거리 (m)
    dropHeight = 30,     -- 폭탄 투하 높이
    bombPrefab = "Data/Prefabs/w14_Bomb.prefab",
}

--- 공중폭격 실행
--- @param playerLocation Vector 플레이어 위치
function AirstrikeManager.Execute(playerLocation)
    -- 폭탄 투하 위치 (플레이어 정면 70m, 공중에서 시작)
    local dropPos = Vector(
        playerLocation.X + CONFIG.bombDistance,
        playerLocation.Y,
        CONFIG.dropHeight
    )

    -- 폭탄 스폰 (낙하 → 지면 도달 시 폭발)
    local bomb = SpawnPrefab(CONFIG.bombPrefab)
    if bomb then
        bomb.Location = dropPos
        print("[Airstrike] Bomb dropped at X=" .. dropPos.X .. ", Y=" .. dropPos.Y)
    end
end

return AirstrikeManager
