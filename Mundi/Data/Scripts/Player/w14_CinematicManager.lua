-- w14_CinematicManager.lua
-- 시네마틱 플레이어 중 한 명을 생존자로 지정하는 매니저

function BeginPlay()
    StartCoroutine(function()
        coroutine.yield("wait_time", 0.1)

        local players = {}

        local player1 = FindObjectByName("CinematicPlayer1")
        local player2 = FindObjectByName("CinematicPlayer2")
        local player3 = FindObjectByName("CinematicPlayer3")
        local player4 = FindObjectByName("CinematicPlayer4")

        if player1 then table.insert(players, player1) end
        if player2 then table.insert(players, player2) end
        if player3 then table.insert(players, player3) end
        if player4 then table.insert(players, player4) end

        if #players == 0 then return end

        local survivorIndex = math.random(1, #players)

        for i, player in ipairs(players) do
            local script = player:GetScript()
            if script and script.SetSurvivorStatus then
                script.SetSurvivorStatus(i == survivorIndex)
            end
        end
    end)
end

function Tick(dt)
end
