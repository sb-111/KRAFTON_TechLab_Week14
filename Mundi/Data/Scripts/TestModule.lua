-- TestModule.lua - require 테스트용 모듈
local M = {}

M.version = "1.0"
M.message = "require 작동함!"

function M.Add(a, b)
    return a + b
end

function M.Greet(name)
    print("[TestModule] Hello, " .. name .. "!")
end

print("[TestModule] 모듈 로드됨!")

return M
