local Math = {}

function Math:RandomInRange(MinRange, MaxRange)
    return MinRange + (MaxRange - MinRange) * math.random()
end

function Math:RandomIntInRange(MinRange, MaxRange)
    -- math.random(m, n)은 m 이상 n 이하의 정수를 반환 (양 끝 포함)
    return math.random(MinRange, MaxRange)
end

function Math:clamp(min, max, t)
    if t < min then
        t = min
    end
    if t > max then
        t = max
    end
    return t
end

function Math:lerp(min, max, t)
    return (1 - t) * min + t * max
end

return Math