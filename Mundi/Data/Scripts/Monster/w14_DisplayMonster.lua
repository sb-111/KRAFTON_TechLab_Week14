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

    anim_instance:AddState("Attack", "Data/GameJamAnim/Monster/BasicZombieIdle_mixamo.com", 1.0, true)
    anim_instance:SetState("Attack", 0)
end