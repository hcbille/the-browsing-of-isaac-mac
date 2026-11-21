# REPENTOGON names the Layer does not implement, and why

This lists the names that stay out of `REPENTOGON.Implemented` on purpose, by reason. A name is registered only when it
really works, because the gate shows REPENTOGON to a mod only if every name the mod uses is real; a stand-in would break
mods that test for it. Counts are from the wave 10 census (1,727 functions, 380 callbacks in REPENTOGON's parity
target). "Not worth it" means reachable in principle and not planned; "cannot" means there is nothing to port it to.

## 1. Dear ImGui (58 names): not planned

`ImGui.AddButton`, `AddCallback`, `AddCheckbox`, `AddCombobox`, `AddDragFloat`, `AddDragInteger`, `AddElement`,
`AddInputColor`, `AddInputController`, `AddInputFloat`, `AddInputInteger`, `AddInputKeyboard`, `AddInputText`,
`AddInputTextMultiline`, `AddPlotHistogram`, `AddPlotLines`, `AddProgressBar`, `AddRadioButtons`, `AddSliderFloat`,
`AddSliderInteger`, `AddTab`, `AddTabBar`, `AddText`, `CreateMenu`, `CreateWindow`, `ElementExists`,
`GetGameWindowRect`, `GetMousePosition`, `GetVisible`, `GetWindowChildFlags`, `GetWindowFlags`, `GetWindowPinned`, `Hide`,
`ImGuiToWorld`, `IsVisible`, `LinkWindowToElement`, `PushNotification`, `RemoveCallback`, `RemoveColor`,
`RemoveElement`, `RemoveMenu`, `RemoveWindow`, `Reset`, `SetColor`, `SetHelpmarker`, `SetSize`, `SetTextColor`,
`SetTooltip`, `SetVisible`, `SetWindowChildFlags`, `SetWindowFlags`, `SetWindowPinned`, `SetWindowPosition`,
`SetWindowSize`, `Show`, `UpdateData`, `UpdateText`, `WorldToImGui`.

Why: REPENTOGON draws these windows with the Dear ImGui library on top of the game's own renderer. Doing the same here
means bundling Dear ImGui with an OpenGL ES backend in the Host, routing SDL keyboard, mouse and controller events into
it, and a protocol that lets Lua describe a window tree to the Host and receive widget callbacks back. The estimate is
28 to 36 worker-hours of Host work for a first phase of about 38 names. No mod in the scanned set (95 mods) uses it, and
the project owner decided to leave it out. Callbacks that REPENTOGON fires from its ImGui console (the console's
autocomplete) stay unregistered for the same reason: the game's own console has nothing to hook.

## 2. Shaders and clip panes: cannot, without a shader pipeline

- `Renderer.LoadShader`, `Renderer.GetShaderByType`
- `Sprite` and `LayerState`: `SetCustomShader`, `ClearCustomShader`, `HasCustomShader`, `SetCustomChampionShader`,
  `ClearCustomChampionShader`, `HasCustomChampionShader` (12 names)
- `Image.RenderWithShader`
- `Renderer.GetClipPaneNormal`, `Renderer.GetClipPaneThreshold` (and the clip-pane getters of the same family)
- `Renderer.StartTransformation` (needs REPENTOGON's `Transformer` class, which is part of the same render path)
- `SurfaceRenderController.SetBlendMode` (the Host's pixel path is a fixed source-over blit)
- Callbacks `MC_PRE_OPENGL_RENDER` (1136) and the render-stage callbacks that exist only to wrap raw GL state

Why: these names load, bind and run GPU shader programs on sprites, images and surfaces. The Layer runs on the Host's
own renderer, which has no hook for attaching a custom program to a draw call, and the game's own shader objects are not
the same programs here. A Lua-only version would store the shader and never draw with it, which is a stand-in. This
needs a renderer feature (a shader compile and bind path in the Host and a way to select it per sprite), not a name
batch. The clip-pane getters read the renderer's internal settings; candidate globals were located but never tested,
and nothing uses them.

What does work: a mod's own `shaders.xml` (the vanilla route) compiles, and `MC_GET_SHADER_PARAMS` fires with the mod's
shader name and drives it (checked with a probe mod, StageAPI and Fiend Folio's `SleeperEtAl`). The per-sprite and
per-image paths above are not planned: the only user is Fiend Folio's five `coloroffset_*` tints, and the cost is high.

## 3. Windows-only calls: cannot

- `Isaac.GetDwmWindowAttribute`, `Isaac.SetDwmWindowAttribute`: query and set attributes of the Windows desktop window
  manager (title-bar colour, rounded corners, dark mode).
- `Isaac.OpenConsole`: opens the Windows console window.
- `Isaac.GetLoadedModules`: lists the DLLs loaded into the process.
- `Debug.GetSignature`: REPENTOGON builds a code signature from a Windows address with `VirtualQuery`.

Why: they ask Windows for things a Mac does not have. There is no meaningful answer to return, and a made-up one would
be fake.

## 4. Reachable, not planned yet (listed so they are not mistaken for "cannot")

- `Debug.ListLoadedFiles`, `Debug.ForceUnload`: mods get no `package` or `debug` table in this build, so there is no
  loaded-module registry to read or clear.
- `HistoryHUD.GetTrinkets`, `GetTrinketOffsets`: vanilla never puts a trinket in the history list; a getter would only
  return an empty list.
- `Backdrop.GetFloorImage`, `GetWallImage`: the engine images exist (780x468) but the Layer's `Image` cannot wrap them
  yet (calling their vtable slots crashed).

## 5. Names that need an engine hook

Not listed here: they are the final hook wave (see `notes/` while it runs, and section 9 of `layer-gaps.md` once done).
