-- Mundi/Data/Scripts/w14_Jet.lua
-- 전투기 스크립트: -Y 방향으로 비행하며 플레이어 근처에서 폭탄을 투하합니다.

local Audio = require("Game/w14_AudioManager")

local BOMB_PREFAB = "Data/Prefabs/w14_Bomb.prefab"
local SPEED = 50.0            -- 제트기 비행 속도 (단위: m/s)
local DROP_THRESHOLD = 10.0    -- 이 Y좌표 차이 안으로 들어오면 폭탄 투하
local DESTROY_DISTANCE = 200.0 -- 이 거리만큼 지나가면 액터 파괴

local Player = nil
local bBombDropped = false
local startY = nil  -- 시작 위치 저장 (제거 판단용)

function BeginPlay()
    bBombDropped = false
    Player = FindObjectByName("Player")
    startY = Obj.Location.Y

    -- 제트기 사운드 재생
    Audio.PlaySFX("Jet")
end

function Tick(dt)
    if not Player then
        return
    end

    -- -Y 방향으로 계속 비행
    local currentPos = Obj.Location
    currentPos.Y = currentPos.Y - SPEED * dt
    Obj.Location = currentPos

    -- 실시간 플레이어 Y 위치와 비교
    local playerY = Player.Location.Y

    -- 아직 폭탄을 투하하지 않았고, 플레이어와의 Y좌표 차이가 10 이하이면 폭탄 투하
    if not bBombDropped and math.abs(currentPos.Y - playerY) <= DROP_THRESHOLD then
        DropBomb()
    end

    -- 시작 위치에서 충분히 멀어지면 액터 제거
    if bBombDropped and (startY - currentPos.Y) > DESTROY_DISTANCE then
        DeleteObject(Obj)
    end
end

function DropBomb()
    bBombDropped = true
    local bomb = SpawnPrefab(BOMB_PREFAB)
    if bomb then
        bomb.Location = Obj.Location
        print("[Jet] Bomb dropped at Y=" .. Obj.Location.Y)
    else
        print("[Jet] Failed to spawn bomb prefab.")
    end
end
