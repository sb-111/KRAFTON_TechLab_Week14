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

--- 벡터의 길이(크기)를 계산합니다.
--- @param vec Vector 벡터
--- @return number 벡터의 길이
function Math:length_vector(vec)
    return math.sqrt(vec.X * vec.X + vec.Y * vec.Y + vec.Z * vec.Z)
end

--- 벡터를 정규화합니다 (길이가 1인 단위 벡터로 변환).
--- @param vec Vector 벡터
--- @return Vector 정규화된 벡터
function Math:normalize_vector(vec)
    local len = self:length_vector(vec)
    if len > 0 then
        return Vector(vec.X / len, vec.Y / len, vec.Z / len)
    end
    return Vector(0, 0, 0)
end

return Math