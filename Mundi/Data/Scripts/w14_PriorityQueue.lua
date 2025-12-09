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

--- PriorityQueue 클래스 (Min-Heap 기반)
--- Location.X가 작은 물체가 head로 가도록 정렬
--- @class PriorityQueue
--- @field heap table
--- @field size number
local PriorityQueue = {}
PriorityQueue.__index = PriorityQueue
PriorityQueue.Const = Const

--- PriorityQueue 인스턴스를 생성합니다.
--- @return PriorityQueue
function PriorityQueue:new()
    local instance = {
        heap = {},
        size = 0
    }
    setmetatable(instance, PriorityQueue)
    return instance
end

--- 비교 함수: Location.X가 작은 것이 우선순위가 높음
--- @param a any
--- @param b any
--- @return boolean a가 b보다 우선순위가 높으면 true
local function compare(a, b)
    if a and b and a.Location and b.Location then
        return a.Location.X < b.Location.X
    end
    return false
end

--- 부모 인덱스 반환
--- @param index number
--- @return number
local function parent(index)
    return math.floor(index / 2)
end

--- 왼쪽 자식 인덱스 반환
--- @param index number
--- @return number
local function left_child(index)
    return index * 2
end

--- 오른쪽 자식 인덱스 반환
--- @param index number
--- @return number
local function right_child(index)
    return index * 2 + 1
end

--- 힙 위로 올리기 (삽입 시 사용)
--- @param self PriorityQueue
--- @param index number
local function heapify_up(self, index)
    while index > 1 and compare(self.heap[index], self.heap[parent(index)]) do
        -- swap
        local p = parent(index)
        self.heap[index], self.heap[p] = self.heap[p], self.heap[index]
        index = p
    end
end

--- 힙 아래로 내리기 (삭제 시 사용)
--- @param self PriorityQueue
--- @param index number
local function heapify_down(self, index)
    local smallest = index
    local l = left_child(index)
    local r = right_child(index)

    if l <= self.size and compare(self.heap[l], self.heap[smallest]) then
        smallest = l
    end

    if r <= self.size and compare(self.heap[r], self.heap[smallest]) then
        smallest = r
    end

    if smallest ~= index then
        self.heap[index], self.heap[smallest] = self.heap[smallest], self.heap[index]
        heapify_down(self, smallest)
    end
end

--- 큐에 값을 추가합니다.
--- @param value any 추가할 값
--- @return void
function PriorityQueue:push(value)
    self.size = self.size + 1
    self.heap[self.size] = value
    heapify_up(self, self.size)
end

--- 큐에서 값을 꺼냅니다 (가장 작은 Location.X를 가진 요소).
--- @return any|nil 꺼낸 값 (큐가 비어있으면 nil)
function PriorityQueue:pop()
    if self.size == 0 then
        return nil
    end

    local value = self.heap[1]
    self.heap[1] = self.heap[self.size]
    self.heap[self.size] = nil
    self.size = self.size - 1

    if self.size > 0 then
        heapify_down(self, 1)
    end

    return value
end

--- 큐의 첫 번째 값을 반환합니다 (제거하지 않음).
--- @return any|nil 첫 번째 값
function PriorityQueue:head()
    if self.size == 0 then
        return nil
    end
    return self.heap[1]
end

--- 큐가 비어있는지 확인합니다.
--- @return boolean
function PriorityQueue:is_empty()
    return self.size == 0
end

--- 큐의 크기를 반환합니다.
--- @return number
function PriorityQueue:get_size()
    return self.size
end

-- 모든 메서드 정의 후 클래스 프로토타입 보호
-- (__newindex는 클래스 수정만 방지, 인스턴스는 자유롭게 수정 가능)
do
    local PriorityQueue_mt = {
        __index = PriorityQueue,
        __newindex = function(_, key, value)
            print("[error] Attempt to modify PriorityQueue class [%s] with value [%s]", key, tostring(value))
        end
    }
    setmetatable(PriorityQueue, PriorityQueue_mt)
end

return PriorityQueue
