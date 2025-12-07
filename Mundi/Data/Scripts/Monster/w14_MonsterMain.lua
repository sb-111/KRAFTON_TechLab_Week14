--- 몬스터 메인 스크립트
--- Monster 클래스를 사용하여 게임 오브젝트에 몬스터 동작을 구현합니다.

local MonsterClass = require("Monster/w14_Monster")
local Monster = nil

--- Physics를 재초기화하는 함수 (ObjectPool 재사용 시)
local function ReinitializePhysics()
    print("[DEBUG Monster] ReinitializePhysics - Name: " .. tostring(Obj.Name))

    -- SkeletalMeshComponent의 Physics State를 완전히 재생성
    local SkeletalMesh = GetComponent(Obj, "USkeletalMeshComponent")
    if SkeletalMesh then
        -- 기존 Physics 완전 파괴 후 재생성 (Bodies의 위치가 새 Actor 위치로 업데이트됨)
        if SkeletalMesh.DestroyPhysicsState then
            SkeletalMesh:DestroyPhysicsState()
            print("[DEBUG Monster] SkeletalMesh DestroyPhysicsState called")
        end

        if SkeletalMesh.CreatePhysicsState then
            SkeletalMesh:CreatePhysicsState()
            print("[DEBUG Monster] SkeletalMesh CreatePhysicsState called")
        end
    end

    -- CapsuleComponent도 재초기화
    local CapsuleComp = GetComponent(Obj, "UCapsuleComponent")
    if CapsuleComp then
        CapsuleComp.CollisionEnabled = 0
        CapsuleComp.CollisionEnabled = 3
        CapsuleComp.BlockAll = true
        print("[DEBUG Monster] CapsuleComponent reinitialized")
    end
end

--- 게임 시작 시 호출됩니다.
--- @return void
function BeginPlay()
    Monster = MonsterClass:new()

    -- 몬스터 초기화 (스탯 + 애니메이션 상태 머신 설정)
    -- Initialize(obj, move_speed, health_point, attack_point, attack_range)
    Monster:Initialize(Obj, 3.0, 50, 10, 4.0)

    -- 이 인스턴스의 이전 활성 상태를 Monster 객체에 저장 (인스턴스별로 독립적)
    Monster.bWasActive = Obj.bIsActive

    -- 초기 Physics 설정
    ReinitializePhysics()
end

--- 매 프레임마다 호출됩니다.
--- @param Delta number 델타 타임
--- @return void
function Tick(Delta)
    -- Monster가 없으면 초기화되지 않은 상태
    if not Monster then
        return
    end

    -- ObjectPool에서 재활성화되었는지 체크 (false -> true 전환)
    -- Monster.bWasActive를 사용하여 인스턴스별로 독립적으로 추적
    if not Monster.bWasActive and Obj.bIsActive then
        print("[DEBUG Monster] Detected reactivation from ObjectPool! Name: " .. tostring(Obj.Name))
        ReinitializePhysics()

        -- Monster 상태도 재초기화
        Monster.is_dead = false
        Monster.current_health = Monster.max_health
    end
    Monster.bWasActive = Obj.bIsActive

    -- 비활성 상태면 업데이트 중지
    if not Obj.bIsActive then
        return
    end

    -- Monster가 이미 죽었으면 업데이트 중지
    if Monster.is_dead then
        return
    end

    -- 애니메이션 상태 업데이트 (Attack, Damaged 끝나면 Idle로 복귀)
    Monster:UpdateAnimationState()

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

    -- 플레이어의 총알과 충돌 처리
    if OtherActor.Tag == "bullet" then
        Monster:GetDamage(10)
    end
end

--- 게임 종료 시 호출됩니다.
--- @return void
function EndPlay()
    bIsActive = false
    Monster = nil
end
