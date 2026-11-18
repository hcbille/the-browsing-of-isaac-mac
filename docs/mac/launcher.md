# The Launcher

SwiftUI, arm64, macOS 13+. It is the main program of `Repentance+.app`
(`Contents/MacOS/Repentance+`); the Mac Build's game is a second program in the
same bundle, `Contents/MacOS/isaac-mac`. Source: `native/mac/launcher/`.

```sh
native/mac/launcher/build.sh <out dir>          # <out dir>/Launcher (bare executable)
native/mac/launcher/build.sh <out dir> --app    # also <out dir>/Repentance+.app (ad-hoc signed)
```

Plain `swiftc`, no packages, no Xcode project. The Launcher stays alive while the
game runs (Steam tracks the process it started) and quits when the game exits.

## Behaviour

- Opens on **Play**. "Skip the Launcher next time" (`skip_launcher`): the next
  start launches the game at once with the saved options, unless **Option** is
  held at start, which brings the Launcher back.
- **Mods**: one row per folder in the mods folder, in the game's load order
  (display only; the list is sorted by name like the game's menu, each row shows its real load-order number), a switch (on/off = `disable.it` absent/present in the mod
  folder), the Shown / Hidden state from the gate log, what each hidden mod is
  missing, "No longer subscribed" with **Remove...** (asks first, then moves the
  folder to the Trash; never a hard delete).
- **Vanilla Launch warning**: shown on Play, Mods and Options when Vanilla Launch
  is on and an enabled mod is Epiphany (folder or metadata name contains
  "epiphany"; Epiphany is the Hard Dependent).
- **Options**: Vanilla Launch (Layer off), Debug console, Log window, Lua debug
  (off by default; warning that it gives mods access to files and programs). The
  Layer's version and REPENTOGON coverage come from `layer-info.json`. No
  auto-update. **Fill the notch area in full screen (NotchFill)** (`notch_fill`, default off): "Uses the strip
  beside the notch in full screen. Loads the NotchFill library into the game." Offered only when a screen has a
  notch (`NSScreen.auxiliaryTopLeftArea` non-empty, macOS 12+; it stays visible while the setting is on, so it can
  be turned off); otherwise hidden.
- **Play** writes `launch-settings.json`, starts the game with the arguments
  below, and always copies the game's stdout and stderr to `logs/game.log`; with
  the Log window option it also tails them in a "Game Log" window.
- **Startup window** (`StartupPanel.swift`): from Play until the game's first
  frame the Launcher's own window is hidden and a small floating panel shows a
  spinner, the elapsed time, the current startup step and a "Show log" button.
  The game reports its steps on stdout:
  - `[mac][boot] status: <text>`: the current step ("Loading the game",
    "Reading packed archives", "Reading mods (N files)", "Starting the game",
    "Reading mod: <folder>"). A phase change always prints; the rest at most
    about four a second.
  - `[mac][boot] ready`: printed once, at the first presented frame. The panel
    closes.
  If the game exits before `ready`, the Launcher comes back with "The game
  exited during startup with status N" and a "Show log" button, and does not
  quit. An older game that never prints a `[mac][boot]` line is treated as
  ready after 240 s.

## Decisions

- **App folder name: `Repentance+ Mac`** (no earlier doc named one). The support
  folder is `~/Library/Application Support/Repentance+ Mac/`.
- **Load order.** The game loads mods in the order the Host's `readdir` lists the folders of the mods
  folder (`mac_seed_mods` in `mac_fs.c`), i.e. the raw, unsorted directory order of the volume (on APFS a
  hash of the name, neither folder name nor display name). Evidence: the 95 `LOADED MOD` lines of a real run
  (`.scratch/verify67/g-run4.log`) are exactly `ls -f` of that run's mods folder, lower-cased; a test
  compares them (`REPENTANCE_MAC_LOADED_MOD_LOG` / `_DIR`). The Launcher reads the folder with `readdir`
  too (`ModStore.gameLoadOrder`: dot-folders, files and a folder named `repentogon` are not mods) and
  shows that number on each row; the Mods tab can sort "By load order". The list itself is sorted like
  the in-game MODS menu, by display name, case-insensitive. The order is display-only and cannot be changed.
- **Window.** Options > Window: "Whole-number scaling (black bars)" (default) or "Fill the window",
  stored as `scale_mode` (`"integer"` | `"fill"`) in `launcher-settings.json` and written to
  `launch-settings.json` at each Play. The game side must read `scale_mode`. No fullscreen-at-start key is
  defined, so the Launcher has none.
- **NotchFill** (`native/mac/third_party/notchfill/`, MIT, Copyright (c) 2026 Dmitrii Tikhonov, vendored
  unchanged from https://github.com/squallsama/NotchFill at `7fd66794f5b96098bf76fa8d1b8200d363f4df86`; credit
  also in the app at `Contents/Resources/licenses/NotchFill-LICENSE.txt`). The build compiles it to
  `Contents/Frameworks/libNotchFillHook.dylib`. With the option on, `GameRunner` starts the game with
  `DYLD_INSERT_LIBRARIES=<that file>` (found next to the game program, else `Bundle.main.privateFrameworksPath`);
  with it off the variable is removed from the game's environment, never inherited. A missing file is logged
  (`launcher.log`, Game Log) and the game starts without it. Why injection works: the game and the library are
  ad-hoc signed without hardened runtime, so dyld honours `DYLD_INSERT_LIBRARIES`. The hook only changes macOS
  native full screen (Cmd+Ctrl+F, the green button; the game's own F key / Options full screen if it uses the
  native mode): it stretches the full-screen window up beside the notch, raises it above the menu bar and reports a
  top safe-area inset of 0. Windowed play is unchanged. It logs `NotchFillHook: loaded` (stderr, so in
  `game.log`). `NOTCHFILL_DISABLE=1` in the environment loads it without any change. The hook also reads NotchFill's
  own preferences (`dev.notchfill.NotchFill`) if that app is installed. The option is applied even on a Mac
  without a notch (the hook does nothing there), so a saved `true` is honoured.
- A Workshop update re-copies a mod and keeps its `disable.it`; the Launcher
  never touches anything but `disable.it` and the Trash move.

## Files and paths

All under the support folder unless noted. Each can be overridden by an
environment variable, for tests.

| What | Default path | Override | Written by |
| --- | --- | --- | --- |
| Support folder | `~/Library/Application Support/Repentance+ Mac/` | `REPENTANCE_MAC_SUPPORT_DIR` | |
| Mods | `<support>/mods/<folder>/` | `REPENTANCE_MAC_MODS_DIR` | Launcher (Workshop sync) |
| Launcher settings | `<support>/launcher-settings.json` | | Launcher |
| Launch settings | `<support>/launch-settings.json` | | Launcher, at each Play |
| Gate log | `<support>/logs/layer-gate.json` | `REPENTANCE_MAC_GATE_LOG` | game, at each launch |
| Mod sync state | `<support>/mod-sync.json` | | Launcher (Workshop sync) |
| Mod scan | `<support>/modscan.lua` | | Launcher (pre-launch, after the sync) |
| Mod scan cache | `<support>/modscan-cache.json` | | Launcher |
| Mac saves | `<support>/saves/` | `REPENTANCE_MAC_SAVE_DIR` | game; Launcher (import) |
| Save import marker | `<support>/save-import.json` | | Launcher |
| Launcher log | `<support>/logs/launcher.log` | | Launcher (every pre-launch decision) |
| Local source folders | `<support>/sync-config.json` | | the user / the build (never committed) |
| Replaced mod copies | `<support>/mods-replaced/<folder>/` | | Launcher (previous copy of an updated mod) |
| Game log | `<support>/logs/game.log` | | Launcher (copy of stdout+stderr) |
| Layer info | `Repentance+.app/Contents/Resources/layer-info.json` | `REPENTANCE_MAC_LAYER_INFO` | the build |
| Game program | `Repentance+.app/Contents/MacOS/isaac-mac` | `REPENTANCE_MAC_GAME_EXE` | the build |

Test-only: `REPENTANCE_MAC_LAUNCHER_TAB=play|mods|options`, `REPENTANCE_MAC_LAUNCHER_SORT=load`,
`REPENTANCE_MAC_LAUNCHER_FORCE=1` (act as if Option were held), `REPENTANCE_MAC_LAUNCHER_FORCE_NOTCH=1` (show the NotchFill option on a Mac without a notch),
`REPENTANCE_MAC_LAUNCHER_SNAPSHOT=<png>` (+ `_SNAPSHOT_DELAY`, `_EXIT=1`) renders
every visible window from inside the app (no screen-recording permission needed).
These are environment variables on purpose: unknown command-line arguments stopped
AppKit from opening the window in testing.

### Mods folder, `disable.it`

A mod is a folder. Off = a file named `disable.it` (any content, empty written by
the Launcher) in it; on = absent. Display name and version come from the folder's
`metadata.xml` (`<name>`, `<version>`), falling back to the folder name.

### `mod-sync.json` (Launcher's Workshop sync writes and reads)

```json
{ "version": 1, "unsubscribed": ["old-pack", "some folder name"] }
```

Folder names (compared case-insensitively) of mods in `mods/` whose Workshop
subscription is gone. Missing file = none. The sync also writes `items`
(`<id>: {folder, version, fingerprint, synced_at}`), `source`, `synced_at`, and keeps other keys.

### `logs/layer-gate.json` (game writes at each launch, Launcher reads)

The gate's decisions, one row per mod that names REPENTOGON in its code
(`REPENTOGON.Extras.Gate.mods` in `scripts/recomp/rgon/rgon.lua`, which also
logs `gate: <folder> is shown|not shown REPENTOGON: <why> (N name(s) of
REPENTOGON's used, M missing: ...)`). Same fields, lower-cased folder as key:

```json
{
  "version": 1,
  "layer_on": true,
  "written_at": "2026-11-18T10:00:00Z",
  "mods": {
    "epiphany": { "shown": true,  "why": "main.lua quits without REPENTOGON",
                  "used": 212, "missing": ["Isaac.ReworkCollectible", "MC_PRE_ROOM_ENTITY_SPAWN_EX"] },
    "zeta-hud": { "shown": false, "why": "has if REPENTOGON then return end",
                  "used": 0, "missing": [] }
  }
}
```

- `shown` = Shown (the Layer lets the mod see `REPENTOGON`), `why` = gate_rule's
  reason, `used` = count of names that are REPENTOGON's, `missing` = the full list
  (the Launcher truncates for display). A shown mod with `missing` entries runs on
  stand-ins; the Launcher says so.
- A mod absent from `mods` is shown as "Does not use REPENTOGON". No file at all
  is shown as "Not scanned yet". Under Vanilla Launch the game may write
  `"layer_on": false` and an empty `mods`.

### `layer-info.json` (the build writes into the app's Resources)

```json
{
  "version": 1,
  "layer_version": "1.1.2",
  "game_version": "1.9.7.17",
  "implemented_names": 1043,
  "unsupported_names": 37,
  "known_names": 1495,
  "implemented_callbacks": 301,
  "known_callbacks": 365,
  "built_at": "2026-11-18T09:00:00Z"
}
```

`layer_version` is `VERSION` in `rgon.lua`; `implemented_names` is the count of
`REPENTOGON.Implemented`; `unsupported_names` of `REPENTOGON.Unsupported`;
`known_names` / `known_callbacks` the size of the gate's name set
(`gate_names.lua`, `enums_ex.lua` callbacks). Coverage shown =
`implemented_names / known_names`. All fields optional. (The fixture's numbers
are invented.)

### `launcher-settings.json` (Launcher's own, persisted)

```json
{ "skip_launcher": false, "vanilla_launch": false, "debug_console": false,
  "log_window": false, "lua_debug": false, "scale_mode": "integer", "notch_fill": false }
```

### Launch contract (what the game must implement)

The Launcher runs `isaac-mac` with the environment plus
`REPENTANCE_MAC_LAUNCH_SETTINGS=<path>` and these arguments, in addition to
whatever the game needs itself (the game finds its own resources relative to its
bundle; the Launcher passes none):

```
isaac-mac --launch-settings <support>/launch-settings.json [--vanilla] [--debug-console] [--lua-debug]
```

- `--vanilla`: Vanilla Launch. The Layer is not loaded, no mod sees
  `REPENTOGON`. (The game should then write `layer-gate.json` with
  `"layer_on": false`.)
- `--debug-console`: enable the game's debug console.
- `--lua-debug`: give mods the Lua `io`/`os`/`debug` libraries, as `--luadebug`
  does on Windows. Off by default.
- The log window needs nothing from the game: the Launcher reads its stdout and
  stderr (the game's log should go to stdout/stderr).

`launch-settings.json` (rewritten at each Play; authoritative, the flags are a
convenience):

```json
{
  "version": 1,
  "launched_at": "2026-11-18T01:41:16Z",
  "vanilla_launch": false, "debug_console": false, "log_window": false, "lua_debug": false,
  "scale_mode": "integer", "notch_fill": false,
  "support_dir": "<support>", "mods_dir": "<mods>",
  "gate_log_path": "<support>/logs/layer-gate.json",
  "game_log_path": "<support>/logs/game.log",
  "modscan_path": "<support>/modscan.lua"
}
```

The game must read the mods folder from `mods_dir` and write the gate log to
`gate_log_path` when present (so test overrides reach it), and treat
`disable.it` as the only on/off state.

### Game side (implemented in `native/mac/host/mac_launch.c`, `mac_fs.c`, `mac_gate.c`)

Flags (`isaac-mac`, all optional): `--launch-settings <json>`, `--vanilla`, `--debug-console`, `--lua-debug`,
`--scale integer|fill`, `--window WxH`, `--segs <file>`, `--bundle <dir>`, `--layer-dir <dir>`, `--save-dir <dir>`, `--mods-dir <dir>`, `--modscan <file>`,
`--gate-log <file>`, `--stage boot|main`, `--headless` (no window, fake GL; tests). The positional dev form
`isaac-mac <segs> <bundle> [boot|main] [save dir]` still works.

- **Precedence**, for every path: command-line flag, then `launch-settings.json`, then the `REPENTANCE_MAC_*`
  environment variable (`REPENTANCE_MAC_LAUNCH_SETTINGS` names the settings file), then the default. Defaults for
  the segs file, bundle and Layer: `<exe dir>/../Resources/{isaac.segs.bin,game-bundle,layer}`. The JSON may carry
  `segs_path`, `bundle_dir`, `layer_dir` and `save_dir` besides the keys above. The three option flags can only turn an
  option on; the JSON says what is off. All directories are made absolute at once (the game chdirs into the bundle).
- **Vanilla Launch**: the Layer is not loaded (`ISAAC_RGON=0`), its files are not served, no `[isaac][rgon]` lines
  appear, and `layer-gate.json` is written as `{"layer_on": false, "reason": "vanilla launch", "mods": {}}`. Mods still
  load. If the Layer folder is missing the game launches the same way and the gate log says `"reason": "layer not installed"`.
- **`--lua-debug`**: passes `--luadebug` in the Guest's argv, so mods get `io`, `os` and `debug`
  (`type(io)` is `"table"`; without the flag it is `nil`).
- **Window scaling (M5, `mac_window.c`)**: launch-settings key `"scale_mode"`: `"integer"` (default; the largest
  whole multiple of the game's 960x540 that fits the window, nearest filter, black bars) or `"fill"` (largest
  aspect-preserving size, linear filter, bars on the short axis). Flag `--scale integer|fill` overrides it, then the
  environment (`REPENTANCE_MAC_SCALE`, `ISAAC_MAC_SCALE`). Read at launch, so a Launcher setting takes effect at the
  next Play. `--window WxH` / `ISAAC_MAC_WINDOW=WxH` sets the initial window size in points (default 960x540, the
  window is resizable). Fullscreen is the game's own option (Options menu, the F key) or Cmd+Ctrl+F; the Launcher
  has nothing to pass.
- **`--debug-console`**: the game sets Options.EnableDebugConsole at the first frame, both ways, overriding
  `options.ini` (log: `debug console on|off by the launch settings`). The grave key opens the console during a run.
- **Layer**: built into the app, served by the Host at the virtual path `mods/repentogon/`, never listed as a mod,
  loaded before any mod code. `modscan.lua` is served at `mods/repentogon/modscan.lua` from `modscan_path`.
- **Mods and `disable.it`**: mods are read from `mods_dir` (copied into the game's `mods/` view). The game's MODS menu
  toggle writes or deletes `disable.it` in `mods_dir`, and that is the only thing the game ever writes there.
  **Seeding rule:** an enabled mod is shown to the game in both mod roots (`mods/` and
  `documents/my games/binding of isaac repentance+/mods/`). A mod with `disable.it` is shown as `metadata.xml` +
  `disable.it` only, in the `mods/` root only, because the game deletes `disable.it` in the Documents-root copy. An off
  mod appears greyed in the MODS menu, runs no Lua and none of its `content/` is served; toggling it on in the game
  deletes `disable.it` and it runs from the next launch (not mid-session).
- **Gate log**: written to `gate_log_path` once, when the Layer's gate has decided (at the first mod chunk), with one row
  per mod that names REPENTOGON, plus `"scanned": true` when `modscan.lua` was read. Under Vanilla Launch it is written
  at start.
- **Saves**: under `save_dir` the game keeps `documents/my games/binding of isaac repentance+/`; files are written
  atomically (temp file, fsync, rename), and the previous contents are restored at the next launch.
- **Test hooks**: `ISAAC_MAC_RUN_FRAMES=N` (stop after N frames with a guard check and exit 0), `ISAAC_MAC_SHOT=F:path`
  (screenshot at presented frame F, `@F` relative to a script anchor), `ISAAC_MAC_SCRIPT` (scripted keys, see
  `mac_script.c`).

## Save import and Workshop sync (run by the Launcher before every game start)

Decision: both live in the Launcher (Swift), not the game. `PreLaunch.run()` runs on a background task
when Play is pressed (also on the skip-the-Launcher path), before `isaac-mac` starts; every decision is
appended to `<support>/logs/launcher.log` and echoed to the Game Log window. Nothing here ever throws or
blocks the launch.

### Save import (ADR 0002)

- The Mac save folder is `<support>/saves`; `launch-settings.json` gains `"save_dir"` and the game must
  use it as the save dir (4th argument of `isaac-mac`). Inside it the game's tree is
  `documents/my games/binding of isaac repentance+/` with `persistentgamedata1..3.dat`.
- Rules: marker `<support>/save-import.json` present -> nothing. Any `persistentgamedata*`,
  `gamestate*` or `rerunstate*` `.dat` already in the Mac tree -> never touched, marker written with
  `"status": "skipped-existing-mac-save"` (so a cloud save that turns up later is never imported over
  fresh Mac progress). Otherwise the best source is imported once: copy to a temp name, verify sha256,
  move into place (never over an existing file); marker `{status: "imported", imported_at, source,
  origin, files: [{name, from, sha256, bytes}]}`. No source -> nothing, no marker (retried next launch,
  until the game creates a save).
- Sources, read only: Mac Steam `userdata/<id>/250900/remote` (override `REPENTANCE_MAC_STEAM_USERDATA`),
  then a bounded walk (depth 7, skipping `workshop`) of the folders in `REPENTANCE_MAC_SAVE_SEARCH`
  (colon separated) and `sync-config.json` `save_search`. Steam Cloud wins, then the newest files.
  `rep+persistentgamedataN.dat` is accepted and stored under the game's plain name.
- Not imported: `gamestate*.dat` / `rep+rerunstate*.dat` (a run in progress) and `options.ini`.
- Never written: anything outside `<support>`.

### Workshop sync (rules from the build brief)

- Source: `REPENTANCE_MAC_WORKSHOP_DIR`, default Mac Steam's `steamapps/workshop/content/250900`; if it
  has no mods, the first of `REPENTANCE_MAC_WORKSHOP_FALLBACK` (colon separated) and
  `sync-config.json` `workshop_fallback` that has. The machine-specific fallback (for example the
  Windows depot's Workshop folder) is therefore never in the repo; the build or the user writes it:
  `{"workshop_fallback": ["<game dir>/steamapps/workshop/content/250900"], "save_search": ["<game dir>"]}`.
  The build also writes `Repentance+.app/Contents/Resources/sync-config.json` (same keys; override
  `REPENTANCE_MAC_BUNDLED_SYNC_CONFIG`). Precedence: environment variables, then `<support>/sync-config.json`,
  then the bundled file, then the built-in defaults; the lists are concatenated in that order, duplicates dropped.
  No source with mods -> nothing is changed (a missing Steam folder never marks mods unsubscribed).
- Target folder: `mods/<directory>_<id>` from the item's `metadata.xml` `<directory>` (what the PC game
  names Workshop copies; 76 of the 95 folders match an existing PC mods folder exactly, the rest are
  newer than that folder). Items are numeric folders containing `metadata.xml`.
- New item: copied (APFS clone via `FileManager.copyItem`, through `<support>/.sync-tmp`). Changed item
  (a `version` or per-file path/size/mtime fingerprint differs from `mod-sync.json`): re-copied, its
  `disable.it` state kept, the old copy moved to `mods-replaced/` (one generation, outside `mods/`; nothing
  is deleted). Item gone from the source: folder kept, listed in `unsubscribed` (recomputed each sync).
  A `mods/` folder not made by the sync is left alone unless its `<id>` equals the item's.
- `[BETA] REPENTOGON: Isaac Script Extender` (3127536138, folder `repentogon_3127536138`) gets
  `disable.it` on first copy only.
- Free space: needs the copied bytes plus 1 GB free on the volume or nothing is copied (logged).
- The Workshop folder is only read.

### Mod scan (`ModScan.swift`; M8)

What the Layer's gate needs to know about each installed mod (does its Lua name `REPENTOGON`, does `main.lua`
quit without it, which REPENTOGON-only names does it use) is read by the Launcher, not the game. It is a
faithful Swift port of `createModScan` / `stripLua` / `scanLua` in `scripts/recomp/web/mods.mjs` (round 122):
same Lua stripping, same rules, same output format. `PreLaunch.run()` runs it right after the Workshop sync
(so also on the skip-the-Launcher path), over the **enabled** folders of `<support>/mods` (a folder with
`disable.it` is not scanned; dot-folders and a folder named `repentogon` are left out, as the page leaves out
the Layer's own folder), and writes `<support>/modscan.lua`. The mod id in the file is the mods-folder name
(lower-cased and sanitised by the file writer, as in the Browser Build), which is the key the gate logs and
`layer-gate.json` uses.

**Contract with the Host.** The Layer does `B.load("modscan.lua")` from its own folder. In the Browser Build the
page seeds the generated file at the virtual paths
`c:/isaac/mods/repentogon/modscan.lua` and
`c:/isaac/documents/my games/binding of isaac repentance+/mods/repentogon/modscan.lua`
(`GUEST_ROOTS` + `RGON_LAYER_ID` + `MODSCAN_FILE` in `mods.mjs`). The Host must serve the bytes of
`<support>/modscan.lua` (also given as `modscan_path` in `launch-settings.json`) at the same
`mods/repentogon/modscan.lua` virtual path, i.e. whatever path form the Layer's `B.load("modscan.lua")` resolves to
in the Mac Build; if the file is missing the Layer logs `gate: no scan of the installed mods` and shows
REPENTOGON only to its own short list (nothing is hidden or shown wrongly, but every mod outside the list is hidden).
Under Vanilla Launch the file is still written and simply not read.

- **Cache.** `<support>/modscan-cache.json`: per mod folder, a fingerprint and the mod's row (or "does not name
  REPENTOGON"). The fingerprint is the Workshop sync's scheme (sha256 of every file's relative path, size and
  mtime, no file read) restricted to `.lua` files, so switching a mod on or off, or changing a texture, never
  rescans; any Lua change does. The cache is also dropped when `ModScan.scannerVersion` changes. An unchanged
  scan result does not rewrite `modscan.lua`.
- Log line (launcher.log): `mod scan: N enabled mod(s), K name REPENTOGON in their code, R read, C unchanged
  (cached), T ms; wrote <path>`.
- Timing on the 95 Workshop mods (1,207 `.lua` files): about 1.5 to 2 s cold, 0.3 to 0.5 s with the cache (the
  rest is walking the files for the fingerprint), against about 3 to 4 s for the JS on the same input.
- Differences from the JS, by design: it only reads `.lua` files of the folder itself (the page gets its
  file list from the mod store); ids come from folder names. Edge: `toLowerCase` of an id with non-ASCII
  letters could in theory differ from Swift's `lowercased()` (both end up as `_` in the file, only the order
  or a collision could differ).

### Tests

`native/mac/launcher/Tests/run.sh <scratch dir>` builds `Tests/main.swift` with the sync sources, `ModScan.swift`
and `Model.swift` (swiftc) and runs about 60 checks on fixtures: save present / absent / Mac save present (bytes
unchanged) / second launch; new, updated (disable.it kept), unsubscribed, resubscribed mod; checker mod
off by default and left on when the user turned it on; Steam folder preferred; own mods untouched; and the mod
scan: stripLua/scanLua cases, `modscan.lua` location and content, cache hits, a mod switched off/on, a changed mod.

**Golden test of the mod scan.** `run.sh` also builds `Tests/golden/main.swift` and compares it with
`Tests/golden_scan.mjs` (a driver over the real `createModScan` / `stripLua` / `scanLua` of `mods.mjs`; needs
`node`): the `modscan.lua` of both must be byte-identical, and so must a per-file dump (sha256 of `stripLua`'s
output, `says/quits/yields`, the names and defined names of every `.lua` file). It runs on
`Tests/fixtures/modscan/mods` (edge cases: comments, `--[==[` blocks, long strings holding `]]`, nested and
escaped quotes, unterminated strings, BOM, CRLF, invalid UTF-8, non-ASCII identifiers, Unicode spaces,
quit/yield/else/elseif/assert guards, a disabled mod, the Layer folder, odd folder names) and, when
`REPENTANCE_MAC_GOLDEN_WORKSHOP=<a Workshop content folder>` is set (read only; the path is never in the
source), on a real Workshop folder.

### `gen_layer_info.py` (the build writes `layer-info.json`)

`native/mac/launcher/gen_layer_info.py [--enums <enums_ex.lua>] [--lab-log <log>] --out <file>` computes
`layer-info.json` from the Layer's sources (`VERSION` in `rgon.lua`, `gate_names.lua`, the callback ids of
REPENTOGON's `enums_ex.lua`, also from env `REPENTOGON_ENUMS_EX`); it is wired into `build.sh` at M11. Without
`--lab-log` the implemented and unsupported counts are static estimates (a known name counts when it appears as
an identifier in `rgon.lua` / `api_*.lua`; the file says `"source": "static estimate from the Lua sources"`);
pass the log of a run with `ISAAC_RGON_LAB=lab_names.lua` for the exact numbers
(`REPENTOGON.Implemented` as the gate reads it). `known_callbacks` counts distinct ids of 1000 and up in
`enums_ex.lua` (381; REPENTOGON's README says 365). The extra `source` key is ignored by the Launcher.


## Verified

Built with `swiftc` (Swift 6.4, `-swift-version 5`) and run against the fake
fixture under `.scratch/launcher-fixture` (not committed): all tabs, toggle
writes/removes `disable.it`, Remove shows a confirmation and moves the folder to
the Trash, skip-the-Launcher starts the fake game directly and the Log window
tails stdout and stderr, the Launcher exits with the game. Not testable here:
holding Option at start (the code reads `NSEvent.modifierFlags`; test via
`REPENTANCE_MAC_LAUNCHER_FORCE=1`), and the real game side of every contract
above.
