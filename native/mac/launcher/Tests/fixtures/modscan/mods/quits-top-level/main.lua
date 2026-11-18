-- a mod that does not run without REPENTOGON
if not REPENTOGON then
  Isaac.ConsoleOutput("needs it")
  return
end
local mod = RegisterMod("Quits", 1)
mod:AddCallback(ModCallbacks.MC_POST_PLAYER_INIT_EX, function() end)
mod:AddCallback(ModCallbacks.MC_POST_UPDATE, function() Isaac.GetPersistentGameData():IncrementEventCounter(1, 1) end)
ImGui.CreateMenu("x", "y")
local function helper() if not REPENTOGON then return end end
if REPENTOGON == nil or Game():IsGreedMode() then error("no") end
