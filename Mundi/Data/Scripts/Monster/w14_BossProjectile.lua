--[[
    w14_BossProjectile.lua

    보스 몬스터가 발사하는 투사체
    - 플레이어를 추적하며 이동
    - 플레이어 X 좌표보다 뒤로 가면 자동 파괴
    - 플레이어와 충돌 시 데미지
]]

local Math = require("w14_Math")

---------------------------------------------------------------------------
-- 상수 정의
---------------------------------------------------------------------------
local DEFAULT_SPEED = 8          -- 기본 이동 속도
local DEFAULT_DAMAGE = 10        -- 기본 데미지

---------------------------------------------------------------------------
-- BossProjectile 클래스 정의
---------------------------------------------------------------------------
--- @class BossProjectile
local BossProjectile = {}
BossProjectile.__index = BossProjectile

--- BossProjectile 인스턴스를 생성합니다.
--- @param obj userdata 게임 오브젝트
--- @return BossProjectile
function BossProjectile:new(obj)
    local instance = {}
    instance.obj = obj
    instance.speed = DEFAULT_SPEED
    instance.damage = DEFAULT_DAMAGE
    instance.is_destroyed = false
    setmetatable(instance, BossProjectile)
    return instance
end

--- 발사체를 초기화합니다.
--- @param speed number 이동 속도
--- @param damage number 데미지
--- @return void
function BossProjectile:Initialize(speed, damage)
    self.speed = speed or DEFAULT_SPEED
    self.damage = damage or DEFAULT_DAMAGE
end

--- 플레이어 위치를 찾아 반환합니다.
--- @return userdata 플레이어 위치
function BossProjectile:FindPlayerPosition()
    local playerObj = FindObjectByName("player")
    if playerObj ~= nil then
        return playerObj.Location
    end
    return Vector(0, 0, 0)
end

--- 매 프레임 업데이트
--- @param delta number 델타 타임
--- @return void
function BossProjectile:Tick(delta)
    if self.is_destroyed then
        return
    end

    local player_pos = self:FindPlayerPosition()
    local my_pos = self.obj.Location

    -- 플레이어 X 좌표보다 뒤로 가면 파괴
    if my_pos.X < player_pos.X then
        self.is_destroyed = true
        return
    end

    -- 플레이어 방향으로 이동
    local direction = player_pos - my_pos
    local distance = Math:length_vector(direction)

    if distance > 0.1 then
        local normalized_dir = Math:normalize_vector(direction)
        self.obj.Location = my_pos + normalized_dir * self.speed * delta
    end
end

--- 데미지를 반환합니다.
--- @return number 데미지
function BossProjectile:GetDamageAmount()
    return self.damage
end

--- 파괴 여부를 반환합니다.
--- @return boolean
function BossProjectile:ShouldDestroy()
    return self.is_destroyed
end

--- 파괴 플래그를 설정합니다.
--- @return void
function BossProjectile:Destroy()
    self.is_destroyed = true
end

return BossProjectile
