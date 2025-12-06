-- w14_Obstacle.lua
-- 장애물 Actor용 스크립트
-- 장애물 Prefab에 붙여서 사용
--
-- 사용법:
--   1. Prefab에 이 스크립트 붙이기
--   2. Actor 이름을 w14_ObstacleConfig.lua에 등록된 이름으로 설정
--      (예: "Obstacle_Fence", "Obstacle_Tree", "Obstacle_Box")
--   3. Player에서 충돌 시 OtherActor.Name으로 Config 조회

function BeginPlay()
    Obj.Tag = "obstacle"
end

function EndPlay()
end

function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end

function Tick(dt)
end
