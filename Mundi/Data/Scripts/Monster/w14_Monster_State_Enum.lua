local MonsterState = {}
MonsterState.__index = MonsterState

function MonsterState:new()
    local instance = {
        IDLE = "IDLE",
        LEFT_ATTACK = "LEFT_ATTACK",
        RIGHT_ATTACK = "RIGHT_ATTACK",
        DIE = "DIE",
        DAMAGED = "DAMAGED"
    }
    setmetatable(instance, MonsterState)
    return instance
end

return MonsterState