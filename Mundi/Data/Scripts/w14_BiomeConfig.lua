--- @class BiomeConfig
--- 바이옴별 장애물 설정
--- 새로운 바이옴/장애물 추가 시 이 파일만 수정하면 됨

local BiomeConfig = {
    -- 공통 설정
    pool_size = 120,
    pool_standby_location = Vector(-2000, 0, 0),
    default_spawn_num = 3,
    default_radius = 5,

    -- 바이옴 정의
    biomes = {
        {
            name = "Forest",
            obstacles = {
                { prefab = "Data/Prefabs/Obstacle_Tree1.prefab", spawn_num = 3, radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Tree2.prefab", spawn_num = 3, radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Tree3.prefab", spawn_num = 3, radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Tree4.prefab", spawn_num = 3, radius = 5 },
            }
        },
        {
            name = "Fence",
            obstacles = {
                { prefab = "Data/Prefabs/Obstacle_Fence1.prefab", spawn_num = 3, radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Fence2.prefab", spawn_num = 3, radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Fence3.prefab", spawn_num = 3, radius = 5 },
            }
        },
        {
            name = "Mixed",
            obstacles = {
                { prefab = "Data/Prefabs/Obstacle_Tree1.prefab", spawn_num = 2, radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Tree2.prefab", spawn_num = 2, radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Fence1.prefab", spawn_num = 2, radius = 5 },
                { prefab = "Data/Prefabs/Obstacle_Fence2.prefab", spawn_num = 2, radius = 5 },
            }
        },
    }
}

return BiomeConfig
