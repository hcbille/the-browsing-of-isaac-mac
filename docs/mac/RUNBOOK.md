# Mac Build runbook

How to build, run, test and change the native Apple Silicon Mac Build of Repentance+, written from what
has been learned so far. Read this first. For details, follow the links into the other files in this folder.

Paths below: `<repo>` is the checkout of this repository on branch `mac-native`, `<toolchain>` the toolchain
folder next to it (`<toolchain>/env.sh`), `<game>` the Windows depot of the game, `<workshop>` its
`steamapps/workshop/content/250900`, `<public>` the checkout of the public Mac-files repo.

## 1. Where things stand

| Milestone | State |
|---|---|
| M1 headless, M2 picture, M3 input, M4 sound | Verified, committed, published |
| M5 window (integer/fill scaling, fullscreen) | Verified, committed, published |
| M6 saves, M7 mods (Workshop sync, `disable.it`) | Verified, committed, published |
| M8 Layer (REPENTOGON support) | Rounds 1-2 committed. Verified 2026-11-18 (Verifier: Epiphany Basement I to Caves I, 95 mods with no new Lua error, headless 3000 frames). 5 of 11 REPENTOGON-using mods Shown (Unique Items API hidden again: `MC_MENU_INPUT_ACTION` and `MC_PRE_GLOWING_HOURGLASS_LOAD` not counted until seen firing); Epiphany's Tarnished page works (`Sprite:Continue`, user-checked) |
| M9 Steam | Design and static trace pass done; bridge code written (`native/mac/host/mac_steam.c`), syntax-checked only, not built or run (`steam-bridge.md` section 8) |
| M10 Launcher | Built, tested against a fake game; not run against the real game from the app |
| M11 app + `build.sh` | Draft: light stages tested, signed app opens; heavy stages never run from scratch; two follow-ups open |
| M12 publish | Per milestone; README not updated |

Working status (what runs now, the plan, open questions) lives in the git-ignored `notes/` folder of the work
checkout (`notes/current.md`, `notes/NEXT.md`, `notes/PROGRESS.md`). Uncommitted work at any moment: `git status`.

## 2. Ground rules that don't bend

- Nothing game-derived goes into the public repo or into any commit (no exe, assets, Ghidra output, lifted C,
  objects, upstream's game-translated sources). Publishing check: section 9.
- Public commits use the GitHub noreply address. Never the user's own address.
- Never write to Steam Cloud, the Workshop folder, Steam's folders, or an existing Mac save
  (`~/Library/Application Support/Repentance+ Mac/`). Tests use scratch support folders.
- Never write C through a shell heredoc or `echo` (backslashes get stripped). Use the editor tools.
- No absolute home-folder paths (the macOS user folder) in anything under `docs/` or `scripts/` (both are published).
- `package-lock.json` has an unrelated change. Never commit it.
- Keep the Browser Build working: Mac-only C changes go behind `#if defined(ISAAC_NATIVE) && ISAAC_NATIVE`;
  Layer Lua and engine hooks must work in both builds.

## 3. Machine limits (8 GB RAM M2, about 12 GB disk free)

- One heavy job at a time: a compile (`build_native.py`, lift, Ghidra) or one game instance. Two games at
  once have been killed by memory pressure (exit code 137). A light Swift build or a read-only search can
  run next to one game.
- macOS has no `timeout`. Wrap every game run: `perl -e 'alarm 600; exec @ARGV' <command>`.
- Check `df -h /` before anything big. Logs can explode (one Lua hook loop wrote 454 MB in minutes):
  look at log sizes after a run and delete your own scratch output.
- `screencapture` is denied. Screenshots come from the game itself (`ISAAC_MAC_SHOT`) and the Launcher
  (`REPENTANCE_MAC_LAUNCHER_SNAPSHOT`).

## 4. Build

```bash
cd <repo>
source <toolchain>/env.sh
python scripts/mac/build_native.py --jobs 3        # -> output/mac/isaac-mac, about 15 s incremental
```

- `build_native.py` compares file times only. After changing a header (e.g. `mac_launch.h`), touch the `.c`
  files that include it, or delete `output/mac/obj/host/*.o`, or stale objects crash at start.
- Layer Lua needs no build: stage it with `python3 scripts/mac/stage_layer.py <dir>` and point the game at
  `<dir>` (`--layer-dir`). Staging takes a snapshot, so later edits don't change a running game.
- An engine hook (new row in `scripts/recomp/rgon/tools/hook_table.py`) changes the lifted game:
  `hook_table.py --write`, then `apply_wrap_patches` on `output/recomp/lift/gu`, then `build_native.py`
  (only the touched lifted files recompile). Recipe: `layer-gaps.md` section 5. In practice (hook 174, 2026-11-19):
  `hook_table.py` (no `--write`) checks the row's `ret` but fails the reg/sig rows when the prebuilt outputs are gone, so add
  the row by hand to `lift_patches.py` `RGON_HOOKS` and `hook_slots.lua`; apply with
  `cd scripts/recomp/lift && python3 -c "from pathlib import Path; import lift_patches as L; L.apply_wrap_patches(Path('<repo>/output/recomp/lift/gu'))"`
  (about 2 min, rewrites only the TU holding the function); build with `cp -Rp output/mac <dir>` then
  `build_native.py --out <dir>` (10 s, keeps the other objects) and run it with `ISAAC_BIN=<dir>/isaac-mac` when the tree holds
  other workers' uncommitted host changes. A Layer that arms a slot with no wrapper in the binary fires nothing (silently).
- The Launcher: `native/mac/launcher/build.sh <out dir>`; tests `native/mac/launcher/Tests/run.sh <scratch dir>`.
- The whole app: `scripts/mac/build.sh --game <game>` (`build.md`). Stages 1-8 (Steamless to
  `build_native`) are adopted from existing outputs and skipped; 9-14 (Launcher, Layer, bundle, dylibs,
  config, sign) run. A from-scratch run of 1-8 has never been done.
- The game build is pinned in `scripts/mac/game-pin.txt` (version, Steam build id, exe SHA-256, depot and
  manifest ids). `build.sh` refuses any other `isaac-ng.exe` (`--allow-other-exe` to override);
  `scripts/mac/fetch-game.sh` downloads exactly that build with SteamCMD. To move the pin: new manifests
  from `appmanifest_250900.acf`, the new exe hash, edit the file; then redo the lift, `CANON_SHA` and every
  Layer offset. A new DLC depot is one line (id, manifest from that build's `InstalledDepots`, dlcappid, name). Details: `build.md`, "Getting the pinned game, and moving the pin".
  Verified 2026-11-19 with a real account: Steam still serves all five pinned manifests (about 2 GB). The
  merged folder matched the reference install file for file (1,203 files, same SHA-1), apart from the
  shared VC redist depot, which is not needed. Homebrew's `steamcmd` is a wrapper, so the depots land in
  `<Caskroom>/steamcmd/<ver>/MacOS/steamapps/content/`. SteamCMD prints that path with backslashes, and
  the script reads it from SteamCMD's output. If the merge step fails, rerun with
  `--merge-from <that content folder>`; no new download is needed.

## 5. Run

### Headless check (the regression test after any change)

```bash
ISAAC_MAC_RUN_FRAMES=3000 output/mac/isaac-mac --headless \
  output/recomp/host/isaac.segs.bin .scratch/game-bundle main .scratch/<name>/save
```

Pass: exit 0 and `reached N frames (wanted 3000); guard intact`. The only expected error-like line is the
`_controlfp_s` note (plus `Cannot load GameState if PersistentData is missing!` with an empty save folder).

### A real game with mods (scratch support folder)

The game takes a `launch-settings.json`; flags win over the JSON, which wins over env vars
(`launcher.md`, "Game side"). The runner is `scripts/mac/run.sh <support dir> <log>`. It stages a fresh
Layer into `<support>/.layer`, writes the JSON (support, mods, saves, gate log, modscan, layer, segs, bundle),
and runs the game under `perl alarm`. It refuses to start while a game runs. Env knobs (all in its header):
`FRAMES`, `SCRIPT`, `SHOT`, `LAB=lab_trapdoor.lua` (copied from `scripts/recomp/rgon/`), `HEADLESS=1`,
`FRESH=1` (runs `scripts/mac/fresh.sh` first), `DBG=true`, `VAN=true`, `ALARM`, `BUNDLE`. Example, headless
to Cellar I in about 10 s: `HEADLESS=1 FRESH=1 FRAMES=900 LAB=lab_trapdoor.lua
SCRIPT='until=Room 1.2(Start Room):enter' scripts/mac/run.sh <support> <log>`. To fill a support folder with
the 95 Workshop mods, run the Launcher's
pre-launch (`.scratch/verify67/prelaunch.bin`) with `REPENTANCE_MAC_SUPPORT_DIR=<support>`,
`REPENTANCE_MAC_WORKSHOP_DIR=/nonexistent`, `REPENTANCE_MAC_WORKSHOP_FALLBACK=<workshop>`,
`REPENTANCE_MAC_STEAM_USERDATA=<empty dir>`.

**Clean up after every test run (user rule, 2026-11-19).** When a test run is finished, delete the big
folders it made: the support folder with its copied mods (0.4 to 1.1 GB each), copied game bundles, app
copies. Keep only the evidence (screenshots, logs, the run's commands) in its `.scratch/<task>/` folder. Don't
copy the mods into a new support folder for each run; reuse one, or use one without mods when the test doesn't
need them. Never delete `.scratch/play/` (the user's play setup), `.scratch/m11/out` (the built app),
`.scratch/game-bundle`, `output/`, or anything outside `.scratch/`. The disk is small: 6.3 GB of leftover
support folders had built up before this rule.

**The app itself, without touching the real save**: `open -n <app> --env REPENTANCE_MAC_SUPPORT_DIR=<scratch
support>` plus `--env ISAAC_MAC_SCRIPT=...`, `--env ISAAC_MAC_SHOT=...` and `--env ISAAC_MAC_RUN_FRAMES=...`.
The Launcher passes its environment to the game. `{"skip_launcher": true}` in that folder's
`launcher-settings.json` makes it press Play by itself. `REPENTANCE_MAC_LAUNCHER_SNAPSHOT=<png>` with
`..._SNAPSHOT_DELAY=4` captures the startup window. In `pgrep -f`, escape the `+` of `Repentance+.app`, or
match `MacOS/isaac-mac`.

**Steam**: the bridge is off unless `ISAAC_STEAM=1`. A build bundles the real `libsteam_api.dylib`, and a
live bridge registers as app 250900 with the user's Steam client and can write achievements to the real
account. Turn it on only for the M9 checks with the stub (`ISAAC_STEAM_DYLIB=<stub>`).

### Playing the current build by hand

`.scratch/play/play.sh` opens the Launcher of the draft app; its Play button runs the current
`output/mac/isaac-mac` with a fresh Layer snapshot, support folder `.scratch/play/support`. Don't play while
an agent runs a game (memory).

`.scratch/play/play.sh --unlock-all` does the same and also puts `lab_unlock_all.lua` into the Layer snapshot
(`game-wrapper.sh` then sets `ISAAC_RGON_LAB`). When a save file is picked on the title screen, the lab calls the
game's own `TryUnlock` for achievements 1..637 and the Layer's `unlock_modded` for every mod achievement, and logs
`lab_unlock_all: slot N: game achievements 1..637: A already unlocked, B newly unlocked, C failed; 637 unlocked in
total now; M mod achievement(s) newly unlocked` in `.scratch/play/support/logs/game.log`. First run on the play
save: 31 already, 606 new, 0 failed, 136 mod achievements. The user confirmed all Tainted characters show on the character menu afterwards. Only that play save is touched. Before any agent
game run or heavy compile: `pgrep -f isaac-mac` must be empty (the user may be playing).

## 6. Testing recipes and game facts

**Scripted input** (`ISAAC_MAC_SCRIPT`, grammar in the header of `native/mac/host/mac_script.c`):
`frame:key[:hold=N]`, `@N:` relative to an anchor, `until=<log text>:key` taps a key until a log line
appears, plus mouse, pad, `fullscreen`, `window:WxH` tokens.

- A fresh save needs Enter at about 450 and 650 (Welcome popup) and 850 (title); with 95 mods the title
  appears around frame 1000. Then Enter on the file, Enter on New Run.
- `until=Room 1.2(Start Room):enter` gets from boot to the first room.
- If `gamestate1.dat` exists, Enter picks Continue, not New Run. `scripts/mac/fresh.sh <support>` moves it aside.
- **Debug console**: grave (`` ` ``) opens it during a run (not on the title screen); **Escape closes it**.
  Pressing grave again only types a backtick, and every later key then goes into the console as text.
- **Window sizes (adapt is the default scale mode)**: the game lays itself out for the window (see window.md "Full-screen
  adapt"). Check a size with `ISAAC_MAC_SCRIPT='until=Room 1.2(Start Room):enter,@60:window:1280x800,@200:fullscreen:on'`
  and `ISAAC_MAC_SHOT=@150:<png>,...`; the log shows `Viewport: WxH` (the game's own layout) and `[mac][window] adapt:` lines.
  `ISAAC_MAC_SCALE=integer|fill` gives the M5 modes. Full screen over the notch: add
  `DYLD_INSERT_LIBRARIES=<app>/Contents/Frameworks/libNotchFillHook.dylib`. `.scratch/m13-adapt/blocks.py <png>` prints the
  art pixel block size. A game window wider than 1920 pixels crashes the game's layout, so the Host never gives it one.
- **Fiend Folio: RELOADED** (Workshop 3778123093, the Repentance+ one) is on the Layer's hand list in
  `rgon.lua` as `fiendfolio-reloaded`. The host looks mods up by their full folder name, lowercased,
  which on a Mac carries the Workshop id (`fiendfolio-reloaded_3778123093`). The list's metatable retries a
  miss without the trailing `_<digits>`. Without that, Fiend Folio draws "REPENTOGON Required, MISSING!" in
  the first room. Check: `REPENTOGON is visible to the mod 'fiendfolio-reloaded_3778123093'` in the log.
  It adds about 24k files. A hand-installed mod isn't in `modscan.lua` until the Launcher scans again.
- **`[warn] no music for ID n ().` is harmless** (checked 2026-11-19): the mod Music Mod Callback (2491006386) ships a
  `resources/music.xml` with only 13 tracks (title, credits, jingles, voiceovers), which replaces the vanilla table, so the
  stage entries (ids 1..119) are blank on purpose; it plays its own copies (ids 120 and up, from its `content/music.xml`)
  from Lua. The game's room-music code still asks for the vanilla id, and `Music::Play`/`Crossfade` log the warning and do
  nothing when the entry's track path is empty. Windows behaves the same. Music is really playing when `Queued Path
  music/...` lines appear and the `[mac][audio] mixed` lines show rms around 0.05 or more. `lab_music_probe.lua` dumps the
  table (MusicManager +0x31c/+0x320, 0x60-byte entries, track path at +0x34, its size at +0x44).
- **Startup speed**: with Fiend Folio the boot took 181 s, because every DeleteFileA/RemoveDirectoryA
  rebuilt the whole file-table hash (61k entries). Now a delete removes only its own entry, and folder
  listings use a parent→children index, so the boot takes about 10 s. If boot is slow again, run
  `sample <pid> 5` during it before guessing. The steps print as `[mac][boot] status:` lines, which the
  Launcher shows (docs/mac/launcher.md).
- **`gridspawn 9000` spawns a trapdoor** (`17` does not: the console takes room-editor ids). It lands on a
  random free tile, so walking to it blind doesn't work.
- **Floor changes**: use the lab, not walking. `ISAAC_RGON_LAB=lab_trapdoor.lua` (copy it into the staged
  Layer folder; `ISAAC_LAB_FLOORS=N`, default 3) spawns a trapdoor next to the player in each start room,
  waits for it to open, puts the player on it, and logs `NEW LEVEL: stage N '<name>'`. The game's own
  trapdoor logic does the descent. The deepest a trapdoor goes is the Dark Room.
- **Screenshots**: `ISAAC_MAC_SHOT=@F:<png>[,@F:<png>...]` at presented frames (relative to the script
  anchor with `@`). Look at the screenshot yourself before believing a script did what it says.
- **Clean exit**: an `ISAAC_MAC_RUN_FRAMES` or alarm exit skips the game's shutdown, so `options.ini` is not
  written. Send SIGTERM for a clean exit when that matters.
- **Closing the window as the red button does**: script token `F:quit` (mac_script.c; works headless too) pushes WM_CLOSE, so
  the game's own shutdown runs (`Isaac is shutting down...`, `delete N rooms.`, every mod's MC_PRE_MOD_UNLOAD, `main returned 0`).
  Window close, not the menu's Exit, is what logs `[mac] quit requested`. Repro of the shutdown crash below:
  `HEADLESS=1 FRESH=1 FRAMES=6000 LAB=lab_trapdoor.lua ISAAC_LAB_FLOORS=2 SCRIPT='until=Room 1.2(Start Room):enter,@1500:quit'`
  with the 95 mods (quit in the first room does not crash: StageAPI saves only after a floor change).
- **Shutdown crash in `lua_rawgeti` (fixed 2026-11-19)**: at shutdown the game's `LuaEngine::Shutdown` (0x0086e520) walks its mod
  list (0xc7985c) and runs each mod's MC_PRE_MOD_UNLOAD (0x00870e00), which frees and nulls that node's mod LuaRef (node+0x50).
  A mod that calls `Isaac.SaveModData` there (StageAPI) runs 0x008711f0, which walks the list from the head comparing each
  node's LuaRef to the caller's mod table (0x00897540) with no null check; every node before the caller is already nulled, so
  it reads a LuaRef at guest 0, calls `lua_rawgeti(L=0)`, and natively guest 0 is a readable page: Lua reads a null
  `global_State` and faults at 0x40. The game has no guard, so it is probably a latent Windows bug too (the process is leaving
  anyway). Fix: `host_lua.c` answers `lua_rawgeti/lua_compare/lua_settop` on a null `lua_State` with 0 and logs once
  (`[isaac][lua] ... with a null lua_State`); the walk then skips the dead node. Evidence: `[isaac][lua] lua_rawgeti with a null lua_State`
  in a quit run, then `main returned 0`.
- **Labs** are test-only Lua files in `scripts/recomp/rgon/lab_*.lua`, never packed, loaded only with
  `ISAAC_RGON_LAB`: `lab_names` (dumps implemented names: the Layer measurement), `lab_trapdoor`,
  `lab_store` (Layer save), `lab_unlock` (mod achievements), `lab_scan`/`lab_vp`/`lab_menu` (offset probes),
  `lab_cb` (callback firing), `lab_night`.

**Open bug, Lua GC crash (2026-11-18)**: one windowed run with `lab_unlock_all` died with SIGBUS in
`sweeplist` <- `luaC_step` <- `lua_gc` <- `isaac_lua_gc` <- `sub_008657c0`, about 50 frames after the unlock,
reading `0x0000000f00000052`: a GC object's `next` pointer was `0x0000000f00000049` (`marked` is at +9), i.e. two
32-bit guest values over a native 64-bit Lua header. The same lab on the play save did not crash. Treat it as guest
memory corruption into the native Lua heap, not memory pressure. Declared not reproducible (2026-11-19) after
about 45,000 windowed frames with 95 mods, a fresh save and `lab_unlock_all`, with every check below on and no fault.

**Lua heap debug knobs** (`host_lua.c`; all off by default, a build with them compiled in behaves like the release):
`ISAAC_LUA_REDZONE=1` (redzones around every Lua block, freed blocks filled with 0xDD and quarantined,
`ISAAC_LUA_QUARANTINE_MB`, default 48), `ISAAC_LUA_HEAPCHECK=N` (every N-th `lua_gc` call, about one a frame: verify all
redzones, a slice of the quarantine and Lua's own object lists; on a fault it logs `[isaac][luagc] HEAP FAULT` with the
guest address and header, then aborts unless `ISAAC_LUA_HEAPCHECK_ABORT=0`), `ISAAC_LUA_GC_FORCE=N` (a whole collection
every N-th call). Build into its own folder (`build_native.py --out .scratch/<x>/out`), run with `ISAAC_BIN=` in
`run.sh`. Cost with 95 mods (1.1M live blocks): HEAPCHECK=10 about 30 fps windowed. Note the unlock popups: after
`lab_unlock_all` the game shows about 1,000 achievement popups, so tap Enter every 10 frames
(`until=...:enter:from=120:every=10:hold=4`).

**Room-change hang with Epiphany (fixed 2026-11-19)**: the game froze at 99% CPU inside one frame at a room change,
printing `EL: invalid sublist` (the game's per-frame EntityList sublist arena filling up; only a symptom). Cause: the game
gives a mod's `<null>` with no `id=""` the id 0, so `Isaac.GetNullItemIdByName("essence of samson")` was -1,
`GetNullItem(-1)` nil, and Epiphany's `MC_POST_PLAYER_TRIGGER_EFFECT_REMOVED` callback registered with that nil ran for
every removed effect: it `UseActiveItem`s Dull Razor, which adds an effect, which the room change's ClearEffects (it
repeats until a pass removes nothing) removes again, forever. Now an id-less mod null gets a reserved blank id
(`api_lua.lua`, `reserve_nulls`), so the callback matches nothing real; `AddNullItemEffect` ignores such ids. The null's
stats, cooldown and persistence are still not loaded (the id-less mod nulls of Epiphany/Fiend Folio do nothing).
Repro/check: `HEADLESS=1 FRESH=1 FRAMES=1500 LAB=lab_el.lua SCRIPT='until=Room 1.2(Start Room):enter'` with the 95 mods:
before, 449k removals and the alarm; after, `1 effect removals fired`. Evidence: `.scratch/hang-el/`.

**Epiphany's own unlocks**: Tarnished characters, the Tarnished menu page and Epiphany's items are kept in
Epiphany's mod save, not in game or XML achievements; `Epiphany:UnlockAll()` (its console command `unlock-all`)
sets them. `lab_unlock_all` calls it once the save manager has loaded (`Epiphany UnlockAll ok; TARNISHED_MENU = 2`).
Mod achievements do persist across launches (checked by the user); Epiphany's in-mod reset (`wipe-save`) locks them
again. **E** on the character menu swaps pages (normal -> Tainted -> Tarnished with Epiphany); the Tarnished page
needed `Sprite:Continue` (REPENTOGON; added to `api_more.lua` 2026-11-18): the user confirmed the page shows.

**Clocks**: windowed runs use the real clock for `QueryPerformanceCounter`, headless runs the deterministic one
(one tick per query plus each `Sleep`; repeatable). `ISAAC_MAC_CLOCK=wall|deterministic` overrides; the log says
`[isaac][time] QueryPerformanceCounter mode = ...`. On the deterministic clock the Theora intro ran far ahead of its
audio (fixed 2026-11-18, user-checked). The other time imports already read the real time or are harmless stubs.

**Hangs**: if a run stops progressing (frame lines stop, CPU at 0), take `sample <pid> 2` before killing
it. That is how the M4 audio deadlock (lock-order inversion in `mac_audio.c`) was found. Crashes:
`lldb --batch`.

**M9 Steam bridge recipe** (design and verifier steps: `steam-bridge.md` section 8). Off unless `ISAAC_STEAM=1`; then it needs a
`libsteam_api.dylib` in `<exe dir>/../Frameworks`, next to the exe, or at `ISAAC_STEAM_DYLIB`; when off or missing, the
log shows one `[mac][steam] disabled: ...` line and the game behaves as before. Test without Steam against
`.scratch/steam-research/libsteam_api_stub.dylib` (rebuild: `clang -arch arm64 -dynamiclib -install_name
@rpath/libsteam_api.dylib stub_steam_api.c -o libsteam_api_stub.dylib`):
`ISAAC_STEAM=1 ISAAC_STEAM_DYLIB=$PWD/.scratch/steam-research/libsteam_api_stub.dylib ISAAC_STEAM_SELFTEST=1 ISAAC_MAC_RUN_FRAMES=600
output/mac/isaac-mac --headless ... 2>&1 | grep -E 'mac\]\[steam|stub-steam|TRAP'`. Knobs: `ISAAC_STEAM=0` (force off),
`ISAAC_STEAM_DRYRUN=1` (log writes, do not forward), `ISAAC_STEAM_SELFTEST=1`. A `CLOUD-WRITE` line from the stub
means the cloud guard failed. Never run it against the real client without `ISAAC_STEAM_DRYRUN=1` first.

## 7. Working on the Layer (REPENTOGON support)

- Structure, where to add a function, a callback or a hook, and how the gate decides: `layer-gaps.md`
  section 5. Remaining names per mod: section 4. Suggested order: section 7.
- Measure after each batch: run all 95 mods with `ISAAC_RGON_LAB=lab_names.lua` and read the gate lines
  (`[isaac][rgon] gate: <folder> is shown|not shown ...`) and `layer-gate.json`.
- **Honesty rule**: a name counts as implemented only if it really works. An inert stand-in or a hook that
  was never seen firing is a gap, recorded in `layer-gaps.md`, not counted.
- REPENTOGON's offsets are for v1.9.7.12; every offset must be measured in this exe (labs, the game's own
  Lua registrations, signatures). Several "known" offsets were off by 4 or 8.
- Not Layer gaps: `! fired's_angels_and_statues` errors at `main.lua:18` under Vanilla Launch too (the mod's
  own bug).

## 8. Committing

Commit on `mac-native` per verified piece, conventional message, ending with the session's Co-Authored-By
line. `git add` explicit paths (never `-A` at the repo root, never `package-lock.json`). Check the commit for
binaries: `git show --stat HEAD`.

## 9. Publishing

In `<public>`: sync the Mac files (`native/mac`, `scripts/mac`, `docs/mac`, `docs/adr`, `GLOSSARY.md`) from
`git archive mac-native`; regenerate `patches/host-native.patch` from
`git diff b986dd8..mac-native -- scripts/recomp`. Then check: no upstream file except as patch hunks; the
patch's added lines contain no game-derived code; no binaries; a recursive grep for the macOS home-folder prefix finds nothing; every
author and committer is the noreply address; `scripts/mac/apply.sh` applied to a clean worktree of upstream
at `b986dd8` (an absolute path outside the upstream repo, removed afterwards) reproduces `mac-native`'s
`scripts/recomp` and `native/mac` exactly. Commit and `git push origin main`, never force. Don't let a sync
delete `apply.sh`.

## 10. How to work (lessons from the first two sessions)

- **Every correction caught in code becomes a lint rule** in `scripts/mac/lint.py`. The rule's message says what to
  do instead. The same commit removes every existing copy of the mistake. Run `python3 scripts/mac/lint.py` before
  every commit and before every publish; `--list` prints the rules with the mistake each one came from.

- **Small tasks with a concrete finish line.** "Grow the Layer" ran for hours; "make Epiphany's menu sprite
  load, shown by a screenshot" took one pass. Every task names its files, its check and its evidence.
- **Check results by looking.** A script finishing proves nothing: open the screenshot, read the log line.
  The floor-change test looked successful from the script and was not (the console never closed).
- **Ask for game knowledge instead of guessing.** The user knew in seconds that Escape closes the console and
  that the trapdoor is `gridspawn 9000`.
- **Diagnose before handing off.** A one-minute `sample`, log tail or screenshot often names the cause; hand
  the fix over with the cause.
- **Timebox open-ended work** (Layer gaps). Stop, measure, commit what works, then pick the next batch.
- **Hand hard-to-automate checks to the user.** Anything that can't easily be tested programmatically
  (something only visible in a menu that scripted input can't reach reliably, feel, sound quality, a Steam login)
  goes to the user with exact instructions: the command to run, what to do in the game, what they should see, and
  which log line to report or that the agent will read. Don't spend an agent hour scripting it. Example:
  `play.sh --unlock-all`, then the Tarnished page on the character menu.
- **Commit as you go.** Verified work sitting uncommitted for hours is a risk and hides progress.

## 11. Next steps, in order

1. **Secure what works.** Quick check of the uncommitted work (headless M1; a windowed Epiphany run with
   `lab_trapdoor` to Caves I; 95-mod run with no new Lua error; Launcher tests), then commit in pieces:
   the audio deadlock fix (`mac_audio.c`), the Layer rounds 1-2, the Launcher's bundled `sync-config.json`
   merge, `build.sh` + `build.md` + `apply.sh` + `make_icon.py`. Publish.
2. **Make the app playable end to end** (user value first): done 2026-11-19 except the user's check.
   ANGLE loads from the app's Frameworks folder. `build.sh` rebuilt the app from the current game. Play from
   the app reached the first room with 95 mods, and Fiend Folio sees REPENTOGON. `run.sh` and `fresh.sh` are
   in `scripts/mac/`, `lab_unlock_all.lua` is in `scripts/recomp/rgon/`. Left for the user: double-click from
   Finder, which uses the real save folder.
3. **M9 Steam bridge**: verified with the stub 2026-11-19 (steps 1-5 and 7 in `steam-bridge.md` 8.4; 637
   SetAchievement calls from the game's own TryUnlock reach it). Off unless `ISAAC_STEAM=1`. The game calls
   `ResetAllStats(true)` when the stat `version` isn't 1000; it is refused and never forwarded (it would wipe a
   real account). Before a real client is ever used: replace the token pool (it borrows never-called rows
   such as `DefWindowProcA`, `_purecall` and `__CxxLongjmpUnwind` while live) with PROVIDED rows in
   `gen_shims.py`. Find out why the console's `achievement N` doesn't reach Steam. The user then runs steps
   6 and 8, dry run first. Original plan, per `steam-bridge.md` section 7 (achievements: SetAchievement + StoreStats; cloud
   slots stay NULL; graceful failure without Steam; test with the stub in `.scratch/steam-research/`).
4. **M10**: Launcher features against the real game, with Launcher snapshots.
5. **M11**: a from-scratch `build.sh` run in a clean output folder (check disk first).
6. **M12**: README with status and steps, final publish.
7. **M8 backlog, timeboxed batches**: `MC_MENU_INPUT_ACTION` (fires or is not counted), Epiphany's characters
   on the wheel, Browser Build rebuild and run with the new hooks, then EID and StageAPI structures.
