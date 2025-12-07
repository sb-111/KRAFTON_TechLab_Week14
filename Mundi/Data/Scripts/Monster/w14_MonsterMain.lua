--- 몬스터 메인 스크립트
--- Monster 클래스를 사용하여 게임 오브젝트에 몬스터 동작을 구현합니다.

local MonsterClass = require("Monster/w14_Monster")
local Monster = nil

--- 게임 시작 시 호출됩니다.
--- @return void
function BeginPlay()
    print("[MonsterMain] BeginPlay called")
    Monster = MonsterClass:new()

    -- 몬스터 초기화 (스탯 + 애니메이션 상태 머신 설정)
    -- Initialize(obj, move_speed, health_point, attack_point, attack_range)
    Monster:Initialize(Obj, 3.0, 100, 10, 2.5)
    print("[MonsterMain] BeginPlay finished")
end

local TickCount = 0

--- 매 프레임마다 호출됩니다.
--- @param Delta number 델타 타임
--- @return void
function Tick(Delta)
    -- Monster가 없거나 이미 죽었으면 업데이트 중지
    if not Monster or Monster.is_dead then
        return
    end

    -- 1초마다 한 번씩 현재 상태 출력 (디버그용)
    TickCount = TickCount + 1
    if TickCount % 60 == 0 then
        local currentState = Monster.anim_instance:GetCurrentStateName()
        print("[MonsterMain] Tick running... Current state: '" .. tostring(currentState) .. "' (type: " .. type(currentState) .. ")")
    end

    -- 플레이어 위치 기반 공격 체크
    Monster:CheckPlayerPositionAndAttack()

    -- 데미지/죽음 상태가 아닐 때만 이동 (현재 주석 처리)
    -- if not Monster:IsDamagedState() then
    --     MoveToPlayer(Delta)
    -- end
end

----- 플레이어를 향해 이동합니다.
----- @param Delta number 델타 타임
----- @return void
--function MoveToPlayer(Delta)
--    local player_pos = Monster:FindPlayerPosition()
--
--    -- 플레이어를 찾지 못했으면 이동 안함
--    if player_pos.X == 0 and player_pos.Y == 0 and player_pos.Z == 0 then
--        return
--    end
--
--    local offset = player_pos - Obj.Location
--    local distance_squared = offset.X * offset.X + offset.Y * offset.Y
--    local attack_range_squared = Monster.attack_range * Monster.attack_range
--
--    -- 공격 범위 밖이면 플레이어를 향해 이동
--    if distance_squared > attack_range_squared then
--        local direction = Vector(offset.X, offset.Y, 0)
--        direction:Normalize()
--
--        -- 이동
--        Obj.Location = Obj.Location + direction * Monster.move_speed * Delta
--
--        -- 몬스터가 플레이어를 바라보도록 회전
--        local yaw = math.deg(math.atan(direction.Y, direction.X))
--        Obj.Rotation = Vector(0, 0, yaw)
--    end
--end

--- 다른 액터와 충돌 시작 시 호출됩니다.
--- @param OtherActor userdata 충돌한 액터
--- @return void
function OnBeginOverlap(OtherActor)
    if not Monster then
        return
    end

    print("[MonsterMain] OnBeginOverlap called! OtherActor.Tag: " .. tostring(OtherActor.Tag))

    --- Test Code
    if OtherActor.Tag == "player" then
        print("[MonsterMain] Player collision detected!")
        Monster:GetDamage(10)
        print("[MonsterMain] After damage - HP: " .. Monster.health_point)
    end

    ---- 플레이어의 총알과 충돌 처리
    --if OtherActor.Tag == "bullet" then
    --    Monster:GetDamage(10)
    --    print("[MonsterMain] Hit! HP: " .. Monster.health_point)
    --end
end

--- 게임 종료 시 호출됩니다.
--- @return void
function EndPlay()
    bIsActive = false
    Monster = nil
end
