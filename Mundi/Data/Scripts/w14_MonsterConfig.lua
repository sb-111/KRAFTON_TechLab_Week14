--- Monster Tag별 데미지 설정
--- @class MonsterConfig

local MonsterConfig = {
    -- BasicMonster
    BasicMonster = {
        damage = 10,
        description = "기본 좀비"
    },

    -- ChaserMonster
    ChaserMonster = {
        damage = 15,
        description = "추격 좀비"
    },

    -- 기본값 (태그가 정의되지 않은 경우)
    Default = {
        damage = 5,
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
