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
--- @class Queue
--- @field first number
--- @field last number
--- @field index_upper_limit number
--- @field items table
local Queue = {}
Queue.__index = Queue
Queue.__newindex = function(_, key, value)
    print(string.format("[error] Attempt to modify constant [%s] with value [%s]", key, tostring(value)))
end
Queue.Const = Const

--- Queue 인스턴스를 생성합니다.
--- @param index_upper_limit number|nil 인덱스 상한선 (기본값: 1000)
--- @return Queue
function Queue:new(index_upper_limit)
    local obj = setmetatable({}, self)
    obj.first = 1
    obj.last = 0
    obj.index_upper_limit = index_upper_limit or Const.DEFAULT_INDEX_UPPER_LIMIT
    obj.items = {}
    return obj
end

--- 큐에 값을 추가합니다.
--- @param value any 추가할 값
--- @return void
function Queue:push(value)
    self.last = self.last + 1
    self.items[self.last] = value
end

--- 큐에서 값을 꺼냅니다.
--- @return any|nil 꺼낸 값 (큐가 비어있으면 nil)
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

--- 큐의 첫 번째 값을 반환합니다 (제거하지 않음).
--- @return any|nil 첫 번째 값
function Queue:head()
    if self.first > self.last then
        return nil
    end
    return self.items[self.first]
end

--- 큐가 비어있는지 확인합니다.
--- @return boolean
function Queue:is_empty()
    return self.first > self.last
end

return Queue
