-- Tool/MapGenerator.lua
-- 맵 타일 생성 도구
-- 사용법:
--   1. StaticMeshComponent가 있는 액터에 이 스크립트를 붙이고 PIE 실행
--   2. G 키로 타일 생성 (원본은 자동으로 숨겨짐)
--   3. LuaScriptComponent 제거
--   4. "To Prefab" 버튼으로 프리팹 저장
--   (선택) 원본 템플릿 컴포넌트는 숨겨진 상태로 유지되거나 수동 삭제

-- ===== 여기서 설정값 수정 =====
local xScale = 20.0           -- X축 스케일
local yScale = 20.0           -- Y축 스케일
local xCount = 2             -- X축 방향 타일 개수
local yCount = 30          -- Y축 방향 타일 개수
local tileSpacingX = 1.0    -- X축 타일 간격
local tileSpacingY = 1.0    -- Y축 타일 간격
-- ==============================

-- 내부 상태
local generatedComponents = {}
local isGenerated = false

function BeginPlay()
    print("=== MapGenerator Started ===")
    print(string.format("Config: xScale=%.2f, yScale=%.2f, xCount=%d, yCount=%d",
        xScale, yScale, xCount, yCount))
    print(string.format("Tile Spacing: X=%.2f, Y=%.2f", tileSpacingX, tileSpacingY))
    print("Press G to Generate tiles")

    -- 원본 StaticMeshComponent 확인
    local meshComp = GetComponent(Obj, "UStaticMeshComponent")
    if meshComp then
        print("[MapGenerator] Original StaticMeshComponent found")
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

function GenerateTiles()
    print(string.format("[MapGenerator] Generating %d x %d tiles...", xCount, yCount))

    -- 템플릿 컴포넌트 가져오기 (메시/머티리얼 복사용)
    local templateComp = GetComponent(Obj, "UStaticMeshComponent")
    if not templateComp then
        print("[MapGenerator] ERROR: No template StaticMeshComponent!")
        print("[MapGenerator] Actor structure should be: SceneComponent(Root) -> StaticMeshComponent(Template)")
        return
    end

    -- 실제 타일 간격 (스케일 적용)
    local actualSpacingX = tileSpacingX * xScale
    local actualSpacingY = tileSpacingY * yScale

    -- 중심 기준으로 생성하기 위한 오프셋 계산
    local halfX = math.floor(xCount / 2)
    local halfY = math.floor(yCount / 2)

    -- n x m 타일 전부 생성 (루트 위치 기준으로 배치)
    -- 새 컴포넌트는 루트의 자식이므로 RelativeLocation (0,0,0) = 루트 위치
    local count = 0
    for ix = -halfX, xCount - 1 - halfX do
        for iy = -halfY, yCount - 1 - halfY do
            -- 새 컴포넌트 추가
            local newComp = AddComponent(Obj, "UStaticMeshComponent")
            if newComp then
                -- StaticMesh 복사
                newComp.StaticMesh = templateComp.StaticMesh

                -- MaterialSlots 복사
                newComp.MaterialSlots = templateComp.MaterialSlots

                -- 스케일 설정
                newComp.RelativeScale = Vector(xScale, yScale, 1.0)

                -- 상대 위치 설정 (루트 기준, (0,0) = 루트 위치)
                local relX = ix * actualSpacingX
                local relY = iy * actualSpacingY
                newComp.RelativeLocation = Vector(relX, relY, 0)

                table.insert(generatedComponents, newComp)
                count = count + 1
            else
                print("[MapGenerator] ERROR: Failed to add component!")
            end
        end
    end

    print(string.format("[MapGenerator] Loop range: ix=[%d,%d], iy=[%d,%d]", -halfX, xCount-1-halfX, -halfY, yCount-1-halfY))

    -- 원본 템플릿 숨기기 (스케일 0으로)
    templateComp.RelativeScale = Vector(0, 0, 0)

    isGenerated = true
    print(string.format("[MapGenerator] Generated %d tiles", count))
    print("[MapGenerator] Original template hidden (scale=0). You can delete it manually if needed.")
end

