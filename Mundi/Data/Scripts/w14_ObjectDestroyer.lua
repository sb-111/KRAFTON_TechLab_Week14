local ObjectDestroyer = {}
ObjectDestroyer.__index = ObjectDestroyer

function ObjectDestroyer:new(time_to_destroy, obj_to_destroy)
    local instance = {
        time_to_destroy = time_to_destroy,
        obj_to_destroy = obj_to_destroy,
        timer = 0
    }
    setmetatable(instance, ObjectDestroyer)
    
    return instance
end

function ObjectDestroyer:destroy_if_time_is_over(delta)
    self.timer = self.timer + delta
    if (self.timer >= self.time_to_destroy) then
        DeleteObject(self.obj_to_destroy)
    end
end 

return ObjectDestroyer