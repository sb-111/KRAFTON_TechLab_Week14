-- w14_ObstacleConfig.lua
-- 장애물 종류별 속도 감소 설정

local Config = {
    -- ["프리팹/오브젝트 이름"] = { speedMult = 속도배율, duration = 지속시간(초) }
    ["Obstacle_Fence"] = { speedMult = 0.5, duration = 1.0 },   -- 50% 속도로 1초
    ["Obstacle_Tree"] = { speedMult = 0.3, duration = 1.5 },    -- 30% 속도로 1.5초
    ["Obstacle_Box"] = { speedMult = 0.7, duration = 0.5 },     -- 70% 속도로 0.5초
}

-- 기본값 (Config에 없는 장애물용)
Config.Default = { speedMult = 0.5, duration = 1.0 }

return Config
