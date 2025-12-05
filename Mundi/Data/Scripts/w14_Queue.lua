--- constants

local Const = {}

local ConstValues = {
    DEFAULT_INDEX_UPPER_LIMIT = 1000
}
ConstValues.__index = ConstValues
ConstValues.__newindex = function(_, key, value)
    print("[error] Attempt to modify constant [%s] with value [%s]", key, tostring(value))
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
Queue.Const = Const

--- Queue 인스턴스를 생성합니다.
--- @param index_upper_limit number|nil 인덱스 상한선 (기본값: 1000)
--- @return Queue
function Queue:new(index_upper_limit)
    -- 인스턴스 생성 (필드 수정 제약 없음)
    local instance = {
        first = 1,
        last = 0,
        index_upper_limit = index_upper_limit or Const.DEFAULT_INDEX_UPPER_LIMIT,
        items = {}
    }
    setmetatable(instance, {__index = Queue})
    return instance
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

-- 모든 메서드 정의 후 클래스 프로토타입 보호
-- (__newindex는 클래스 수정만 방지, 인스턴스는 자유롭게 수정 가능)
do
    local Queue_mt = {
        __index = Queue,
        __newindex = function(_, key, value)
            print("[error] Attempt to modify Queue class [%s] with value [%s]", key, tostring(value))
        end
    }
    setmetatable(Queue, Queue_mt)
end

return Queue
