--- ChaserMonster 메인 스크립트
--- ChaserMonster 클래스를 사용하여 게임 오브젝트에 몬스터 동작을 구현합니다.

local ChaserMonsterClass = require("Monster/w14_ChaserMonster")
local MonsterConfig = require("w14_MonsterConfig")
local ChaserMonster = nil
local PrevActive = false

--- 게임 시작 시 호출됩니다.
--- @return void
function BeginPlay()
    ChaserMonster = ChaserMonsterClass:new()

    -- 몬스터 초기화 (스탯 + 애니메이션 상태 머신 설정)
    -- Initialize(obj, move_speed, health_point, attack_point, attack_range)
    ChaserMonster:Initialize(Obj, 0.1, 80, 3, 10.0)

    PrevActive = Obj.bIsActive
end

--- 매 프레임마다 호출됩니다.
--- @param Delta number 델타 타임
--- @return void
function Tick(Delta)
    -- Respawn 감지: 비활성 -> 활성 전환 시 ChaserMonster 재초기화
    if not PrevActive and Obj.bIsActive and ChaserMonster then
        ChaserMonster:Reset()
    end
    PrevActive = Obj.bIsActive

    -- ChaserMonster가 없거나 이미 죽었으면 업데이트 중지
    if not ChaserMonster or ChaserMonster.stat.is_dead then
        return
    end

    -- 애니메이션 상태 업데이트 (Attack, Damaged 끝나면 Walk로 복귀)
    ChaserMonster:UpdateAnimationState()

    -- 플레이어 위치 기반 공격 체크
    ChaserMonster:CheckPlayerPositionAndAttack()

    -- 데미지/죽음 상태가 아닐 때만 이동
    if not ChaserMonster:IsDamagedState() then
        ChaserMonster:MoveToPlayer(Delta)
    end
end

--- 다른 액터와 충돌 시작 시 호출됩니다.
--- @param OtherActor userdata 충돌한 액터
--- @return void
function OnBeginOverlap(OtherActor)
    if not ChaserMonster then
        return
    end

    -- 플레이어의 총알과 충돌 처리
    if OtherActor.Tag == "bullet" then
        ChaserMonster:GetDamage(10)
    end

    -- obstacle 또는 다른 몬스터와 충돌 시 영구 Idle 상태로 전환
    if OtherActor.Tag == "obstacle" or MonsterConfig.IsMonsterTag(OtherActor.Tag) then
        ChaserMonster:SetPermanentIdle()
    end
end

function GetDamage(Damage)
    ChaserMonster:GetDamage(Damage)
end

--- Pool에서 Respawn 시 호출됩니다.
--- @return void
function Reset()
    if ChaserMonster then
        ChaserMonster:Reset()
    end
    PrevActive = true
end

--- 게임 종료 시 호출됩니다.
--- @return void
function EndPlay()
    bIsActive = false
    ChaserMonster = nil
end
