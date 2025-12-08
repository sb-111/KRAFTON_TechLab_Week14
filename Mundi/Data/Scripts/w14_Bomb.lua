-- w14_Bomb.lua
-- 폭탄 스크립트 - 떨어지다가 땅에 닿으면 폭발

local FALL_SPEED = 50          -- 낙하 속도
local GROUND_Z = 0.5           -- 지면 높이
local EXPLOSION_PREFAB = "Data/Prefabs/w14_Explosion.prefab"

function BeginPlay()
end

function Tick(dt)
    -- 낙하
    local pos = Obj.Location
    pos.Z = pos.Z - FALL_SPEED * dt
    Obj.Location = pos

    -- 지면 도달 체크
    if pos.Z <= GROUND_Z then
        Explode()
    end
end

function Explode()
    -- 폭발 프리팹 스폰
    local explosion = SpawnPrefab(EXPLOSION_PREFAB)
    if explosion then
        explosion.Location = Vector(Obj.Location.X, Obj.Location.Y, GROUND_Z)
    end

    -- 폭탄 제거
    DeleteObject(Obj)
end
