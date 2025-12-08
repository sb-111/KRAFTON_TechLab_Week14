local MovementSpeed = 3.0
local ElapsedTime = 0
local RunDuration = 3.0
local IsRunning = true
local IsSurvivor = nil  -- nil = 아직 매니저가 지정 안함
local StateMachine = nil
local HasCheckedSurvival = false

function BeginPlay()
    local mesh = GetComponent(Obj, "USkeletalMeshComponent")
    local Gun = GetComponent(Obj, "UStaticMeshComponent")
    Gun:SetupAttachment(mesh, "mixamorig6:RightHand")

    mesh:UseStateMachine()
    StateMachine = mesh:GetOrCreateStateMachine()
    if not StateMachine then return end

    StateMachine:AddState("Run", "Data/GameJamAnim/Player/Rifle_Run_mixamo.com", 1.0, true)
    StateMachine:AddState("Die", "Data/GameJamAnim/Player/Rifle_Die_mixamo.com", 1.0, false)
    StateMachine:SetState("Run", 0)

    StartCoroutine(Die)
end

-- CinematicManager가 호출하여 생존 여부 지정
function SetSurvivorStatus(isSurvivor)
    IsSurvivor = isSurvivor
end

function Tick(Delta)
    if IsRunning then
        Obj.Location = Obj.Location + Vector(MovementSpeed * Delta, 0, 0)

        -- 아직 생존 확인을 안 했고, 매니저가 생존 여부를 지정했을 때
        if not HasCheckedSurvival and IsSurvivor ~= nil then
            ElapsedTime = ElapsedTime + Delta

            if ElapsedTime >= RunDuration then
                HasCheckedSurvival = true

                if not IsSurvivor then
                    IsRunning = false
                    if StateMachine then
                        StateMachine:SetState("Die", 0)
                    end
                end
            end
        end
    end
end

function Die()
    coroutine.yield("wait_time", 7.5)
    Obj.bIsActive = false
end