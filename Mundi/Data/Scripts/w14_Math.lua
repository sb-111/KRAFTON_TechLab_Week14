local Math = {}

function Math:RandomFloatInRange(MinRange, MaxRange)
    return MinRange + (MaxRange - MinRange) * math.random()
end

function Math:RandomIntInRange(MinRange, MaxRange)
    -- math.random(m, n)은 m 이상 n 이하의 정수를 반환 (양 끝 포함)
    return math.random(MinRange, MaxRange)
end

return Math