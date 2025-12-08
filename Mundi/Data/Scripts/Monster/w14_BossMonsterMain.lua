--- BossMonster 메인 스크립트
--- BossMonster 클래스를 사용하여 게임 오브젝트에 몬스터 동작을 구현합니다.

local BossMonsterClass = require("Monster/w14_BossMonster")
local MonsterConfig = require("w14_MonsterConfig")
local BossMonster = nil

local should_destroy = false

--- 게임 시작 시 호출됩니다.
--- @return void
function BeginPlay()
    BossMonster = BossMonsterClass:new(Obj)
end

--- 매 프레임마다 호출됩니다.
--- @param Delta number 델타 타임
--- @return void
function Tick(Delta)
    -- BossMonster가 없거나 이미 죽었으면 업데이트 중지
    if not BossMonster or BossMonster.stat.is_dead then
        return
    end

    -- BossMonster:Tick()에서 이동, 공격, 애니메이션 상태 전환 모두 처리
    BossMonster:Tick(Delta)
end

--- 다른 액터와 충돌 시작 시 호출됩니다.
--- @param OtherActor userdata 충돌한 액터
--- @return void
function OnBeginOverlap(OtherActor)
    if not BossMonster then
        return
    end

    if OtherActor.Tag == "ground" then
        should_destroy = true
    end
    
    -- obstacle 또는 다른 몬스터와 충돌 시 다른 이동 위치를 정해 이동(보류)
    --if OtherActor.Tag == "obstacle" or MonsterConfig.IsMonsterTag(OtherActor.Tag) then
    --    BossMonster:GetNextPosition()
    --end
end

function GetDamage(Damage)
    BossMonster:GetDamage(Damage)
end

function ShouldDestroy()
    return should_destroy
end

function Initialize(health_point, attack_point)
    -- 스탯 초기화 (애니메이션은 BeginPlay에서 이미 설정됨)
    BossMonster:Initialize(health_point, attack_point)
end

--- BossMonster는 Pool에서 Respawn되지 않음
--- @return void
--function Reset()
--    if BossMonster then
--        BossMonster:Reset()
--    end
--end

--- 게임 종료 시 호출됩니다.
--- @return void
function EndPlay()
    BossMonster = nil
end
