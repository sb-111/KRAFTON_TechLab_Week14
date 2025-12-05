--- constants

local Const = {}

local ConstValues = {
    DEFAULT_INDEX_UPPER_LIMIT = 1000
}
ConstValues.__index = ConstValues
ConstValues.__newindex = function(_, key, value)
    print(string.format("[error] Attempt to modify constant [%s] with value [%s]", key, tostring(value)))
end

-- Const를 불변으로 만든다
setmetatable(Const, ConstValues)

--- Queue 클래스
local Queue = {}
Queue.__index = Queue
Queue.Const = Const

function Queue:new(index_upper_limit)
    local obj = setmetatable({}, self)
    obj.first = 1
    obj.last = 0
    obj.index_upper_limit = index_upper_limit or Const.DEFAULT_INDEX_UPPER_LIMIT
    obj.items = {}
    return obj
end

function Queue:push(value)
    self.last = self.last + 1
    self.items[self.last] = value
end

function Queue:pop()
    if self.first > self.last then
        return nil
    end

    local value = self.items[self.first]
    self.items[self.first] = nil
    self.first = self.first + 1

    if self.first > self.index_upper_limit then
        local newItems = {}
        for i = self.first, self.last do
            newItems[#newItems + 1] = self.items[i]
        end
        self.first = 1
        self.last = #newItems
        self.items = newItems
    end

    return value
end

function Queue:head()
    if self.first > self.last then
        return nil
    end
    return self.items[self.first]
end

function Queue:isEmpty()
    return self.first > self.last
end

return Queue
