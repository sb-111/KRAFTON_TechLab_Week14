-- Game/w14_AirstrikeManager.lua
-- 공중폭격 지원 시스템

local AirstrikeManager = {}

local JetCONFIG = {
    jetX = 60,
    jetY = 80,
    jetZ = 40, 
    jetPrefab = "Data/Prefabs/w14_Jet.prefab"
}

--- 공중폭격 실행
--- @param playerLocation Vector 플레이어 위치
function AirstrikeManager.Execute(playerLocation)
    -- 제트기 시작 위치 계산
    local JetStartPos = Vector(
        playerLocation.X + JetCONFIG.jetX,
        playerLocation.Y + JetCONFIG.jetY,
        JetCONFIG.jetZ
    )

    -- 제트기 스폰 (w14_Jet.lua가 실시간 플레이어 위치 추적 및 폭탄 투하 담당)
    local jetActor = SpawnPrefab(JetCONFIG.jetPrefab)
    if jetActor then
        jetActor.Location = JetStartPos
        print("[Airstrike] Jet dispatched at Y=" .. JetStartPos.Y)
    else
        print("[Airstrike] ERROR: Failed to spawn Jet prefab.")
    end
end

return AirstrikeManager
