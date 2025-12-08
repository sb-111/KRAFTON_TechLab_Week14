--- 몬스터 메인 스크립트
--- Monster 클래스를 사용하여 게임 오브젝트에 몬스터 동작을 구현합니다.

local MonsterClass = require("Monster/w14_Monster")
local Monster = nil
local PrevActive = false

--- 게임 시작 시 호출됩니다.
--- @return void
function BeginPlay()
    Monster = MonsterClass:new()

    -- 몬스터 초기화 (스탯 + 애니메이션 상태 머신 설정)
    -- Initialize(obj, move_speed, health_point, attack_point, attack_range)
    Monster:Initialize(Obj, 3.0, 20, 1, 10.0)

    PrevActive = Obj.bIsActive
end

--- 매 프레임마다 호출됩니다.
--- @param Delta number 델타 타임
--- @return void
function Tick(Delta)
    -- Respawn 감지: 비활성 -> 활성 전환 시 Monster 재초기화
    if not PrevActive and Obj.bIsActive and Monster then
        Monster:Reset()
    end
    PrevActive = Obj.bIsActive

    -- Monster가 없거나 이미 죽었으면 업데이트 중지
    if not Monster or Monster.stat.is_dead then
        return
    end

    -- 애니메이션 상태 업데이트 (Attack, Damaged 끝나면 Idle로 복귀)
    Monster:UpdateAnimationState()

    -- 플레이어 위치 기반 공격 체크
    Monster:CheckPlayerPositionAndAttack()
end

--- 다른 액터와 충돌 시작 시 호출됩니다.
--- @param OtherActor userdata 충돌한 액터
--- @return void
function OnBeginOverlap(OtherActor)
    if not Monster then
        return
    end
end

function GetDamage(Damage)
    Monster:GetDamage(Damage)
end

--- Pool에서 Respawn 시 호출됩니다.
--- @return void
function Reset()
    if Monster then
        Monster:Reset()
    end
    PrevActive = true
end

--- 게임 종료 시 호출됩니다.
--- @return void
function EndPlay()
    bIsActive = false
    Monster = nil
end
