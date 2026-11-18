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
| M5 Window | Not started. Hook point: `isaac_mac_window_to_game()` in `mac_input.c`; window is fixed 960x540 in `mac_gl.c` (`ISAAC_MAC_SCALE` reserved) | none | none |
| M6 Saves | **Verified** (progress survives restart; import cases cloud-present, absent and Mac-save-present pass; fixture Steam folders untouched; evidence `.scratch/verify67/`) | M6+M7 commit | pending |
| M7 Mods | **Verified** (95 mods synced, checker mod off; MODS menu lists 95, off greyed; Launcher and in-game toggles agree; update keeps `disable.it`; unsubscribe keeps the mod, marked, Remove moves to Trash after confirm). Side notes: Launcher sorts by folder name but the game menu sorts by display name; `fired's_angels_and_statues` main.lua logs a Lua error (M8) | M6+M7 commit | pending |
| M8 Layer | Swift mod scan byte-identical to JS `createModScan` on all 95 mods; `gen_layer_info.py` written; Layer served at `mods/repentogon/` and gate log written by the game. Epiphany stops in `epiphany_menu.lua:35` ("no animation named Overlay"). Not verified | uncommitted | none |
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
- Epiphany main.lua stops at `epiphany_menu.lua:35` ("no animation named Overlay", `no member named 'A'`), and a `ISAACSAVEMANAGER_POST_DATA_LOAD` error follows: Layer/engine gap for M8, not investigated.
- The Layer's own mod shell (main.lua, `RegisterMod("REPENTOGON (browser)")`) is not run, so the Layer's save file (mod achievements, innate items, api_data.lua) has no mod to hang on: needs a Layer change.
- Gate ids come from folder names; the Launcher's sanitised ids vs chunk-name folder names could differ for odd names.
- Windowed runs can be killed with rc 137 on the 8 GB machine; a rerun was fine (none happened in the final runs).
- Tests run with the scratch mods only; Epiphany and the 95 Workshop mods were not re-run after the last edits.

### Next steps
1. Verifier re-runs the checks above (and the save and Workshop tests of M6/M7 against the real game).
2. Give the Layer a mod-less save hook; fix Epiphany's menu error (M8).
