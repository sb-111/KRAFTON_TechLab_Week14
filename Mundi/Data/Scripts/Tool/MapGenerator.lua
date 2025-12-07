-- Tool/MapGenerator.lua
-- 맵 타일 생성 도구 (2종류 메시 가중치 랜덤 배치)
-- 사용법:
--   1. 액터에 StaticMeshComponent 2개를 붙임 (메시 A, B)
--   2. 이 스크립트를 붙이고 PIE 실행
--   3. G 키로 타일 생성 (원본들은 자동으로 숨겨짐)
--   4. LuaScriptComponent 제거
--   5. "To Prefab" 버튼으로 프리팹 저장

-- ===== 여기서 설정값 수정 =====
local xScale = 20.0           -- X축 스케일
local yScale = 20.0           -- Y축 스케일
local xCount = 2             -- X축 방향 타일 개수
local yCount = 30          -- Y축 방향 타일 개수
local tileSpacingX = 1.0    -- X축 타일 간격
local tileSpacingY = 1.0    -- Y축 타일 간격

-- 메시 가중치 설정 (A:B 비율)
local weightA = 7            -- 메시 A 가중치
local weightB = 3            -- 메시 B 가중치
-- ==============================

-- 내부 상태
local generatedComponents = {}
local isGenerated = false
local templateA = nil
local templateB = nil

function BeginPlay()
    print("=== MapGenerator Started ===")
    print(string.format("Config: xScale=%.2f, yScale=%.2f, xCount=%d, yCount=%d",
        xScale, yScale, xCount, yCount))
    print(string.format("Tile Spacing: X=%.2f, Y=%.2f", tileSpacingX, tileSpacingY))
    print(string.format("Weight: A=%d, B=%d (%.0f%% : %.0f%%)",
        weightA, weightB,
        weightA / (weightA + weightB) * 100,
        weightB / (weightA + weightB) * 100))
    print("Press G to Generate tiles")

    -- 2개의 StaticMeshComponent 가져오기 (템플릿 A, B)
    local meshComps = GetComponents(Obj, "UStaticMeshComponent")
    if meshComps and #meshComps >= 2 then
        templateA = meshComps[1]
        templateB = meshComps[2]
        print("[MapGenerator] ========== TEMPLATE ORDER ==========")
        print("[MapGenerator] Template A (weight " .. weightA .. "): " .. tostring(templateA.StaticMesh))
        print("[MapGenerator] Template B (weight " .. weightB .. "): " .. tostring(templateB.StaticMesh))
        print("[MapGenerator] =====================================")
    elseif meshComps and #meshComps == 1 then
        print("[MapGenerator] ERROR: Only 1 StaticMeshComponent found! Need 2 for A/B mixing.")
    else
        print("[MapGenerator] ERROR: No StaticMeshComponent found!")
    end
end

function Tick(dt)
    -- G 키: 타일 생성
    if InputManager:IsKeyPressed('G') then
        if not isGenerated then
            GenerateTiles()
        else
            print("[MapGenerator] Already generated. Restart PIE to regenerate.")
        end
    end
end

-- 가중치 기반 랜덤 선택 함수
local function SelectTemplate()
    local totalWeight = weightA + weightB
    local roll = math.random(1, totalWeight)
    if roll <= weightA then
        return templateA
    else
        return templateB
    end
end

function GenerateTiles()
    -- 재진입 방지 (먼저 플래그 설정)
    isGenerated = true

    print(string.format("[MapGenerator] Generating %d x %d tiles...", xCount, yCount))

    -- 템플릿 확인
    if not templateA or not templateB then
        print("[MapGenerator] ERROR: Need 2 StaticMeshComponents as templates!")
        print("[MapGenerator] Actor structure: SceneComponent(Root) -> StaticMeshComponent(A) + StaticMeshComponent(B)")
        return
    end

    -- 랜덤 시드 초기화 (os 없으므로 타일 좌표 기반)
    math.randomseed(xCount * 1000 + yCount + math.random(1, 9999))

    -- 실제 타일 간격 (스케일 적용)
    local actualSpacingX = tileSpacingX * xScale
    local actualSpacingY = tileSpacingY * yScale

    -- 중심 기준으로 생성하기 위한 오프셋 계산
    local halfX = math.floor(xCount / 2)
    local halfY = math.floor(yCount / 2)

    -- 통계용 카운터
    local count = 0
    local countA = 0
    local countB = 0

    -- n x m 타일 전부 생성 (루트 위치 기준으로 배치)
    for ix = -halfX, xCount - 1 - halfX do
        for iy = -halfY, yCount - 1 - halfY do
            -- 가중치 기반으로 템플릿 선택
            local selectedTemplate = SelectTemplate()

            -- 새 컴포넌트 추가
            local newComp = AddComponent(Obj, "UStaticMeshComponent")
            if newComp then
                -- 선택된 템플릿에서 StaticMesh 복사
                newComp.StaticMesh = selectedTemplate.StaticMesh

                -- MaterialSlots 복사 (원본 방식)
                newComp.MaterialSlots = selectedTemplate.MaterialSlots

                -- 스케일 설정
                newComp.RelativeScale = Vector(xScale, yScale, 1.0)

                -- 상대 위치 설정 (루트 기준, (0,0) = 루트 위치)
                local relX = ix * actualSpacingX
                local relY = iy * actualSpacingY
                newComp.RelativeLocation = Vector(relX, relY, 0)

                table.insert(generatedComponents, newComp)
                count = count + 1

                -- 통계 업데이트
                if selectedTemplate == templateA then
                    countA = countA + 1
                else
                    countB = countB + 1
                end
            else
                print("[MapGenerator] ERROR: Failed to add component!")
            end
        end
    end

    print(string.format("[MapGenerator] Loop range: ix=[%d,%d], iy=[%d,%d]", -halfX, xCount-1-halfX, -halfY, yCount-1-halfY))

    -- 원본 템플릿들 숨기기 (스케일 0으로)
    templateA.RelativeScale = Vector(0, 0, 0)
    templateB.RelativeScale = Vector(0, 0, 0)

    print(string.format("[MapGenerator] Generated %d tiles (A: %d, B: %d)", count, countA, countB))
    print(string.format("[MapGenerator] Actual ratio - A: %.1f%%, B: %.1f%%",
        countA / count * 100, countB / count * 100))
    print("[MapGenerator] Original templates hidden (scale=0).")
end

