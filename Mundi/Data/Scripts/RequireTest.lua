-- RequireTest.lua - require 기능 테스트
-- 이 스크립트를 아무 Actor에 붙여서 실행

function BeginPlay()
    print("=== require 테스트 시작 ===")

    -- 모듈 로드 테스트
    local ok, TestModule = pcall(require, "TestModule")

    if ok then
        print("require 성공!")
        print("version: " .. TestModule.version)
        print("message: " .. TestModule.message)
        print("1 + 2 = " .. TestModule.Add(1, 2))
        TestModule.Greet("Mundi")
    else
        print("require 실패: " .. tostring(TestModule))
    end

    -- GlobalConfig 접근 테스트
    print("GlobalConfig 접근: " .. tostring(GlobalConfig ~= nil))

    print("=== require 테스트 완료 ===")
end

function Tick(dt)
end
