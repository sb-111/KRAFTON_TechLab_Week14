-- w14_Obstacle.lua
-- 장애물 Actor용 스크립트
-- 장애물 Prefab에 붙여서 사용

local Config = require("w14_ObstacleConfig")

function BeginPlay()
    Obj.Tag = "obstacle"

    -- Config에서 속도 감소 설정 가져오기
    local cfg = Config[Obj.Name] or Config.Default
    --Obj.SpeedMult = cfg.speedMult      -- 속도 배율 (0.5 = 50%)
    --Obj.SlowDuration = cfg.duration    -- 지속 시간 (초)
end

function EndPlay()
end

function OnBeginOverlap(OtherActor)
    -- 장애물 측 충돌 처리 (필요시 추가)
end

function OnEndOverlap(OtherActor)
end

function Tick(dt)
end
