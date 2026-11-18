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

Status table with evidence: top of `PROGRESS.md`. Uncommitted work at any moment: `git status`.

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
  (only the touched lifted files recompile). Recipe: `layer-gaps.md` section 5.
- The Launcher: `native/mac/launcher/build.sh <out dir>`; tests `native/mac/launcher/Tests/run.sh <scratch dir>`.
- The whole app: `scripts/mac/build.sh --game <game>` (`build.md`). Stages 1-8 (Steamless to
  `build_native`) are adopted from existing outputs and skipped; 9-14 (Launcher, Layer, bundle, dylibs,
  config, sign) run. A from-scratch run of 1-8 has never been done.

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
(`launcher.md`, "Game side"). A working runner is `.scratch/m8/run.sh <support dir> <log>`: it writes the
JSON (support, mods, saves, gate log, modscan, layer, segs, bundle) and runs the game under `perl alarm`.
Env knobs: `FRAMES`, `SCRIPT`, `SHOT`, `DBG=true` (debug console), `VAN=true` (Vanilla Launch),
`ISAAC_RGON_LAB=<lab file>`. To fill a support folder with the 95 Workshop mods, run the Launcher's
pre-launch (`.scratch/verify67/prelaunch.bin`) with `REPENTANCE_MAC_SUPPORT_DIR=<support>`,
`REPENTANCE_MAC_WORKSHOP_DIR=/nonexistent`, `REPENTANCE_MAC_WORKSHOP_FALLBACK=<workshop>`,
`REPENTANCE_MAC_STEAM_USERDATA=<empty dir>`.

These runners live in `.scratch/` (untracked). Promoting `run.sh`, `fresh.sh` and `lab_trapdoor.lua` into
`scripts/mac/` is an open task.

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
- If `gamestate1.dat` exists, Enter picks Continue, not New Run. `.scratch/m8/fresh.sh <support>` moves it aside.
- **Debug console**: grave (`` ` ``) opens it during a run (not on the title screen); **Escape closes it**.
  Pressing grave again only types a backtick, and every later key then goes into the console as text.
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
- **Labs** are test-only Lua files in `scripts/recomp/rgon/lab_*.lua`, never packed, loaded only with
  `ISAAC_RGON_LAB`: `lab_names` (dumps implemented names: the Layer measurement), `lab_trapdoor`,
  `lab_store` (Layer save), `lab_unlock` (mod achievements), `lab_scan`/`lab_vp`/`lab_menu` (offset probes),
  `lab_cb` (callback firing), `lab_night`.

**Open bug, Lua GC crash (2026-11-18)**: one windowed run with `lab_unlock_all` died with SIGBUS in
`sweeplist` <- `luaC_step` <- `lua_gc` <- `isaac_lua_gc` <- `sub_008657c0`, about 50 frames after the unlock,
reading `0x0000000f00000052`: a GC object's `next` pointer was `0x0000000f00000049` (`marked` is at +9), i.e. two
32-bit guest values over a native 64-bit Lua header. The same lab on the play save did not crash. Treat it as guest
memory corruption into the native Lua heap, not memory pressure.

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

**M9 Steam bridge recipe** (design and verifier steps: `steam-bridge.md` section 8). Off by default unless a
`libsteam_api.dylib` sits in `<exe dir>/../Frameworks`, next to the exe, or at `ISAAC_STEAM_DYLIB`; with none, the
log shows one `[mac][steam] disabled: ...` line and the game behaves as before. Test without Steam against
`.scratch/steam-research/libsteam_api_stub.dylib` (rebuild: `clang -arch arm64 -dynamiclib -install_name
@rpath/libsteam_api.dylib stub_steam_api.c -o libsteam_api_stub.dylib`):
`ISAAC_STEAM_DYLIB=$PWD/.scratch/steam-research/libsteam_api_stub.dylib ISAAC_STEAM_SELFTEST=1 ISAAC_MAC_RUN_FRAMES=600
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
2. **Make the app playable end to end** (user value first): `mac_gl.c` loads ANGLE from
   `<exe dir>/../Frameworks` when present; rebuild the app with `build.sh` from the current game; Play from
   the app reaches a room with mods; double-click from Finder works. Promote `run.sh`/`fresh.sh`/the lab into
   `scripts/mac/`.
3. **M9 Steam bridge** per `steam-bridge.md` section 7 (achievements: SetAchievement + StoreStats; cloud
   slots stay NULL; graceful failure without Steam; test with the stub in `.scratch/steam-research/`).
4. **M10**: Launcher features against the real game, with Launcher snapshots.
5. **M11**: a from-scratch `build.sh` run in a clean output folder (check disk first).
6. **M12**: README with status and steps, final publish.
7. **M8 backlog, timeboxed batches**: `MC_MENU_INPUT_ACTION` (fires or is not counted), Epiphany's characters
   on the wheel, Browser Build rebuild and run with the new hooks, then EID and StageAPI structures.
