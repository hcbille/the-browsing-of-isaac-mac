# Building the app: `scripts/mac/build.sh`

One command turns a Windows game folder into `Repentance+.app` (Launcher as the main program, the
game as a second program, ANGLE and SDL3 bundled, ad-hoc signed). It is resumable: every stage records
a stamp and skips itself when its inputs and output are current.

```sh
scripts/mac/build.sh --game <game dir> [--out <dir>] [--jobs N] [--force-stage X]... [--no-seed] [--dry-run] [--list]
```

| Option | Meaning |
| --- | --- |
| `--game <dir>` | The Windows game folder (`isaac-ng.exe`, `resources/`). Never written to. |
| `--out <dir>` | Where `Repentance+.app` goes (default `output/mac-app`). Working files go to `<out>/work`, stamps of the app stages to `<out>/.stamps`. |
| `--jobs N` | Parallelism of the lift and the C compile (default 3; the machine has 8 GB of RAM). |
| `--force-stage X` | Run stage X although it is current (repeatable, `all` = every stage). Later stages rerun by themselves. |
| `--no-seed` | Do not adopt existing outputs that have no stamp (see below). |
| `--dry-run` | Say what each stage would do, run nothing. |

Run it from a shell that has the toolchain (or let it source one, see "Environment"). A first run from
nothing takes hours (the Ghidra export alone is about 50 minutes); a rerun with nothing changed takes
seconds.

## Stages

Commands run from the repository root; `$PY` is `python` (else `python3`) of the toolchain.

| # | Stage | What it runs | Output that must exist |
| --- | --- | --- | --- |
| 1 | `steamless` | Copies `<game>/isaac-ng.exe` to a temp folder, `mono Steamless.CLI.exe --quiet` there (with `MONO_PATH=<steamless>/Plugins`), then `$PY scripts/recomp/prepare-input.py --input <unpacked>` | `tools/isaac-ng.unpacked.exe` with SHA-256 `5129df72...` |
| 2 | `ghidra` | `node scripts/recomp/export-all.mjs --stage all`; `$PY scripts/recomp/census/run_all.py`; `$PY scripts/decomp/tools/build-pe-index.py` | `output/recomp/export/{decompile-complete.marker,functions.jsonl}`, `output/recomp/census/imports.json`, `output/decomp/<hash>/` |
| 3 | `tables` | `$PY scripts/recomp/host/boot_tables.py`; `$PY scripts/recomp/host/gen_shims.py` | `output/recomp/host/{shim-table,boot-tables}.json`, `scripts/recomp/host/generated/shim_table.c` |
| 4 | `lift` | `prepare_starts.py`; `orphan_starts.py --write`; `lift_parallel.py --jobs N --exe ... --out output/recomp/lift/gu ...` (the arguments of `docs/DEVELOPING.md`); `patch_reentry.py --dir output/recomp/lift/gu --exe tools/isaac-ng.unpacked.exe` | `output/recomp/lift/gu/{lifted_000.c,stats.json}`, `output/recomp/lift/starts_union.txt` |
| 5 | `build_boot` | `$PY scripts/recomp/lift/build_boot.py --web --fast --jobs N` (applies the lift patches to the lifted tree in place, writes `stubs.c` and `dispatch_tbl.c`, builds the Browser Build's module; needs Emscripten) | `output/recomp/lift/gu/{dispatch_tbl.c,stubs.c}` |
| 6 | `bundle` | `$PY scripts/recomp/assets/bundle.py build <game> .scratch/game-bundle --strict` | `.scratch/game-bundle/{.bundle.json,resources/}` |
| 7 | `memimage` | `$PY scripts/recomp/host/memimage.py`; `$PY scripts/recomp/host/verify_memimage.py` | `output/recomp/host/isaac.segs.bin` |
| 8 | `build_native` | Unpacks Lua 5.3.3 into `tools/` if missing (downloads the tarball from lua.org if absent, SHA-256 checked); `$PY scripts/mac/build_native.py --jobs N --out <native out>` | `<native out>/isaac-mac` |
| 9 | `launcher` | `native/mac/launcher/build.sh <out>/work` | `<out>/work/Launcher` |
| 10 | `layer` | `$PY scripts/mac/stage_layer.py <out>/work/layer`; `$PY native/mac/launcher/gen_layer_info.py --enums <REPENTOGON enums_ex.lua> --out <out>/work/layer-info.json` | `<out>/work/{layer/,layer-info.json}` |
| 11 | `app` | Builds `Repentance+.app` from scratch: `Info.plist`, `MacOS/<Launcher>`, `MacOS/isaac-mac`, `Resources/{isaac.segs.bin, game-bundle/, layer/, layer-info.json, AppIcon.icns}`. Big data is cloned with `cp -c` (APFS), so it costs no disk. The icon is a plain placeholder drawn by `scripts/mac/make_icon.py` (no game art) | the app folder |
| 12 | `dylibs` | Compiles the vendored NotchFill hook (`native/mac/third_party/notchfill/NotchFillHook.m`, MIT, unchanged; `clang -arch arm64 -mmacosx-version-min=11.0 -O2 -dynamiclib -fobjc-arc -framework AppKit`) to `Frameworks/libNotchFillHook.dylib` and copies its `LICENSE` to `Resources/licenses/NotchFill-LICENSE.txt`; copies ANGLE (`libEGL`, `libGLESv2`), SDL3 and, if found, `libsteam_api.dylib` into `Contents/Frameworks`; sets `@rpath/<name>` install names; rewrites the game's non-system dependencies to `@rpath/...`; replaces every `LC_RPATH` of the game with `@executable_path/../Frameworks` (of the libraries: `@loader_path`); fails if any non-system, non-`@rpath` dependency remains | `Contents/Frameworks/*.dylib`, `Resources/licenses/NotchFill-LICENSE.txt` |
| 13 | `config` | Writes `Contents/Resources/sync-config.json` with `workshop_fallback` = `<game>/steamapps/workshop/content/250900` and `save_search` = `<game>` | the file |
| 14 | `sign` | `codesign --force --sign -` on each dylib, then `isaac-mac`, then the Launcher program, then the app; `codesign --verify --deep --strict` | a valid signature |

`libsteam_api.dylib` is Valve's and is never committed: it is copied from `STEAM_API_DYLIB` or the local
Steam client's bundle (`~/Library/Application Support/Steam/Steam.AppBundle/.../libsteam_api.dylib`,
universal). When neither exists the stage prints a warning and the app is built without it.

Not run by the build: `build_selftest.py` (the Browser Build's host self test) and the Browser Build's
`ship.py` steps.

## Skip logic

Each stage has an inputs fingerprint (content hashes of its scripts and sources, the game folder's file
names, sizes and times for the bundle, and the stamp of every stage it depends on) and an output check.
The stamp file holds `<fingerprint> <run id>`. A stage is **skipped** when the stamp matches and the output
exists. It **runs** when the output is missing or the fingerprint changed (a changed script, a changed game
file, or an upstream stage that ran again; the run id makes that chain work). Stamps of stages 1-8 live in
`output/mac/stamps` (override `ISAAC_STAMP_DIR`), those of stages 9-14 in `<out>/.stamps`, so a clean `--out`
rebuilds only the cheap app stages.

**Seeding.** When a stage has no stamp but its output exists, the stamp is written from the current inputs
and the stage is skipped ("seeded"). That is how an existing tree (hand-run earlier) is adopted without a
rerun. It trusts the output; use `--no-seed` or `--force-stage` to rebuild, and note that sources that were
mid-edit when the stamp was seeded are counted as current. Stages 9-14 are never seeded.

## Environment

Nothing machine specific is built in. Everything has a discovery default and an override:

| Variable | Default |
| --- | --- |
| `ISAAC_TOOLCHAIN_ENV` | A shell file sourced first (Python with pypcode, JDK, Ghidra, Emscripten). Default: `<repo>/../toolchain/env.sh`, then `<repo>/toolchain/env.sh`; none found: the current environment is used |
| `ISAAC_TOOLCHAIN_DIR` | Folder holding `steamless/` and `angle/`. Default: the env file's folder, else `<repo>/../toolchain` |
| `STEAMLESS_DIR` | `$ISAAC_TOOLCHAIN_DIR/steamless` (`Steamless.CLI.exe` and `Plugins/`) |
| `ANGLE_DIR` | `$ISAAC_TOOLCHAIN_DIR/angle` (ANGLE with the Metal backend, `@rpath` install names) |
| `SDL_PREFIX` | `brew --prefix sdl3` |
| `STEAM_API_DYLIB` | The local Steam client's copy, if any |
| `ISAAC_STAMP_DIR`, `ISAAC_NATIVE_OUT` | `output/mac/stamps`, `output/mac` (where `build_native.py` writes `isaac-mac`; useful for tests) |

`GHIDRA_HOME`, `EMSDK` and `KHRONOS_DIR` are read by the pipeline scripts as before.

## App layout and what the programs expect

```
Repentance+.app/Contents/
  Info.plist
  MacOS/Repentance+          the Launcher (CFBundleExecutable)
  MacOS/isaac-mac            the game
  Frameworks/                libEGL, libGLESv2, libSDL3.0, libsteam_api (when found), libNotchFillHook (optional, Launcher option)
  Resources/isaac.segs.bin   game default: <exe dir>/../Resources/isaac.segs.bin
  Resources/game-bundle/     game default
  Resources/layer/           game default (--layer-dir)
  Resources/layer-info.json  the Launcher's Options tab
  Resources/sync-config.json this machine's Workshop fallback and save search
  Resources/licenses/        NotchFill-LICENSE.txt (the Layer's licence is in Resources/layer/)
  Resources/AppIcon.icns
```

The Launcher starts `Contents/MacOS/isaac-mac` next to itself (`Paths.gameExecutable` in `Model.swift`); the
game takes its defaults from `<exe dir>/../Resources/`. Both match this layout, so Steam's launch option is
`"<path>/Repentance+.app/Contents/MacOS/Repentance+" %command%`.

### Two follow-ups outside this script's reach

1. **`sync-config.json`.** The Launcher reads only `<support>/sync-config.json`
   (`SyncDirs` in `SyncSupport.swift`). The build writes the file into `Contents/Resources/`, so the Launcher
   must also read `Bundle.main` `Contents/Resources/sync-config.json` (merging both) before the Workshop
   fallback and save search work from the app.
2. **ANGLE path.** `build_native.py` compiles the ANGLE folder it was given into the game
   (`-DISAAC_ANGLE_DIR`), and `mac_gl.c` uses it as the library path. In the app the libraries are in
   `Contents/Frameworks`, so `mac_gl.c` should prefer `<exe dir>/../Frameworks` when that folder exists (and
   the compiled-in path is then only a development fallback). Until then the game's windowed run in the app
   still finds the toolchain's ANGLE on the build machine only. The binary's `LC_RPATH` and install names
   do not depend on the toolchain.

## Verifying a build

```sh
otool -L Repentance+.app/Contents/MacOS/isaac-mac            # system libs and @rpath only
codesign --verify --deep --strict Repentance+.app
open -n Repentance+.app                                      # the Launcher window
```

For the Launcher without a screen capture, `REPENTANCE_MAC_LAUNCHER_SNAPSHOT=<png>` (see `launcher.md`)
renders its window from inside the app; pass it with `open --env`.
