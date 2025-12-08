-- w14_Explosion.lua
-- 폭발 스크립트 - Overlap으로 몬스터 처치

local ScoreManager = require("Game/w14_ScoreManager")
local MonsterConfig = require("w14_MonsterConfig")

local DAMAGE = 9999
local SphereCollider = nil

function BeginPlay()
    -- SphereCollider 가져오기 (처음엔 비활성)
    SphereCollider = GetComponent(Obj, "USphereComponent")
    if SphereCollider then
        SphereCollider.bIsActive = false
    end

    -- 카메라 쉐이크
    local cm = GetCameraManager()
    if cm and cm.StartCameraShake then
        cm:StartCameraShake(0.5, 3.0, 4.0, 10.0)
    end

    -- 약간의 딜레이 후 콜라이더 활성화 → 잠시 후 비활성화 → 제거
    StartCoroutine(function()
        -- 파티클이 퍼지는 동안 대기
        coroutine.yield("wait_time", 0.05)

        -- 콜라이더 활성화 (Overlap 발생)
        if SphereCollider then
            SphereCollider.bIsActive = true
        end

        -- 잠깐 유지 후 비활성화
        coroutine.yield("wait_time", 0.2)
        if SphereCollider then
            SphereCollider.bIsActive = false
        end

        -- 파티클 끝날 때까지 대기 후 제거
        coroutine.yield("wait_time", 2.5)
        if Obj then
            DeleteObject(Obj)
        end
    end)
end

function OnBeginOverlap(OtherActor)
    if not OtherActor then return end

    -- 몬스터 태그 체크
    if MonsterConfig.IsMonsterTag(OtherActor.Tag) then
        local script = OtherActor:GetScript()
        if script and script.GetDamage then
            script.GetDamage(DAMAGE)
            ScoreManager.AddKill(1)
            print("[Explosion] Monster killed: " .. OtherActor.Tag)
        end
    end
end

function Tick(dt)
end
