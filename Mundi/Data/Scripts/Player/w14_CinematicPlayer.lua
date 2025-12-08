local MovementSpeed = 3.0
local ElapsedTime = 0
local RunDuration = 3.0
local IsRunning = true
local IsSurvivor = false
local StateMachine = nil
local HasCheckedSurvival = false -- [추가] 생존 확인을 했는지 체크하는 변수

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

function Tick(Delta)        
    -- IsRunning이 true면 계속 앞으로 갑니다.
    if IsRunning then
        
        -- [이동 로직] 죽지 않았다면 계속 실행됨
        Obj.Location = Obj.Location + Vector(MovementSpeed * Delta, 0, 0)
        
        -- 아직 생존 확인을 안 했을 때만 시간을 잽니다.
        if not HasCheckedSurvival then
            ElapsedTime = ElapsedTime + Delta
            
            -- 3초가 지났으면 운명의 시간
            if ElapsedTime >= RunDuration then
                HasCheckedSurvival = true -- [중요] 이제 다시는 이 if문에 들어오지 않습니다.
                
                -- 25% 확률 생존 게임
                if math.random() <= 0.25 then
                    IsSurvivor = true
                    print(Obj.Name .. " 생존! 계속 달린다!")
                    -- [포인트] 여기서 IsRunning을 끄지 않습니다. 그래서 계속 달립니다.
                else
                    IsSurvivor = false
                    print(Obj.Name .. " 사망...")
                    
                    -- 사망했을 때만 멈춥니다.
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
    coroutine.yield("wait_time", 5.5)
    Obj.bIsActive = false
end