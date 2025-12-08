local Math = require("w14_Math")

local DifficultyManager = {}
DifficultyManager.__index = DifficultyManager

function DifficultyManager:new(
        difficuly_min,
        difficuly_max,
        time_interval
)
    local instance = {
        difficuly_min = difficuly_min,
        difficuly_max = difficuly_max,
        time_interval = time_interval,
        current_time = 0
    }
    setmetatable(instance, DifficultyManager)
    return instance
end

function DifficultyManager:Tick(delta)
    self.current_time = self.current_time + delta
    if (self.current_time > self.time_interval) then
        self.current_time = self.time_interval
    end
    local t = self.current_time / self.time_interval
    return math.floor(Math:lerp(self.difficuly_min, self.difficuly_max, t))
end 

function DifficultyManager:reset()
    self.current_time = 0
end

return DifficultyManager