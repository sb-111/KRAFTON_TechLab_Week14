-- Player/w14_PlayerSlow.lua
-- 속도 감소 상태 관리 모듈

local PlayerSlow = {}

--- 새 PlayerSlow 인스턴스 생성
--- @param Obj userdata 플레이어 GameObject
--- @return table PlayerSlow 인스턴스
function PlayerSlow:new(Obj)
    local Instance = setmetatable({}, self)
    self.__index = self

    Instance.Obj = Obj
    Instance.SpeedMultiplier = 1.0  -- 현재 속도 배율 (1.0 = 정상)
    Instance.bSlowed = false        -- 감속 상태인지

    return Instance
end

--- 속도 감소 + 비네트 효과 + 카메라 넉백(Roll) 통합 함수
--- @param speedMult number 속도 배율
--- @param duration number 지속 시간
--- @param knockBackStrength number (Optional) 넉백 강도 (없으면 0 처리)
--- @param direction number (Optional) 넉백 방향 (-1: 좌, 1: 우)
function PlayerSlow:ApplySlow(speedMult, duration, knockBackStrength, direction)
    -- 1. 중복 실행 방지 및 값 갱신
    if self.bSlowed and speedMult >= self.SpeedMultiplier then
        return
    end

    self.SpeedMultiplier = speedMult
    self.bSlowed = true

    -- 2. 초기 설정 (카메라 셰이크 & 비네트 시작)
    local cameraMgr = GetCameraManager()

    if cameraMgr then
        cameraMgr:StartCameraShake(0.15, 0.2, 0.15, 30)

    end

    -- 넉백 강도 및 방향 기본값 처리 (인자가 nil일 경우 대비)
    local kStrength = knockBackStrength or 0
    local kDir = direction or 0
    
    -- Roll 최대 각도 설정 (강도 * 방향 * 배율)
    -- -5.0은 각도를 좀 더 다이나믹하게 주기 위한 배율입니다.
    local maxRollAngle = kStrength * -5.0 * kDir 

    -- 3. 통합 코루틴 시작
    local selfRef = self
    StartCoroutine(function()
        local elapsedTime = 0
        local deltaTime = 0.016
        duration = duration * 0.6
        
        while elapsedTime < duration do
            coroutine.yield("wait_time", deltaTime)
            elapsedTime = elapsedTime + deltaTime
            
            -- 진행률 (0.0 ~ 1.0)
            local t = math.min(elapsedTime / duration, 1.0)
            
            -- 사인 파형 (0 -> 1 -> 0)
            local sineValue = math.sin(t * math.pi)

            if cameraMgr then
                local intensity = sineValue -- 0 ~ 1 ~ 0

                -- B. 카메라 롤 업데이트 (화면 기울기)
                -- 넉백 강도가 있을 때만 실행
                if kStrength > 0 then
                    local camera = cameraMgr:GetCamera()
                    if camera then
                        local currentRot = camera:GetWorldRotation()
                        local rollOffset = maxRollAngle * sineValue -- 0 ~ Max ~ 0

                        -- X(Pitch), Y(Yaw)는 건드리지 않고 Z(Roll)만 변경
                        camera:SetWorldRotation(Vector(rollOffset, currentRot.Y, currentRot.Z))
                    end
                end
            end
        end
        
        -- 4. 종료 처리 (복구)
        if cameraMgr then
            -- 카메라 Roll 복구 (0도로 초기화)
            if kStrength > 0 then
                local camera = cameraMgr:GetCamera()
                if camera then
                    local finalRot = camera:GetWorldRotation()
                    -- Pitch/Yaw는 유지하고 Roll만 0으로
                    camera:SetWorldRotation(Vector(0, finalRot.Y, finalRot.Z))
                end
            end
        end
        
        selfRef:EndSlow()
    end)
end

--- 속도 감소 종료
function PlayerSlow:EndSlow()
    self.SpeedMultiplier = 1.0
    self.bSlowed = false
end

--- 현재 속도 배율 반환 (Player 이동 시 사용)
--- @return number 속도 배율 (1.0 = 정상, 0.5 = 50%)
function PlayerSlow:GetSpeedMultiplier()
    return self.SpeedMultiplier
end

--- 감속 상태인지 확인
--- @return boolean 감속 중이면 true
function PlayerSlow:IsSlowed()
    return self.bSlowed
end

--- 상태 초기화
function PlayerSlow:Reset()
    self.SpeedMultiplier = 1.0
    self.bSlowed = false
end

return PlayerSlow
