# Mac Build progress

## Status overview (orchestrator, 2026-11-18; work stopped at the user's request)

The milestones are those in the orchestrator brief. "Verified" means an
independent Verifier subagent re-ran the definition-of-done checks and
reported PASS. The per-milestone sections further down were written by the
implementers; any "(pending Verifier)" in their headings is out of date.

| Milestone | State | mac-native commit | Public commit |
|---|---|---|---|
| M1 Headless main | **Verified** (3,007 frames, guard intact, no new errors vs Browser Build log, absolute-path fix works from another cwd) | af702c0 | 998bd5a |
| M2 Picture | **Verified** ("ANGLE Metal Renderer: Apple M2"; title + Basement I start room match `shot_b.png` in layout, mean abs diff about 6/255; 59.9 to 60.0 fps steady, one 160 ms hitch when the room loads) | a30c9d6 | ea429f3 |
| M3 Input | **Verified** (keyboard via SDL events and SDL virtual gamepad both start, walk, fire, pause; rumble follows RumbleEnabled; hot-plug; key table matches the Browser Build) | b4ededc | 5f07da8 |
| M4 Sound | **Verified** (music and effects non-silent in WAV dump; 0 underruns/overruns over 148 s; 3 late callbacks at one room-load stall; no crackle found; MusicVolume/SFXVolume respected; no panning, same as the Browser Build) | 75efe7b | 7aaafe3 |
| M5 Window | **Verified** (clean rebuild; integer x1/x2 nearest with pure-black bars at 1280x800, 1100x700, 2000x1200; fill aspect-preserving; fullscreen by F, Cmd+Ctrl+F and the Options menu in both modes, options.ini matches; mouse maps through the letterbox, presses in bars ignored; precedence flag > JSON > env; headless M1 3,000 frames; 60.0 fps; evidence `.scratch/verify5/`) | M5 commit | pending |
| M6 Saves | **Verified** (progress survives restart; import cases cloud-present, absent and Mac-save-present pass; fixture Steam folders untouched; evidence `.scratch/verify67/`) | d12bb6c | 3fad2d6 |
| M7 Mods | **Verified** (95 mods synced, checker mod off; MODS menu lists 95, off greyed; Launcher and in-game toggles agree; update keeps `disable.it`; unsubscribe keeps the mod, marked, Remove moves to Trash after confirm). Side notes: Launcher sorts by folder name but the game menu sorts by display name; `fired's_angels_and_statues` main.lua logs a Lua error (M8) | d12bb6c | 3fad2d6 |
| M8 Layer | Round 2 done, **not complete**. Verified 2026-11-18 (Verifier: Epiphany Basement I to Caves I, 95 mods with no new Lua error, headless 3000 frames). 5 of 11 REPENTOGON-using mods Shown (Unique Items API hidden again: `MC_MENU_INPUT_ACTION` and `MC_PRE_GLOWING_HOURGLASS_LOAD` not counted until seen firing); Epiphany's Tarnished page works (`Sprite:Continue`, user-checked). Shown: Epiphany, Door Overhaul, Enhanced Boss Bars, miniMAPI, Music Mod Callback (5 of 11; round 1 had 3). Real floor changes by the game's own trapdoor (`lab_trapdoor.lua`): 95 mods on, 3 changes to Caves II, and Epiphany alone Basement I to Dark Room (10 changes, 11,500 frames), no Lua error except `! fired's_angels_and_statues` (the mod's own bug, same under Vanilla Launch). Three engine hooks added. Still hidden: Custom Stage API, External Item Descriptions, and 3 by the rule. Epiphany still misses 112 names; its characters not yet seen on the wheel; `MC_MENU_INPUT_ACTION` never seen firing | uncommitted | none |
| M9 Steam | Design only: `docs/mac/steam-bridge.md`; stub dylib at `.scratch/steam-research/` | none | none |
| M10 Launcher | First piece built (SwiftUI, `native/mac/launcher/`, contracts in `docs/mac/launcher.md`), tested against a fake game only. Not verified | uncommitted | none |
| M11 App + build.sh | Not started (Launcher `build.sh --app` makes an unbundled wrapper only) | none | none |
| M12 Publish | Running per milestone; README not yet updated with status/steps | none | none |

### Uncommitted work on mac-native at the stop

- **Launcher (M10, M6/M7/M8 logic):** `native/mac/launcher/**`, `docs/mac/launcher.md`.
- **Integration (game side):**
  - New files: `native/mac/host/mac_launch.{c,h}`, `mac_gate.c`, `scripts/mac/stage_layer.py`.
  - Modified: `mac_fs.c`, `mac_gl.c`, `mac_main.c`.
  - `ISAAC_NATIVE`-guarded edits in `scripts/recomp/host/src/`: `host_boot.c`, `host_lua.c`, `host_rgon.c`, `host_shims_fs.c`, `host_shims_gaps.c`.
  - The last build compiled clean; headless M1 re-checked after the last edits (3,016 frames, exit 0).
- **Docs:** `docs/mac/layer-gaps.md`, `docs/mac/steam-bridge.md`.
- **Not ours:** `package-lock.json` is an unrelated npm change (added `"license"` line). Leave it out of commits.

### Decisions taken during orchestration

- **Mac support folder name:** `~/Library/Application Support/Repentance+ Mac/`.
- **Save import and Workshop sync live in the Launcher:** they are Swift code run before every game start. The Launcher is the bundle's main program and always runs first, even with "Skip the Launcher".
- **The mod scan also lives in the Launcher:** it is a Swift port of `createModScan`, because Node can't ship to end users and a Host-side C scan couldn't show results in the Launcher.
- **Machine-specific source folders stay out of the repo:** the Workshop fallback and save search come from env vars or `<support>/sync-config.json`. `build.sh --game <dir>` is to write that config at M11.
- **Workshop count is 95, not 96:** Steam's subscription list also says 95.
- **No Repentance+ cloud save exists on this Mac:** userdata has no 250900 folder. The import imports only `persistentgamedata1..3.dat`.
- **The real `libsteam_api.dylib` comes from the local Steam client:** a universal binary (arm64 slice) is inside the client bundle. It is copied in at build time and never committed.
- **`scripts/mac/apply.sh` exists only in the public repo.** It should also be added to mac-native's `scripts/mac/`.

### Next steps, in order

1. Finish the Integration items listed in its section below. Then run a Verifier for M6 and M7 (the game side).
2. **M5:** scaling modes and fullscreen.
3. **M8:**
   - Fix Epiphany's menu error (missing "Overlay" animation).
   - Add a mod-less save hook for the Layer.
   - Re-measure gaps with a real `implemented.txt` (lab_names run), then grow the Layer for each gap, Epiphany first.
4. **M9:** implement `mac_steam.c` per `docs/mac/steam-bridge.md`. Start with the trace pass for the ISteamUserStats vtable offsets.
5. **M10:** the Launcher against the real game. Then **M11:** bundle and `build.sh`. Then **M12:** README.

## M5 Window: done (pending Verifier)

Status: the window is resizable and the picture is scaled into it by `native/mac/host/mac_window.c`; fullscreen
follows the game's Fullscreen option; the mouse maps through the same rectangle.

How it works:
1. The game's "default framebuffer" is an offscreen framebuffer (RGB8 + depth/stencil) of the game's own size,
   960x540 (read from the Guest's window size at `0x00c799a8/ac`; the fake display in `host_shims_win.c` is 960x540
   on the native build, and `GetClientRect` returns the client size, not the frame size, so the game's own
   fullscreen toggle keeps rendering at 960x540). `glBindFramebuffer(0)` binds it (`host_gl_webgl.c`,
   `ISAAC_NATIVE`-guarded) and `GL_FRAMEBUFFER_BINDING` answers 0 for it.
2. At every present `isaac_mac_window_compose()` clears the drawable black and `glBlitFramebuffer`s the picture
   centred: **integer** (default): the largest whole multiple of 960x540 that fits, nearest filter, bars on both
   axes; if the window is smaller than 1x the picture is fitted (linear) so it is never cut. **fill**: the largest
   aspect-preserving size, linear filter, bars on the short axis. Screenshots (`ISAAC_MAC_SHOT`) now read the
   composed drawable, bars included.
3. Setting: `--scale integer|fill`, launch-settings key `"scale_mode"`, env `REPENTANCE_MAC_SCALE` /
   `ISAAC_MAC_SCALE` (precedence in that order). Initial size: `--window WxH` / `ISAAC_MAC_WINDOW=WxH` (points;
   test knob). `ISAAC_MAC_WINDOW_BORDERLESS=1` is a test hook (macOS keeps a titled window inside the screen, so
   a 2x window on this 1710x1107-point desktop needs a borderless one). No Retina rendering: the drawable is in points.
4. Fullscreen: SDL3 desktop (borderless, Spaces) fullscreen. `Options.Fullscreen` is the byte at Manager+0x2a360
   (Manager = `[0x00c7169c]`; Options at Manager+0x2a33c, Fullscreen at Options+0x24, set by `FUN_00925c60`, which
   runs the game's own window change `FUN_00a196b0`). `mac_window.c` polls it once per frame and toggles the SDL
   window when it changes (options menu: verified by scripted menu navigation; game's F key). **F is the game's own
   fullscreen key** (found by feeding F to the game: it toggles the option), so plain F needs no Host code, it is
   forwarded as before. **Cmd+Ctrl+F** (the macOS key, not forwarded to the game) presses F for the game, so the
   option, the menu, options.ini and the window always agree; if the game has not changed the option after 12
   frames it toggles the window directly (the byte is not written then, the game re-syncs it). Neither works while
   the debug console is open (F is text there). Leaving fullscreen by the system (green button) is followed by
   the poll/event too.
5. Mouse: `isaac_mac_window_to_game()` (moved from `mac_input.c` to `mac_window.c`) maps window points through the
   same rectangle; a move in the bars clamps to the nearest game pixel, a press in the bars is ignored (a release is
   always passed). `ISAAC_MAC_MOUSE_LOG=1` logs every mapped event; button events always log.
6. Script (`ISAAC_MAC_SCRIPT`): `F:fullscreen[:on|:off]`, `F:cmdctrl-f` (SDL event with Cmd+Ctrl), `F:window:WxH`,
   `F:mousewin:X:Y` (a point in the window, bars included); with `ISAAC_MAC_SCRIPT_VIA_SDL=1` the `mouse:X:Y` token
   goes through the scaling rectangle too.

Decisions: aspect-preserving fill (not stretch); whole-number is relative to the game's 960x540; scale mode is a
launch setting, not an in-game option (the Launcher can add a setting later: key `scale_mode`); the F shortcut is
the game's own rather than a Host shortcut (it exists on PC too).

Notes: after a change to `mac_launch.h` (struct layout) touch the files that include it, `build_native.py` only
compares file times. The 1920x1080 and larger window screenshots use the borderless test hook.

Evidence (`.scratch/m5/`, commands in `.scratch/m5/run.sh`; shots in `.scratch/m5/shots/`):
- integer 1280x800: `int-1280x800.png` (1x, bars 160/130); 1100x700: `int-1100x700.png` (1x, bars 70/80); 2000x1200:
  `int-2000x1200.png` (2x, bars 40/60); 1920x1080: `int-1920x1080.png` (2x, no bars).
- fill 1280x800: `fill-1280x800.png` (x1.33, bars 40 top/bottom); 1100x700: `fill-1100x700.png`; 2000x1200:
  `fill-2000x1200.png` (x2.08).
- fullscreen (this desktop's drawable is 1710x1074 points): integer by Cmd+Ctrl+F from a 1280x800 window
  `fullscreen-integer-from1280x800.png`, back by the game's F `windowed-after-F-1280x800.png`, again by Cmd+Ctrl+F
  `fullscreen-integer-2nd.png`; fill by the game's F from 1100x700 `fullscreen-fill-from1100x700.png`, back by
  Cmd+Ctrl+F `windowed-after-cmdctrlf-1100x700.png`; by the Options menu `menu-option-fullscreen-on.png` and
  `menu-option-fullscreen-off.png` (log `.scratch/m5/menuFS.log`: "fullscreen on by the game option").
- mouse (`mouse-integer.log`, `mouse-fill.log`): game 959,539 -> window 1119.5,669.5 -> back to game 959,539; a
  press at window 5,5 in the bars is "ignored".
- headless M1: `ISAAC_MAC_RUN_FRAMES=3000 ... --headless` exits 0, "reached 3007 frames; guard intact"
  (`.scratch/m5/headless.log`). Windowed 60.0 fps in a room at 1280x800 and 2000x1200.

## Full-screen adapt: done (pending Verifier)

Status: the default scale mode is now **adapt**. The game is told the real window size (windowed, resized, full
screen with or without NotchFill) and lays itself out for it, as the PC build does: it picks its own whole-number
pixel scale, centres its viewport, anchors the HUD to the screen edges and shows more or less room around the
play field. `integer` and `fill` (M5) are unchanged (`--scale integer|fill`, `"scale_mode"`, `REPENTANCE_MAC_SCALE`).

How the game learns its size (all from the decompile): GLFW's window procedure (`0x00a5b7b0`, class `GLFW30`)
handles WM_SIZE and, when the size differs from what it holds, calls the framebuffer-size callback
(`0x00a19bf0`), which stores the size at `0x00c799a0/a4`, calls the game's resize callbacks and ends in
`0x009a9ef0`. That calls `FUN_00954320` (menus, runs) or `FUN_00954150` (cutscenes, state 3), which compute the
layout and write `0x00c799a8/ac` (the viewport size). The Host already ran Guest window procedures
(`DispatchMessageW`), so the change is: `isaac_native_window_resized(w, h)` in `host_shims_win.c` stores the size
(`GetClientRect` then answers it exactly) and queues a WM_SIZE (`lParam = w | h << 16`) for the game's window;
`mac_window.c` calls it from the per-frame hook when the drawable changed, and once at frame 0 (the game starts
from its option, 960x540). The fake display (`GetSystemMetrics`, `GetMonitorInfoW`, `EnumDisplaySettings`) answers
the real display (CoreGraphics, dlsym'd). No Guest memory is written.

**How the game picks its scale** (`FUN_00954320`, `Viewport: WxH` / `Framebuffer Width ...` in the log):
`dp = floor(960 / W + 0.5)`; `s = min(W/400, H/240)`; `scale = min(max(floor(dp*s)/dp, 1/dp), Options.MaxScale)`;
logical size = `floor(W/scale) x floor(H/scale)`; viewport = `dp*scale*logical` (the largest whole multiple that fits,
the rest is padding the game centres); a second cap by Options.MaxRenderScale sets the internal "Render Width"
(960 at 1920x1080: the scene is rendered at render scale 2 and shown at 4). So the game's own scale **is** whole-number.
**Game fact found: `dp` is 0 for any window wider than 1920**, so the game then lays out 0x0 and crashes (malloc of a
garbage size) -- first try at 2200x1400 pixels. The Mac drawable is bigger than that (3420x2148, Retina), so the Host
gives the game the drawable divided by the smallest whole **k** that makes it at most 1920 wide (`adapt_k`), the FBO is
that size, and compose blits it back up by k, nearest. Every art pixel is then an N x N block with N = game scale x k;
only the odd leftover device pixels (fewer than k per side) are bars. Nothing was patched in the game.

Note on the display: on this Mac the full-screen drawable is **3420x2148 pixels** (1710x1074 points x2, or 3420x2224
with NotchFill), not 2560x1664. macOS renders a "looks like 1710x1107" mode at 2x and the GPU scales that to the
2560x1664 panel (x0.75). So at the panel an 8-pixel block is about 6 physical pixels; a whole-number guarantee
cannot reach past that macOS scaling step (the panel's native mode would be needed).

Measured (`.scratch/m13-adapt/blocks.py` counts the most common run length of identical pixels; evidence below):
- windowed 1280x800 points (2560x1600 px): game window 1280x800 (k 2), game scale 3 (viewport 1278x798), **N = 6**,
  padding in the game 1 px (2 device px) each side; runs 6/12/18/24 horizontally and vertically.
- default window 960x540 points (1920x1080 px): k 1, game scale 4, viewport 1920x1080, N = 4, no padding.
- 1100x700 points: k 2, game 1100x700, scale 2, viewport 1100x700, N = 4.
- full screen without NotchFill (3420x2148): game 1710x1074, scale 4 (viewport 1708x1072), **N = 8**, padding 2 device px
  left/right and 2 top/bottom.
- full screen with NotchFill (3420x2224): game 1710x1112, scale 4 (viewport 1708x1112), **N = 8**, padding 2 device px
  left/right, none top/bottom; the picture covers the notch strip.
- fps: 59.9-60.0 steady in a room at 3420x2148 and 3420x2224 and at 2560x1600 (the 54 fps lines are asset loading).
- mouse (`mouse.log`): window point -> game pixel is 1:1 with the game's window (640,400 -> 640,400 at 1280x800;
  10,5 -> 10,5; 1279,799 -> 1279,799), and the click goes to the same pixel.
- full screen on and off by the game's F key (script `fullscreen`) re-lays the game out each way (Viewport 1920x1080 ->
  1708x1072 -> 1920x1080 in `fs1.log`); resizes 960x540 -> 1100x700 -> 1280x800 follow (`t6.log`).
- headless M1 regression: `ISAAC_MAC_RUN_FRAMES=3000 ... --headless` exits 0, "reached 3011 frames; guard intact"
  (`headless.log`). integer and fill still draw as in M5 (`integer-1280x800.png` bars 160/130, `fill-1280x800.png` x1.33).

Files: `native/mac/host/mac_window.c` (adapt_k, layout, FBO follow, tell the game), `mac_gl.c` (high-pixel-density
window in adapt), `mac_launch.[ch]` (`scale_adapt`, default), `mac_main.c` (early display query),
`scripts/recomp/host/src/host_shims_win.c` (dynamic display size, `isaac_native_window_resized`, GetClientRect),
Launcher `Model.swift` / `Views.swift` / `Tests/main.swift`. Headless and the Browser Build are unchanged.

Cutscenes (intro, title art; `FUN_00954150`): the game keeps a fixed 960x540 viewport (`0x00c799a8/ac`) at the origin its
glViewport uses (`0x00c79984` when bit 0x40 of `0x00c798e4` is set, else `0x00c7b640`), which used to sit in the corner of
the window. In adapt, compose now blits only that viewport region, scaled by the largest whole number that fits the
drawable (nearest) and centred, black around it (`layout()` in `mac_window.c`; rooms use the same rule, their viewport is the
game window less a few padding pixels, so they still fill the window). The mouse maps through the same rectangle (the shown
part's corner is added back, in the game's top-left coordinates: window centre at 1280x800 points -> game 480,530, the
middle of the 960x540 region). At 1280x800 points the 960x540 picture is shown at x2 (1920x1080, bars 320/260 device px);
in full screen (3420x2148) at x3 (2880x1620, bars 270/264), art runs multiples of 6 (`intro-*.png`).

Not done / open: a menu item clicked by scripted mouse was not shown (the mapping is logged).

Evidence (`.scratch/m13-adapt/`, commands in `run.sh`; shots in `shots/`): `t6c.png` (1280x800 room, HUD at the edges),
`fs1-fullscreen.png`, `fs2-notchfill-fullscreen.png`, `fs1-back-windowed.png`, `integer-1280x800.png`,
`fill-1280x800.png`, `menu-1280x800.png`, `intro-nicalis-1280x800.png`, `intro-art-1280x800.png`, `intro-a-fullscreen.png`, `intro-b-fullscreen.png`, `room2-windowed-2560x1600.png`, `room2-fullscreen-3420.png`; logs `t6.log`, `fs1.log`, `fs2.log`, `mouse.log`, `headless.log`.

## M3 Input: done (pending Verifier)

Status: keyboard, mouse and gamepads work live (SDL events) and scripted. A scripted
run starts a game, walks, fires and pauses by keyboard, and the same through SDL's
virtual gamepad into the XInput shim. Rumble reaches the pad when the game's RUMBLE
option is on and does not when it is off.

Files: `native/mac/host/mac_input.c` (new), `mac_script.c` (extended), `mac_gl.c`
(forwards SDL events). Host, guarded by `ISAAC_NATIVE`: `host_shims_xinput.c` (native
arm of the pad state/rumble hooks, Browser Build unchanged), `host_shims_win.c`
(`isaac_input_mouse_wheel`, WM_MOUSEWHEEL 0x20A; poll event type 4).

Keyboard: SDL scancode to the Windows VK and set-1 scancode (+ extended bit) the Browser
Build's KEYS table uses (letters, digits, punctuation, F1-F12, arrows, Enter, Esc, Tab,
Backspace, Space, modifiers, Insert/Delete/Home/End/PgUp/PgDn, keypad). Key repeat is
dropped (as in the Browser Build); Cmd+key is not forwarded (M5's shortcuts); focus loss
releases every held key and button (as the Browser Build does on blur). Modifiers send the
generic VK with the side in the scancode/extended bit.
Mouse: motion is coalesced to one move per frame, left and right buttons only (the Browser
Build treats middle as left; here it is ignored), wheel as 120 per notch. Window to game
pixels goes through `isaac_mac_window_to_game()` in `mac_input.c` (plain stretch of the
window to 960x540 today; M5 replaces the body).
Pads (SDL3 gamepad API, up to 4): slot = connection order and stays put when another pad
leaves; a free slot answers XInputGetState with ERROR_DEVICE_NOT_CONNECTED. Mapping: A/B/X/Y =
south/east/west/north, bumpers, Back, Start, Guide (0x0400, which the game reads), stick clicks,
dpad; triggers 0..255; sticks int16 with up positive (SDL's Y is inverted); a packet number that
increments on change. XInputSetState: left motor = SDL low-frequency, right = high, 1000 ms per
call (the game sends 0/0 to stop). Confirmed: with `RumbleEnabled=1` a bomb explosion logs
`rumble lo=65535 hi=65535` then `lo=0 hi=0`; with `RumbleEnabled=0` the game makes no call.
SDL is polled once per frame (hot-plug needs no event handling). Pads are off in `--headless`
unless a script uses one (or `ISAAC_MAC_PADS=1`); `ISAAC_MAC_PADS=0` turns them off everywhere.

Script grammar additions (`mac_script.c` header has the full list):
`F:mouse:X:Y`, `F:mouse:left|right[:hold=N]`, `F:mouse:wheel=D`, `F:pad[K]:BTN[:hold=N]`,
`F:pad[K]:AXIS=V[:hold=N]` (axes lx ly rx ry lt rt; SDL sense, ly negative = up),
`F:pad[K]:connect|disconnect`, `until=TEXT:pad-BTN`. `ISAAC_MAC_SCRIPT_VIA_SDL=1` pushes the
keyboard and mouse tokens as SDL events so the live path (scancode table, coordinates) is the
one under test. Pad tokens attach SDL's virtual gamepad (a real SDL joystick with the standard
mapping), so SDL gamepad -> XInput shim -> game is exercised. `ISAAC_MAC_PAD_LOG=0` mutes the
`[mac][pad] slot N state` lines.

Not done: pause on focus loss (upstream has none). A physical controller and live OS keyboard were
not driven in this session (no hardware injection); the live handlers were driven by SDL events.

Evidence (`.scratch/m3-shots/`, logs `.scratch/m3-*.log`): see the commands in the M3 hand-off.

## M6 Saves and M7 Mods: logic done in the Launcher (pending Verifier; game side not run)

Decisions: Save import and Workshop sync live in the Launcher as Swift code
(`native/mac/launcher/Sources/SaveImport.swift`, `WorkshopSync.swift`, `SyncSupport.swift`).
The Launcher is the bundle's main program and always runs first (also with "Skip the
Launcher", which only skips the window), so both run before every game start.
Contracts: `docs/mac/launcher.md` ("Save import", "Workshop sync").

- Saves: the Mac save folder is `<support>/saves`, passed to the game as its save dir
  (`launch-settings.json` key `save_dir`). The game keys its files below it as
  `documents/my games/binding of isaac repentance+/persistentgamedata{1,2,3}.dat`
  (observed in `.scratch/save-m1`; the exe's format strings are `%spersistentgamedata%d.dat`,
  `%sgamestate%d.dat`, `%srep+rerunstate%d.dat`, `%s/options.ini`). Only
  `persistentgamedata1..3.dat` are imported (not a run in progress, not options.ini).
- No Steam Cloud save for Repentance+ exists on this Mac: Steam's `userdata/79444488/` has no
  `250900/` folder (only `7/remote`, Steam's own), and no `persistentgamedata*.dat` exists under
  the fallback game folder. So today the import does nothing and the game makes a fresh save.
- Workshop: Mac Steam's `steamapps/workshop/content/250900` does not exist yet, so the fallback is
  used. The fallback folder is not in the repo: `REPENTANCE_MAC_WORKSHOP_FALLBACK` or
  `<support>/sync-config.json` (`workshop_fallback`, `save_search`). It holds 95 mods (matches the
  95 entries in Steam's `ugc/250900_subscriptions.vdf`; the brief's 96 is off by one).
- Tests: `native/mac/launcher/Tests/run.sh <scratch dir>`: 35 checks pass (see launcher.md).
  Real sync into a scratch support dir: 95 mods copied in about 2 s, 402 MB logical, about 12 MB
  physical (APFS clones); a second run copies 0.

## M8 Layer: mod scan and layer-info (Launcher side; pending Verifier, game side not run)

Notes (Implementer, mod-scan piece):

- The gate's input, `modscan.lua`, is produced by the Launcher: `native/mac/launcher/Sources/ModScan.swift` is a
  faithful Swift port of `createModScan` / `stripLua` / `scanLua` (`scripts/recomp/web/mods.mjs`), run by
  `PreLaunch.run()` after the Workshop sync over the enabled mods of `<support>/mods`; output
  `<support>/modscan.lua` (also `modscan_path` in `launch-settings.json`), cache `<support>/modscan-cache.json`.
  Open item for the Host: serve that file at the virtual path `mods/repentogon/modscan.lua` (the Browser Build seeds
  `c:/isaac/mods/repentogon/modscan.lua`); see launcher.md "Mod scan".
- Golden test (`Tests/run.sh`, `Tests/golden/`, `Tests/golden_scan.mjs`): Swift and the JS `createModScan(...).lua()`
  give byte-identical files on 17 edge-case fixture files and on the 95 Workshop mods (1,207 `.lua` files; set
  `REPENTANCE_MAC_GOLDEN_WORKSHOP`), and stripLua / scanLua agree per file. Cold scan of the 95 mods about 1.5 to 2 s,
  cached about 0.3 to 0.5 s (JS about 3 to 4 s).
- `native/mac/launcher/gen_layer_info.py` writes `layer-info.json` (layer_version from `rgon.lua` VERSION, coverage
  counts from `gate_names.lua` and `enums_ex.lua`); static estimates until a `lab_names` run's log is passed with
  `--lab-log`. To be wired into `build.sh` at M11.

## M2 Picture: done (pending Verifier)

Status: `isaac-mac` opens a 960x540 SDL3 window, creates an OpenGL ES 3.0 context
through ANGLE on Metal and renders the intro, the title screen, the file select
and a Basement I room. `GL_RENDERER` reads
`ANGLE (Apple, ANGLE Metal Renderer: Apple M2, Version 27.0.1 (Build 26A434))`.
Frame pacing: 60.0 fps average over every 120-frame window in a room (min/max
per window in the log, about 15-30 ms on isolated frames).

How it fits together:
1. Upstream's GL backend (`host_gl_webgl.c`) now compiles natively too (guard
   `ISAAC_WEB || ISAAC_NATIVE`; the Khronos GLES3 headers, linked against ANGLE's
   libGLESv2). Browser-only parts stay under `ISAAC_WEB` (the EM_JS present and
   stack log; `glGetBufferSubData` is a mapped-range read natively because ANGLE
   does not export it). `host_gl_clientarrays.c`, SwapBuffers, `glGetIntegerv`,
   `wglCreateContext/MakeCurrent` and the exit report got the same native arm.
2. Window, context, swap, pacing and screenshots: `native/mac/host/mac_gl.c`.
   `ANGLE_DEFAULT_PLATFORM=metal` is set before `SDL_Init` (SDL's Cocoa EGL path
   ignores platform attributes). Swap interval 1 plus a 16.667 ms limiter, so a
   120 Hz display still runs at 60. SDL events are pumped once per present;
   quit (close button, SIGTERM, Cmd+Q) posts WM_CLOSE to the game, which exits
   through its own path (`main returned 0`); after 90 more frames it is forced.
3. Headless is kept: `--headless` or `ISAAC_MAC_HEADLESS=1` creates no window and
   loads no ANGLE context. The fake GL of M1 lives in the same binary: its entry
   points are renamed `fakegl_imp_*` (header generated by `build_native.py`) and
   the import table is pointed at them at startup, so the real backend is the
   default and headless needs no other build.
4. Scripted input, `native/mac/host/mac_script.c` (reusable, not temporary):
   `ISAAC_MAC_SCRIPT` feeds `isaac_input_key` through the Host's weak
   `isaac_input_poll_js` hook, as the Browser Build's page does. Live SDL
   keyboard/mouse/pad is M3.
5. Screenshots: `ISAAC_MAC_SHOT=F:path,@F:path` (`@F` is F frames after the
   scripted run's anchor), glReadPixels from the default framebuffer to an
   uncompressed RGB PNG (`mac_png.c`, no dependency).
6. Frame timing line every 120 frames (`ISAAC_MAC_STATS_EVERY`), and a steady
   state summary at exit. `ISAAC_MAC_VSYNC=0`, `ISAAC_MAC_UNCAPPED=1` for tests.

Run windowed (the scripted sequence is `drive_interactive.mjs`'s: Enter tapped
every 1.5 s until the game logs `Room 1.2(Start Room)`, then D held 1.5 s, W held 1.5 s):

    ISAAC_MAC_SCRIPT='until=Room 1.2(Start Room):enter,@120:d:hold=90,@240:w:hold=90' \
    ISAAC_MAC_SHOT='290:.scratch/m2-shots/title.png,@20:.scratch/m2-shots/start_room.png,@400:.scratch/m2-shots/start_room_walked.png' \
    output/mac/isaac-mac output/recomp/host/isaac.segs.bin .scratch/game-bundle main .scratch/m2-save-w

Headless (M1): `ISAAC_MAC_RUN_FRAMES=1500 output/mac/isaac-mac --headless output/recomp/host/isaac.segs.bin .scratch/game-bundle main .scratch/m2-save-h`.

Decisions: window is created at 1x (960x540; the game's native client size)
until M5; `ISAAC_MAC_SCALE` is reserved. Relative screenshot paths are relative to
where `isaac-mac` was started (the driver chdirs into the instance). No GL errors
or shader compile failures appear in a full title-to-room run.

Known: the game's own work is about 13 ms of each 16.7 ms frame in a room, so
headroom is thin; 3-5 isolated frames per 120 take 20-30 ms, the average holds.

## M1 Headless main runs: done (pending Verifier)

Status: `isaac-mac ... main` loads every archive and Lua script, runs the game
past 20,000 presented frames (the intro cutscene, then the menu path) and exits
cleanly through a frame watchdog; the guard canary is intact.

Causes and fixes:
1. Large archives failed to open: `mac_main.c` chdirs into the instance while
   `g_mac_instance_dir` stayed relative. Both the instance and save dirs are now
   resolved with `realpath` first (`native/mac/host/mac_main.c`).
2. "attempt to load a text chunk (mode is '')": natively Guest address 0 is a
   readable page (like WebAssembly), so `S_ARG`/`P_ARG` in `host_lua.c` turned a
   Guest NULL into a pointer to a zero byte (""). Both now map Guest 0 to NULL
   (harmless for the Browser Build, where 0 already is NULL).
3. "Modding data path ... is not a writable directory" MessageBoxes: the native
   `getcwd` leaked the real instance path to the Guest (GetCurrentDirectoryA,
   GetModuleFileName*), so its `mods/` and `data/` keys missed the RAM
   filesystem. The Guest now sees cwd "/" as under Emscripten
   (`guest_getcwd` in `host_shims_misc.c`); the real chdir stays for the Host Lua loader.

Decisions: `ISAAC_MAC_RUN_FRAMES=N` starts a watchdog that, after N presented
frames, checks the guard, prints `[mac] reached N frames ... guard intact` and
exits 0 (unset: run until the game exits). stdout is line buffered.

Evidence: `ISAAC_MAC_RUN_FRAMES=20000 output/mac/isaac-mac output/recomp/host/isaac.segs.bin .scratch/game-bundle main .scratch/save-m1`
exits 0. Error lines match the Browser Build's boot log
(`output/recomp/local-smoke/page.log`) exactly: XInputGetCapabilitiesEx,
EOS handle, 7 optional `repentance_*.a` language archives, options config.

## M4 Sound: done (pending Verifier)

Status: music and effects play through an SDL3 audio stream. The Browser Build
has no C mixer (WebAudio mixes), so `native/mac/host/mac_audio.c` provides the
strong `isaac_audio_backend_*` hooks (weak in `host_audio.c`) and mixes itself:
PCM converted once to float stereo, per-source head entry/position/gain/pitch/
loop, linear-interpolated resampling to the device. No shim file changed.

Format and pump: 48 kHz, 2 ch, float32 stream on the default playback device;
SDL converts to the hardware. Pump is on demand: SDL's get-callback asks for N
frames and the mixer renders exactly N under one mutex (a per-frame push would
crackle whenever a game frame ran long; the 1024-frame period is 21 ms). Streams
that run dry idle until the game queues more. Music/SFX options reach the mixer
as AL_GAIN per source (the engine sets them); nothing else is applied. Default
device changes are followed by SDL itself (default-device stream). Headless or
no device: a null sink drains the mixer on the wall clock (`ISAAC_MAC_AUDIO_NULL=1`
forces it; `ISAAC_MAC_AUDIO=0` disables).

Counters: `[mac][audio] mixed ... cb, queued, voices, underruns, overruns, late,
peak, rms` once a second (`ISAAC_MAC_AUDIO_LOG=0` mutes). Underrun = callback
found the stream empty well after the previous supply should have played out;
late = callback slower than real time; overrun = request clamped to 0.5 s.
`ISAAC_MAC_AUDIO_DUMP=/abs/path.wav` writes the exact float32 stereo samples
sent to SDL (header patched each second; give an absolute path, the driver
chdirs).

Evidence (`.scratch/m4/`): 9000-frame windowed run with scripted firing,
152 s of audio: 7108 callbacks, underruns 0, overruns 0, late 0; 0 silent
seconds; peak 0.36; 2nd-difference spikes show no enrichment at callback
boundaries (237 vs 243 expected by chance); headless run exits 0 with voices
mixed. Not verified by ear.

Verified: Verifier PASS. Music and effects are non-silent, 0 underruns and 0
overruns, 3 late callbacks at a room-load stall in a 148 s run, no crackle in
the dump analysis, MusicVolume and SFXVolume respected. Known gaps: no
positional audio or panning, because the backend hooks receive no positions
(the Browser Build's `host_audio_web.c` has the same interface and does not pan
either: no panner or position code in it); the atexit `[mac][audio] final`
line never prints because the RUN_FRAMES watchdog and exit path use `_exit`.

## M8 Layer, round 2: engine hooks, more mods Shown, real floor changes

- Audio: `mac_audio.c` `isaac_mac_audio_tick` no longer calls `SDL_GetAudioStreamQueued` with `g_mu` held (lock-order inversion with `stream_cb`, a hang at frame 871). Windowed run of 20,500 frames (about 5.7 minutes), 340 audio reports, no stall (`.scratch/m8/audio-long.log`).
- Floor changes: `lab_trapdoor.lua` spawns a trapdoor two tiles from the player in each start room, waits for it to open (state 1), puts the player on it; the game's own logic does the rest. Console notes: Escape closes the console (grave types a backtick), `gridspawn 9000` is the trapdoor and its place is random, an existing `gamestate1.dat` makes Enter pick Continue (`.scratch/m8/fresh.sh`).
- Hooks (lift pipeline edited: `lift_patches.py`, `hook_table.py`, `hook_slots.lua`; `apply_wrap_patches` on the lifted tree, `build_native.py` recompiles two TUs): slot 171 `NightmareScene::Render`, 172 `Game::SaveState`, 173 `LuaCallbackCaller::CallInputAction`. Details and addresses: `docs/mac/layer-gaps.md` section 3b.
- Layer Lua: `MC_MAIN_MENU_RENDER`, `HasMirrorDimension`, `MainMenu`, `NightmareScene`, `WorldToMenuPosition`, menu offsets measured (labs `lab_scan`, `lab_vp`, `lab_menu`), enum tables counted as implemented, wheel ids checked (Isaac is entry 1; 0 is random).
- Measurement: `lab_names.lua` on the 95 mods: 657 names, 138 callbacks; gate JSON `.scratch/m8/gate-all2.json`.
- Not done: Custom Stage API and External Item Descriptions (structures to measure), Epiphany's remaining 112 names, Epiphany's characters on the wheel (not reached even with every mod achievement unlocked), a Browser Build rebuild and run (only the host sources' syntax check), a Verifier.

## M8 Layer, round 1 (Layer engineer): Epiphany runs, real gap list

Scratch under `.scratch/m8/` (support dirs `supE` = Epiphany + Custom Stage API, `supAll` = the 95 Workshop mods; `run.sh <support dir> <log>` with `FRAMES`, `SCRIPT`, `SHOT`, `DBG`, `VAN`, `ISAAC_RGON_LAB`; `stage2.txt`/`long.txt` are the scripts; logs `e*.log`, `r*.log`, `all*.log`, `long1.log`; shots `q*.png`, `L*.png`; `implemented.txt`, `callbacks.txt`, `gate-all.json`).

- **Root cause of Epiphany's stop**: not a Mac Host gap. REPENTOGON's `Sprite(path, load)` constructor was missing from the Layer, so Epiphany's menu sprites were empty; then `Sprite.Color` is a const copy in the game, `BlendMode`/`ColorModifier`/menus and a dozen more names, one after the other (list and fixes: `docs/mac/layer-gaps.md` section 3). All in the Layer's Lua (`api_sprite.lua`, new `api_gaps.lua`, new `api_menus.lua`, `api_data.lua`), so the Browser Build gets them too.
- **Mod-less Layer save file**: `host_rgon.c` bridge `save_read`/`save_write` + `isaac_fs_store` (`host_shims_fs.c`); `api_data.lua` uses it only under `ISAAC_NATIVE` (flag `B.SAVEFILE`) when the layer registered no mod. Test: `lab_store.lua`, file `rgon-layer-1.dat` written in the first launch, read back in the second (`.scratch/m8/st1.log`, `st2.log`). Browser Build keeps its mod data; the browser host sources compile (`emcc -fsyntax-only` of `host_rgon.c`, `host_shims_fs.c`); the browser itself was not run.
- **Real measurement**: `lab_names.lua` run on all 95 mods: 547 names, 131 callbacks implemented; gate: 3 shown (Epiphany, Door Overhaul, Enhanced Boss Bars), 8 hidden. Epiphany 311 used, 120 missing.
- **Fired's Angels and Statues** (`main.lua:18`): the mod's own bug, the global `AngelStatueMod` is never defined (identical under Vanilla Launch).
- `stage_layer.py --help` prints the usage instead of making a folder named `--help`.
- Not done: Epiphany's remaining 120 names (mostly new engine hooks), the hidden mods' small names, Epiphany's character menu seen in a window, a full Epiphany run beyond floor 4 / to the end, and the Verifier.

## Integration (M6/M7/M8 game side): implemented and tested headless and windowed (pending Verifier)

Nothing committed. Last build compiled clean (`python scripts/mac/build_native.py --jobs 3`).

### Implemented
- `native/mac/host/mac_launch.{c,h}` (new): `--launch-settings <json>` / env `REPENTANCE_MAC_LAUNCH_SETTINGS`, tiny JSON reader, flags `--vanilla --debug-console --lua-debug --segs --bundle --layer-dir --save-dir --mods-dir --modscan --gate-log --stage`. Precedence flag > JSON > env > default. Defaults for segs/bundle/layer: `<exe dir>/../Resources/{isaac.segs.bin,game-bundle,layer}` (M11 bundle layout). Optional JSON keys `segs_path`, `bundle_dir`, `layer_dir`. Positional dev form still works. Guest argv gets `--luadebug` for lua_debug (`mac_guest_argc/argv`, used by `host_boot.c` and `host_shims_gaps.c`, ISAAC_NATIVE-guarded). Debug console: `mac_launch_frame()` (called from `isaac_mac_present` in `mac_gl.c`) sets Options.EnableDebugConsole (Manager+0x2a398) once, both directions (the launch settings are authoritative; they override options.ini).
- `native/mac/host/mac_main.c`: uses the above, sets save/mods/layer/modscan/gate paths, Vanilla Launch sets `ISAAC_RGON=0` and writes gate log `layer_on:false`, calls `mac_seed_mods()`. Header usage comment updated.
- `native/mac/host/mac_fs.c`: mods seeded lazily under both roots (`mods/` and `documents/my games/binding of isaac repentance+/mods/`); lazy reads map to `mods_dir`; only `disable.it` is ever written/deleted in mods_dir (case-resolved folder); saves written atomically (tmp+fsync+rename); Layer served by `isaac_mac_layer_peek` (hooked into `isaac_fs_peek` in `host_shims_fs.c`) at `mods/repentogon/*`, never in the file table so not listed as a mod; `modscan.lua` served from `modscan_path`; `isaac_mac_lua_path` maps `//mods/<dir>/x.lua` for `luaL_loadfilex` (`host_lua.c`).
- Off-mod seeding rule: a mod with `disable.it` is seeded as `metadata.xml` + `disable.it` only, and only in the `mods/` root. The game deletes `disable.it` in the Documents-root copy when it scans it, so that root gets enabled mods only. An off mod therefore has no `main.lua` and no `content/` visible to the game at all.
- `native/mac/host/mac_gate.c` (new): `isaac_mac_gate_log` writes `REPENTOGON.Extras.Gate.mods` to `gate_log_path` (launcher.md format, plus `scanned`), called from `l_gate` in `host_rgon.c`; `isaac_mac_gate_log_off` for vanilla/no layer.
- `scripts/recomp/host/src/host_shims_fs.c`: native fs key array 160 -> 256 (Workshop `<dir>_<id>` paths overflowed; 919 files were dropped).
- `scripts/mac/stage_layer.py` (new): stages the Layer files (no main.lua/metadata.xml) into a folder. (Note: it takes the output folder as its only argument and treats any other first argument, even `--help`, as the folder name.)

### Tested earlier (scratch under `.scratch/integ/`, shots in `.scratch/integ/shots/`)
- Scratch support dir with Epiphany, EID, Enhanced Boss Bars, Instant Fires: headless and windowed runs exit 0, guard intact; the 4 mods load, the Layer loads at the first mod chunk before mod code, gate lines and `layer-gate.json` written; the in-game MODS menu lists the 4 mods and not the Layer.
- Saves: save_dir tree is `documents/my games/binding of isaac repentance+/`; run start wrote `gamestate1.dat` and `persistentgamedata1.dat` at once; second launch restored 9 files, contents/mtimes unchanged.

### Tested at the finish (scratch under `.scratch/fin/`, logs and shots there)
Scratch support dir `.scratch/fin/sup` with three test mods (`probe_1` on, `offmod_2` and `togglemod_3` off), Layer staged by `stage_layer.py` to `.scratch/fin/layer`, `modscan.lua` made by the Launcher's PreLaunch (`.scratch/integ/prelaunch.bin`) with the Workshop and Steam folders pointed at an empty folder.
1. M1 headless, 3000 frames: PASS. Positional dev form (`.scratch/fin/m1-dev.log`) and flag form (`m1-flags.log`): `[mac] reached 3016 frames (wanted 3000); guard intact`, exit 0, no load errors (only the known `_controlfp_s` note and the Browser Build's known option/XInput/EOS lines).
2. Vanilla Launch (`--vanilla`, `vanilla.log`): no `[isaac][rgon]` lines (the Layer never loads) and no gate decisions, only the two `[mac]` Vanilla Launch lines; `layer-gate.json` = `{"layer_on": false, "reason": "vanilla launch", "mods": {}}` (`gate-vanilla.json`), the probe mod still loads and runs (`REPENTOGON=false`). PASS.
3. `--lua-debug`: PASS. `type(io)` is `table` (also `os`, `debug`) with it (`luadebug.log`), `nil` without (`normal.log`, `vanilla.log`). Side finding: without `--lua-debug`, `os` is a table when the Layer is on and `nil` under Vanilla Launch, so the Layer provides an `os` table. Not investigated.
4. `--debug-console`: PASS. Log: `debug console on by the launch settings (was off in options.ini)`. Windowed scripted run (`dc.log`): accept the welcome popup, start a run, press grave 120 frames after the start room: `dc-console.png` shows the "Repentance+ Console" prompt and command list over Basement I. Control run without the flag (`dc-off.log`): same key presses, no console (`dc-control-off.png`). The console does not open on the title screen, only in a run. Screenshots come from `ISAAC_MAC_SHOT`.
5. Off mods: PASS.
   - Greyed: the in-game MODS menu (`mods-off.png`, Tab on the main menu) shows `OffMod` and `ToggleMod` greyed and `Probe` black.
   - No Lua: headless log has `PROBE ran` only; neither `OFFMOD RAN` nor `TOGGLEMOD RAN` (`normal.log`). `[mac] mods: 1 folder(s) on, 2 off (disable.it); 2 file(s)` shows only the on mod's two files were seeded.
   - Toggle on in-game (`mods-toggled.png`: ToggleMod turns black): log `mods: removed .../togglemod_3/disable.it (mod on)`, the file is gone on disk, `offmod_2/disable.it` untouched. Next launch (`relaunch.log`): `2 folder(s) on, 1 off`, `TOGGLEMOD RAN` logged, `OFFMOD RAN` not.
   - `content/` of an off mod: it is never served (by the seeding rule above), so its `items.xml` cannot take effect. The game still prints `LOADED MOD //mods/offmod_2/content/` for it, so that line only says the game listed the folder, not that its content is loaded. What the game itself would do with a served `content/` next to `disable.it` was not tested (the seeding rule makes it moot).

### Half-done / open
- (Resolved in M8 round 1, see the M8 section below: Epiphany's `epiphany_menu.lua:35` stop was REPENTOGON's `Sprite(path, load)` constructor missing from the Layer.)
- (Resolved in M8 round 1: the Layer keeps its save data in `rgon-layer-<slot>.dat` through the Host when it has no mod.)
- Gate ids come from folder names; the Launcher's sanitised ids vs chunk-name folder names could differ for odd names.
- Windowed runs can be killed with rc 137 on the 8 GB machine; a rerun was fine (none happened in the final runs).
- Tests run with the scratch mods only; Epiphany and the 95 Workshop mods were not re-run after the last edits.

### Next steps
1. Verifier re-runs the checks above (and the save and Workshop tests of M6/M7 against the real game).
2. (Done in M8 round 1.)
