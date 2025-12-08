--- 게임 시작 시 호출됩니다.
function BeginPlay()
    local mesh = GetComponent(Obj, "USkeletalMeshComponent")

    if not mesh then
        return
    end

    -- 애니메이션 상태 머신 활성화 및 생성
    mesh:UseStateMachine()
    local anim_instance = mesh:GetOrCreateStateMachine()
    if not anim_instance then
        return
    end

    anim_instance:AddState("Walk", "Data/GameJamAnim/Monster/ChaserZombieWalk_mixamo.com", 3.0, true)
    anim_instance:SetState("Walk", 0)

    StartCoroutine(Die)
end

function Tick(Delta)
    local MovementSpeed = 4.0
    Obj.Location = Obj.Location + Vector(MovementSpeed * Delta, 0, 0)
end

function Die()
    coroutine.yield("wait_time", 5.5)
    Obj.bIsActive = false
end