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

    -- 사이트 위치 (카메라 기준 오프셋: X=앞, Y=좌우, Z=상하)
    instance.ironSightOffset = Vector(0.9, 0.0005, -0.1525)  -- 카메라 앞, 약간 아래로
    instance.ironSightScale = Vector(0.075, 0.075, 0.075)  -- 스케일 조정 필요

    -- DoF 설정값
    -- 스코프 거리: ironSightOffset.X = 0.9 (카메라에서 0.9 단위 앞)
    -- CoC 공식: nearBlur = (FocalDistance - depth) / NearTransitionRange
    --          farBlur = (depth - FocalDistance) / FarTransitionRange
    --
    -- Red Dot Scope DoF: 스코프 바로 뒤에 초점, 스코프만 블러
    -- FocalDistance = 1.5m (스코프 0.9m 바로 뒤)
    -- NearRange = 0.9m → 0~0.9m 범위가 블러됨
    instance.dofEnabled = true  -- DoF 효과 사용 여부
    instance.dofProgress = 0.0  -- DoF 블러 강도 (0~1), adsProgress와 별도로 관리
    instance.dofSpeedIn = 15.0  -- DoF 진입 속도 (빠르게)
    instance.dofSpeedOut = 3.0  -- DoF 해제 속도 (느리게)
    instance.dofSettings = {
        -- 힙파이어 상태 (DoF 비활성화)
        baseFocalDistance = 100.0,
        baseNearRange = 50.0,
        baseFarRange = 1000.0,
        baseMaxCoC = 0.0,  -- 0 = 블러 없음
        -- ADS 완료 상태 (스코프 바로 뒤에 초점)
        adsFocalDistance = 1.5,     -- 스코프(0.7m) 바로 뒤에 초점
        adsNearRange = 0.9,         -- 0~0.9m만 블러 (스코프 포함)
        adsFarRange = 10000.0,      -- 배경은 블러 없음
        adsMaxCoC = 25.0,           -- 최대 블러 반경 (15 → 25로 증가)
    }

    return instance
end

--- 사이트 액터 초기화
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

    -- 사이트 위치/가시성 업데이트
    self:UpdateIronSight()

    -- 총 가시성 업데이트
    self:UpdateGunVisibility()

    -- DoF 업데이트
    self:UpdateDoF(deltaTime)
end

--- FOV 보간
function PlayerADS:UpdateFOV()
    if not self.camera then return end

    local currentFOV = self.defaultFOV + (self.adsFOV - self.defaultFOV) * self.adsProgress
    self.camera.FieldOfView = currentFOV
end

--- DoF (Depth of Field) 업데이트
function PlayerADS:UpdateDoF(deltaTime)
    if not self.dofEnabled then return end

    local s = self.dofSettings

    -- DoF 목표값: ADS 진행도가 0.9 이상이면 DoF 활성화
    local enableThreshold = 0.9
    local targetDofProgress = (self.adsProgress >= enableThreshold) and 1.0 or 0.0

    -- dofProgress 보간 (현재 값에서 부드럽게 전환)
    -- 진입/해제 속도를 다르게 적용
    local dofSpeed
    if targetDofProgress > self.dofProgress then
        dofSpeed = self.dofSpeedIn   -- DoF 켜질 때 (빠르게)
    else
        dofSpeed = self.dofSpeedOut  -- DoF 꺼질 때 (느리게)
    end

    self.dofProgress = self.dofProgress + (targetDofProgress - self.dofProgress) * dofSpeed * deltaTime

    -- 아주 작은 값 정리
    if self.dofProgress < 0.01 then self.dofProgress = 0.0 end
    if self.dofProgress > 0.99 then self.dofProgress = 1.0 end

    -- DoF 적용
    if self.dofProgress > 0.01 then
        SetDOFFocalDistance(s.adsFocalDistance)
        SetDOFNearTransitionRange(s.adsNearRange)
        SetDOFFarTransitionRange(s.adsFarRange)
        SetDOFMaxCoCRadius(s.adsMaxCoC * self.dofProgress)
        EnableDepthOfField()
    else
        DisableDepthOfField()
    end
end

--- 사이트 위치/가시성
function PlayerADS:UpdateIronSight()
    if not self.ironSightActor or not self.camera then return end

    -- ADS 중이면 보이기
    local shouldShow = self.adsProgress > 0.1
    self.ironSightActor.bIsActive = shouldShow

    if shouldShow then
        -- 카메라 위치와 방향 벡터
        local camPos = self.camera:GetWorldLocation()
        local camForward = self.camera:GetForward()
        local camRight = self.camera:GetRight()
        local camUp = self.camera:GetUp()

        -- 오프셋 적용: 카메라 로컬 좌표계 기준
        -- X=앞(forward), Y=오른쪽(right), Z=위(up)
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
    -- 우클릭 누르면 바로 숨김, 떼면 0.2 이하일 때 보이기
    local shouldHide = self.isAiming or self.adsProgress > 0.2

    if self.gunMesh then
        self.gunMesh.bIsVisible = not shouldHide
    end
    if self.skeletalMesh then
        self.skeletalMesh.bIsVisible = not shouldHide
    end
end

--- 총을 쏠 수 있는 상태인지 (팔이 완전히 나왔을 때만)
function PlayerADS:CanFire()
    -- ADS 완전 상태이거나, 힙파이어 완전 상태에서만 발사 가능
    return self.adsProgress < 0.01 or self.adsProgress > 0.99
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

function PlayerADS:SetDoFEnabled(enabled)
    self.dofEnabled = enabled
    if not enabled then
        DisableDepthOfField()
    end
end

function PlayerADS:SetDoFSettings(settings)
    -- settings: { adsFocalDistance, adsNearRange, adsFarRange, adsMaxCoC }
    if settings.adsFocalDistance then self.dofSettings.adsFocalDistance = settings.adsFocalDistance end
    if settings.adsNearRange then self.dofSettings.adsNearRange = settings.adsNearRange end
    if settings.adsFarRange then self.dofSettings.adsFarRange = settings.adsFarRange end
    if settings.adsMaxCoC then self.dofSettings.adsMaxCoC = settings.adsMaxCoC end
end

--- ADS 상태에서의 머즐 플래시 위치 반환
function PlayerADS:GetMuzzlePosition()
    if not self.camera then return nil end

    local camPos = self.camera:GetWorldLocation()
    local camForward = self.camera:GetForward()

    -- 사이트보다 약간 더 앞에서 발사
    local muzzleDistance = self.ironSightOffset.X + 0.3

    return camPos + camForward * muzzleDistance
end

--- 정리
function PlayerADS:Destroy()
    if self.ironSightActor then
        DeleteObject(self.ironSightActor)
        self.ironSightActor = nil
    end
    -- DoF 비활성화
    DisableDepthOfField()
end

return PlayerADS
