-- Player/w14_PlayerADS.lua
-- ADS (Aim Down Sight) 시스템
-- 확장성 고려: 나중에 애니메이션 추가 가능

local PlayerADS = {}
PlayerADS.__index = PlayerADS

function PlayerADS:new(playerCamera, gunMesh, playerObj, skeletalMesh)
    local instance = setmetatable({}, PlayerADS)

    -- 참조
    instance.camera = playerCamera
    instance.gunMesh = gunMesh
    instance.skeletalMesh = skeletalMesh  -- 플레이어 손/팔 메시
    instance.playerObj = playerObj  -- 플레이어 오브젝트 (회전값 참조용)
    instance.ironSightActor = nil

    -- ADS 상태
    instance.isAiming = false
    instance.adsProgress = 0.0  -- 0 = 힙파이어, 1 = 완전 ADS

    -- 설정값
    instance.defaultFOV = 90.0
    instance.adsFOV = 55.0
    instance.adsSpeed = 10.0
    instance.sensitivityMult = 0.5

    -- 아이언사이트 위치 (카메라 기준 오프셋: X=앞, Y=좌우, Z=상하)
    instance.ironSightOffset = Vector(0.6, 0, -0.15)  -- 카메라 앞, 약간 아래로
    instance.ironSightScale = Vector(0.01, 0.01, 0.01)  -- 스케일 조정 필요

    return instance
end

--- 아이언사이트 액터 초기화
function PlayerADS:InitIronSight(prefabPath)
    self.ironSightActor = SpawnPrefab(prefabPath)
    if self.ironSightActor then
        self.ironSightActor.bIsActive = false  -- 초기엔 비활성화
        self.ironSightActor.Scale = self.ironSightScale
        print("[PlayerADS] Iron sight initialized")
    else
        print("[PlayerADS] Failed to spawn iron sight prefab: " .. prefabPath)
    end
end

--- 매 프레임 업데이트
function PlayerADS:Update(deltaTime, isZoomPressed)
    -- ADS 상태 업데이트
    self.isAiming = isZoomPressed

    -- ADS 진행도 보간
    local targetProgress = isZoomPressed and 1.0 or 0.0
    self.adsProgress = self.adsProgress + (targetProgress - self.adsProgress) * self.adsSpeed * deltaTime

    -- 아주 작은 값은 정리
    if self.adsProgress < 0.01 then self.adsProgress = 0.0 end
    if self.adsProgress > 0.99 then self.adsProgress = 1.0 end

    -- FOV 업데이트
    self:UpdateFOV()

    -- 아이언사이트 위치/가시성 업데이트
    self:UpdateIronSight()

    -- 총 가시성 업데이트
    self:UpdateGunVisibility()
end

--- FOV 보간
function PlayerADS:UpdateFOV()
    if not self.camera then return end

    local currentFOV = self.defaultFOV + (self.adsFOV - self.defaultFOV) * self.adsProgress
    self.camera.FieldOfView = currentFOV
end

--- 아이언사이트 위치/가시성
function PlayerADS:UpdateIronSight()
    if not self.ironSightActor or not self.camera then return end

    -- ADS 중이면 보이기
    local shouldShow = self.adsProgress > 0.1
    self.ironSightActor.bIsActive = shouldShow

    if shouldShow then
        -- 카메라 앞에 위치시키기
        local camPos = self.camera:GetWorldLocation()
        local camForward = self.camera:GetForward()

        -- 플레이어 회전에서 Right/Up 벡터 계산
        local playerRot = self.playerObj and self.playerObj.Rotation or Vector(0, 0, 0)
        local yawRad = math.rad(playerRot.Z)
        local pitchRad = math.rad(playerRot.Y)

        -- Right 벡터 (Yaw 기준)
        local camRight = Vector(math.sin(yawRad), -math.cos(yawRad), 0)
        -- Up 벡터 (간단히 월드 Up 사용)
        local camUp = Vector(0, 0, 1)

        -- 오프셋 적용: X=앞, Y=오른쪽, Z=위
        local offset = self.ironSightOffset
        local worldPos = camPos
            + camForward * offset.X
            + camRight * offset.Y
            + camUp * offset.Z

        self.ironSightActor.Location = worldPos

        -- 플레이어와 같은 회전 (카메라는 플레이어에 붙어있으므로)
        if self.playerObj then
            self.ironSightActor.Rotation = self.playerObj.Rotation
        end
    end
end

--- 총/손 가시성 (ADS 중엔 숨김)
function PlayerADS:UpdateGunVisibility()
    -- ADS 진행도가 높으면 총과 손 숨김
    local shouldHide = self.adsProgress > 0.5

    if self.gunMesh then
        self.gunMesh.bIsVisible = not shouldHide
    end
    if self.skeletalMesh then
        self.skeletalMesh.bIsVisible = not shouldHide
    end
end

--- 현재 감도 배수 반환
function PlayerADS:GetSensitivityMultiplier()
    -- ADS 진행도에 따라 감도 보간
    return 1.0 + (self.sensitivityMult - 1.0) * self.adsProgress
end

--- ADS 중인지 여부
function PlayerADS:IsAiming()
    return self.isAiming
end

--- ADS 진행도 (0~1)
function PlayerADS:GetProgress()
    return self.adsProgress
end

--- 설정값 변경
function PlayerADS:SetFOV(defaultFOV, adsFOV)
    self.defaultFOV = defaultFOV
    self.adsFOV = adsFOV
end

function PlayerADS:SetSpeed(speed)
    self.adsSpeed = speed
end

function PlayerADS:SetSensitivityMult(mult)
    self.sensitivityMult = mult
end

function PlayerADS:SetIronSightOffset(offset)
    self.ironSightOffset = offset
end

function PlayerADS:SetIronSightScale(scale)
    self.ironSightScale = scale
    if self.ironSightActor then
        self.ironSightActor.Scale = scale
    end
end

--- ADS 상태에서의 머즐 플래시 위치 반환
function PlayerADS:GetMuzzlePosition()
    if not self.camera then return nil end

    local camPos = self.camera:GetWorldLocation()
    local camForward = self.camera:GetForward()

    -- 아이언사이트보다 약간 더 앞에서 발사
    local muzzleDistance = self.ironSightOffset.X + 0.3

    return camPos + camForward * muzzleDistance
end

--- 정리
function PlayerADS:Destroy()
    if self.ironSightActor then
        DeleteObject(self.ironSightActor)
        self.ironSightActor = nil
    end
end

return PlayerADS
