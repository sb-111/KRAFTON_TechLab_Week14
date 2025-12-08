-- w14_Explosion.lua
-- 폭발 스크립트 - Overlap으로 몬스터 처치

local MonsterConfig = require("w14_MonsterConfig")
local Audio = require("Game/w14_AudioManager")

local DAMAGE = 9999
local SphereCollider = nil

function BeginPlay()
    -- SphereCollider 가져오기 (처음엔 비활성)
    SphereCollider = GetComponent(Obj, "USphereComponent")
    if SphereCollider then
        SphereCollider.bIsActive = false
    end

    -- 파티클 활성화 (bAutoActivate가 false이므로 수동으로)
    local particleComp = GetComponent(Obj, "UParticleSystemComponent")
    if particleComp then
        particleComp:Activate()
    end

    -- 카메라 쉐이크 (Duration, AmpLoc, AmpRotDeg, Frequency, Priority, BlendIn, BlendOut)
    -- BlendOut을 0.3으로 설정하여 부드럽게 회복
    local cm = GetCameraManager()
    if cm and cm.StartCameraShake then
        cm:StartCameraShake(1.5, 1.5, 3.0, 15.0, 0, 0.02, 0.3)
    end

    -- 폭발 사운드 재생
    Audio.PlaySFX("Explosion")

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
        coroutine.yield("wait_time", 1)
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
            print("[Explosion] Monster killed: " .. OtherActor.Tag)
        end
    end
end

function Tick(dt)
end
