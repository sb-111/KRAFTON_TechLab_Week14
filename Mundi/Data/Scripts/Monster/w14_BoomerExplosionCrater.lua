--- BoomerExplosionCrater 스크립트
--- 폭발 자국 데칼이 일정 시간 후 자동 삭제됩니다.
local Audio = require("Game/w14_AudioManager")

local ElapsedTime = 0
local LIFETIME = 4.0  -- 4초 후 삭제

function BeginPlay()
    ElapsedTime = 0
end

function Tick(Delta)
    ElapsedTime = ElapsedTime + Delta

    if ElapsedTime >= LIFETIME then
        DeleteObject(Obj)
    end
end
