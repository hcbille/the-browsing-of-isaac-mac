-- Shader probe (test mod, see notes/shaders-research.md phase A). Logs every shader name MC_GET_SHADER_PARAMS fires with
-- (own call counters, so a paused game does not hide it) and drives shaders in phases counted in ShaderProbeTint calls
-- (the game asks once per frame), so a run with screenshots can show each effect and the log says which phase is on:
--   1  calls < 200      no effect (baseline)
--   2  200 .. 399       ShaderProbeTint (this mod's own shader): screen mixed 50% toward red
--   3  400 .. 599       Fiend Folio's shipped SleeperEtAl, ActiveIn = 1.75 (darkened, red kept)
--   4  600 .. 799       StageAPI's shipped StageAPI-TransitionPixelation, PixelAmount = 0.02 (StageAPI itself uses 0.005 to about 0.1)
--   5  800 .. 999       ShaderProbeRD: green if the game fills RenderData.zw (> 100), magenta if not
--   6  1000 and later   StageAPI-TransitionPixelation, PixelAmount = 0.08
local mod = RegisterMod("Shader Probe", 1)
local seen, count, total = {}, {}, 0
local phase = 0
local PHASE_NAMES = { "baseline", "ShaderProbeTint red", "SleeperEtAl ActiveIn=1.75", "StageAPI-TransitionPixelation 0.02",
  "ShaderProbeRD (green = RenderData.zw > 100, magenta = not)", "StageAPI-TransitionPixelation 0.08" }

local function tint_calls() return count["ShaderProbeTint"] or 0 end

-- Runs first (lowest priority number), because the game stops at the first callback that returns a table:
-- StageAPI and Fiend Folio return params for their own shaders, which hides those names from a later logger.
mod:AddPriorityCallback(ModCallbacks.MC_GET_SHADER_PARAMS, -100000, function(_, name)
  total = total + 1
  count[name] = (count[name] or 0) + 1
  if not seen[name] then
    seen[name] = true
    Isaac.DebugString("shader_probe: callback first fired with shader name '" .. tostring(name) .. "' (call " .. total .. ", game frame " .. Game():GetFrameCount() .. ")")
  end
  if total % 600 == 0 then
    local parts = {}
    for n, c in pairs(count) do parts[#parts + 1] = n .. "=" .. c end
    table.sort(parts)
    Isaac.DebugString("shader_probe: call " .. total .. " counts: " .. table.concat(parts, " "))
  end
  if name == "ShaderProbeTint" then
    local n = tint_calls()
    local p = n < 200 and 1 or n < 400 and 2 or n < 600 and 3 or n < 800 and 4 or n < 1000 and 5 or 6
    if p ~= phase then
      phase = p
      Isaac.DebugString("shader_probe: PHASE " .. p .. " (" .. PHASE_NAMES[p] .. ") from ShaderProbeTint call " .. n)
    end
    if n == 1100 then
      local want = { "PauseScreenCompletionMarks", "ShaderProbeTint", "ShaderProbePlain", "ShaderProbeRD", "EmptyShader",
        "SleeperEtAl", "StageAPI-TransitionPixelation", "StageAPI-RenderAboveHUD" }
      local pass, fail, missing = 0, 0, {}
      for _, w in ipairs(want) do
        if count[w] then pass = pass + 1 else fail = fail + 1 missing[#missing + 1] = w end
      end
      Isaac.DebugString("shader_probe: DONE " .. pass .. " names seen, " .. fail .. " missing" .. (fail > 0 and (" (" .. table.concat(missing, ",") .. ")") or "") .. "; phases reached " .. phase)
    end
    return { TintAmount = (p == 2) and 0.5 or 0.0 }
  elseif name == "SleeperEtAl" and phase == 3 then
    return { ActiveIn = 1.75 }
  elseif name == "StageAPI-TransitionPixelation" and (phase == 4 or phase == 6) then
    return { PixelAmount = (phase == 4) and 0.02 or 0.08 }
  elseif name == "ShaderProbeRD" then
    return { RDMix = (phase == 5) and 1.0 or 0.0 }
  end
end)
