local Math = require("w14_Math")
local MapConfig = require("w14_MapConfig")

--- @class ObjectPlacer
--- @field placed table<userdata> 배치된 물체들의 정보를 저장해놓는 테이블
--- @field area_width number Spawn을 적용할 면적의 너비
--- @field area_height number Spawn을 적용할 면적의 높이
local ObjectPlacer = {}
ObjectPlacer.__index = ObjectPlacer

--- ObjectPlacer의 instance 반환
--- @param placed_max number placed에 저장할 최대 요소 수
--- @param area_width number Spawn을 적용할 면적의 너비
--- @param area_height number Spawn을 적용할 면적의 높이
--- @param area_height_offset number Spawn을 적용할 면적의 높이 offset
--- @return ObjectPlacer
function ObjectPlacer:new(
        area_width,
        area_height,
        area_height_offset,
        area_width_offset,
        placed_max
)
    local instance = {
        placed = {},
        placed_max = placed_max,
        area_width = area_width,
        area_height = area_height,
        area_width_offset = area_width_offset,
        area_height_offset = area_height_offset
    }

    setmetatable(instance, ObjectPlacer)
    return instance
end

--- 물체를 정해진 영역 내에 overlap 없이 배치하는 함수
--- @param radius number 배치할 물체의 반지름
--- @return userdata 배치된 위치 (Vector)
function ObjectPlacer:place_object(radius)
    -- tries: 현재 객체에 대해 랜덤 시도한 횟수. 횟수 제약을 두어 무한 루프 방지.
    local tries = 1

    -- 최대 50번의 랜덤 시도(임계값) 동안 겹치지 않는 위치를 찾는다.
    -- 이 값은 실험적으로 조정 가능(큰 밀도에서는 늘려야 함).
    while tries <= 50 do
        -- 영역 내 랜덤 좌표 생성 (균일 분포)
        -- area_width는 Y축(좌우), area_height는 X축(전진)
        local y_pos = Math:RandomInRange(
                -self.area_width * 0.5 + self.area_width_offset,
                self.area_width * 0.5 + self.area_width_offset
        )
        local x_pos = Math:RandomInRange(
                -self.area_height * 0.5 + self.area_height_offset,
                self.area_height * 0.5 + self.area_height_offset
        )

        -- ok가 true면 현재 좌표에서 기존에 배치된 것들과 충돌이 없는 상태
        local ok = true

        -- 이미 배치된 모든 객체와 충돌 검사(원-원 충돌: 중심 거리 비교)
        for _, p in ipairs(self.placed) do
            -- 두 객체가 겹치지 않기 위해 필요한 최소 거리 = 현재 반지름 + 기존 객체의 반지름
            local min_dist = radius + p.radius

            -- 중심 간 거리의 제곱 (제곱근을 쓰지 않고 비교하기 위해 제곱으로 비교)
            local dx = x_pos - p.x
            local dy = y_pos - p.y

            -- 만약 중심간 거리 제곱이 (min_dist)^2 보다 작으면 겹침 -> 실패 처리
            if dx * dx + dy * dy < min_dist * min_dist then
                ok = false
                break -- 이미 충돌이 있으므로 더 비교할 필요 없음
            end
        end

        -- 모든 기존 배치와 충돌이 없으면 이 위치로 확정하고 placed에 추가
        -- 시도 횟수가 50을 초과하면 포기하고 그냥 배치한다.
        if ok or tries >= 50 then
            table.insert(self.placed, { x = x_pos, y = y_pos, radius = radius })
            return Vector(x_pos, y_pos, 0)
        end

        -- 실패한 시도는 카운트 증가. 50번 넘으면 포기하고 다음 객체로 넘어감.
        tries = tries + 1
    end
end

function ObjectPlacer:update_area(area_width, area_height, area_width_offset, area_height_offset)
    self.area_width = area_width
    self.area_height = area_height
    self.area_width_offset = area_width_offset
    self.area_height_offset = area_height_offset

    local gap = #self.placed - self.placed_max
    if gap > 0 then
        for i=1, gap do
            table.remove(self.placed, 1)
        end
    end
end

return ObjectPlacer
