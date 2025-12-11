--- Monster Tag별 데미지 설정
--- @class MonsterConfig

local MonsterConfig = {
    -- BasicMonster
    BasicMonster = {
        damage = 10,
        knockBackStrength = 6.0,
        bloodColor = "red",  
        description = "기본 좀비"
    },

    -- ChaserMonster
    ChaserMonster = {
        damage = 15,
        knockBackStrength = 10.0,
        bloodColor = "red",
        description = "추격 좀비"
    },

    -- BoomerMonster
    BoomerMonster = {
        damage = 25,
        knockBackStrength = 12.0,
        bloodColor = "green",
        description = "폭발 좀비"
    },

    -- BoomerExplosion (폭발 범위)
    BoomerExplosion = {
        damage = 30,
        knockBackStrength = 15.0,
        bloodColor = "green",
        bloodless = true,
        description = "폭발 좀비 폭발"
    },

    -- BossMonster
    BossMonster = {
        damage = 20,
        knockBackStrength = 15.0,
        bloodColor = "black",
        description = "보스 몬스터"
    },

    -- BossProjectile (보스 발사체)
    BossProjectile = {
        damage = 15,
        knockBackStrength = 8.0,
        bloodColor = "red",
        bloodless = true,
        description = "보스 발사체"
    },

    -- BossBeamBreathHitBox (보스 빔 공격 히트박스)
    BossBeamBreathHitBox = {
        damage = 25,
        knockBackStrength = 10.0,
        bloodColor = "red",
        bloodless = true,
        description = "보스 빔 공격"
    },

    -- 기본값 (태그가 정의되지 않은 경우)
    Default = {
        damage = 5,
        knockBackStrength = 5.0,
        bloodColor = "green",    
        description = "알 수 없는 몬스터"
    }
}

--- 몬스터 태그로 데미지 조회
--- @param tag string 몬스터 태그
--- @return number 데미지
function MonsterConfig.GetDamage(tag)
    local config = MonsterConfig[tag] or MonsterConfig.Default
    return config.damage
end

--- 몬스터 태그로 KnockBack 강도 조회
--- @param tag string 몬스터 태그
--- @return number KnockBack 강도
function MonsterConfig.GetKnockBackStrength(tag)
    local config = MonsterConfig[tag] or MonsterConfig.Default
    return config.knockBackStrength
end

--- 몬스터 태그로 피 색상 조회
--- @param tag string 몬스터 태그
--- @return string "red" 또는 "green"
function MonsterConfig.GetBloodColor(tag)
    local config = MonsterConfig[tag] or MonsterConfig.Default
    return config.bloodColor
end

--- 몬스터 태그가 피가 없는(bloodless) 타입인지 확인
--- @param tag string 몬스터 태그
--- @return boolean
function MonsterConfig.IsBloodless(tag)
    local config = MonsterConfig[tag] or MonsterConfig.Default
    return config.bloodless == true
end

--- 몬스터 태그가 유효한 몬스터인지 확인
--- @param tag string 확인할 태그
--- @return boolean
function MonsterConfig.IsMonsterTag(tag)
    return MonsterConfig[tag] ~= nil
end

--- 모든 몬스터 태그 목록 반환
--- @return table 몬스터 태그 목록
function MonsterConfig.GetAllMonsterTags()
    local tags = {}
    for key, value in pairs(MonsterConfig) do
        if type(value) == "table" and key ~= "Default" then
            table.insert(tags, key)
        end
    end
    return tags
end

return MonsterConfig
