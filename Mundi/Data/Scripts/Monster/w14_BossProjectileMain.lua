--[[
    w14_BossProjectileMain.lua

    BossProjectile 프리팹에 붙이는 메인 스크립트
    - 자체적으로 플레이어 추적, 파괴, 데미지 처리
]]

local BossProjectileClass = require("Monster/w14_BossProjectile")
local BossProjectile = nil

--- 게임 시작 시 호출됩니다.
--- @return void
function BeginPlay()
    BossProjectile = BossProjectileClass:new(Obj)
end

--- 매 프레임마다 호출됩니다.
--- @param Delta number 델타 타임
--- @return void
function Tick(Delta)
    if not BossProjectile then
        return
    end

    BossProjectile:Tick(Delta)

    -- 파괴 조건 충족 시 자기 자신 파괴
    if BossProjectile:ShouldDestroy() then
        DeleteObject(Obj)
    end
end

--- 초기화 (보스가 스폰할 때 호출)
--- @param speed number 이동 속도
--- @param damage number 데미지
--- @return void
function Initialize(speed, damage)
    if BossProjectile then
        BossProjectile:Initialize(speed, damage)
    end
end

--- 데미지 반환 (플레이어 피격 시 사용)
--- @return number
function GetDamageAmount()
    if BossProjectile then
        return BossProjectile:GetDamageAmount()
    end
    return 10
end

--- 발사체 파괴 (플레이어 사격 시 호출)
--- @return void
function DestroyProjectile()
    if BossProjectile then
        BossProjectile:Destroy()
    end
end

--- 게임 종료 시 호출됩니다.
--- @return void
function EndPlay()
    BossProjectile = nil
end
