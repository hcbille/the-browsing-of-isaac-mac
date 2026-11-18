# Mac Build progress

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
