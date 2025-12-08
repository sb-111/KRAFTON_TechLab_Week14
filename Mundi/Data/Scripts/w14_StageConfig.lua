--- @class StageConfig
--- 스테이지별 장애물 설정
--- 새로운 스테이지/장애물 추가 시 이 파일만 수정하면 됨

local StageConfig = {
    -- 공통 설정
    pool_size = 120,
    pool_standby_location = Vector(-2000, 0, 0),
    default_spawn_num = 3,
    default_radius = 5,

    -- 난이도 스케일링 설정
    difficulty = {
        scale_interval = 500,    -- 500m마다 밀도 증가
        spawn_increment = 1,     -- 간격마다 증가할 스폰 개수
        max_multiplier = 3.0,    -- 최대 배수 (base_spawn_num * max_multiplier)
    },

    -- 스테이지 정의 (랜덤 로테이션)
    stages = {
        {
            name = "Trees",
            base_spawn_num = 3,
            obstacles = {
                { prefab = "Data/Prefabs/Obstacle_Tree1.prefab", radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Tree2.prefab", radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Tree3.prefab", radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Tree4.prefab", radius = 5 },
            }
        },
        {
            name = "Fence",
            base_spawn_num = 3,
            obstacles = {
                { prefab = "Data/Prefabs/Obstacle_Fence1.prefab", radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Fence2.prefab", radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Fence3.prefab", radius = 5 },
            }
        },
        {
            name = "Mixed",
            base_spawn_num = 2,
            obstacles = {
                { prefab = "Data/Prefabs/Obstacle_Tree1.prefab", radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Tree2.prefab", radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Fence1.prefab", radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Fence2.prefab", radius = 5 },
            }
        },
    }
}

return StageConfig
