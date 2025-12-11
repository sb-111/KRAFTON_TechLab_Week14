--- BossBeamBreathHitBox 스크립트
--- 보스 빔 공격의 데미지 히트박스
--- 일정 시간 후 자동 삭제됩니다.

local ElapsedTime = 0
local LIFETIME = 2.5  -- 빔 지속 시간 + 여유
local Damage = 25     -- 기본 데미지 (Initialize로 덮어씀)

function BeginPlay()
    ElapsedTime = 0
    print("[BossBeamBreathHitBox] Spawned")
end

function Tick(Delta)
    ElapsedTime = ElapsedTime + Delta

    if ElapsedTime >= LIFETIME then
        DeleteObject(Obj)
    end
end

--- 초기화 (보스가 스폰할 때 호출)
--- @param damage number 보스 공격력
--- @return void
function Initialize(damage)
    if damage then
        Damage = damage
    end
    print("[BossBeamBreathHitBox] Initialized with damage: " .. Damage)
end

--- 데미지 반환 (플레이어 피격 시 사용)
--- @return number
function GetDamageAmount()
    return Damage
end

function EndPlay()
    print("[BossBeamBreathHitBox] Destroyed")
end
