local MonsterUtil = {}

function MonsterUtil:FindPlayerPosition()
    local playerObj = FindObjectByName("player")
    if playerObj ~= nil then
        return playerObj.Location
    end
    return Vector(0, 0, 0)
end 

return MonsterUtil