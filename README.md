# the-porting-of-isaac

Work in progress: a native Apple Silicon build of *The Binding of Isaac:
Repentance+*, built on [chiikabu/the-browsing-of-isaac](https://github.com/chiikabu/the-browsing-of-isaac),
which statically recompiles the game's Windows executable to C.

That project compiles the result to WebAssembly for a browser. This repository
holds only what it takes to compile the same code natively for arm64 macOS:

- a patch to upstream's Host layer (`patches/host-native.patch`) so it builds
  with `-DISAAC_NATIVE=1`, reaching Guest memory through a base pointer
  (arm64 macOS reserves the low 4 GiB, so the browser build's identity
  mapping is impossible, see `docs/adr/0001`)
- the Mac Host: Guest memory, file I/O, window, input, audio, saves, mods and the
  Steam bridge (`native/mac/host`)
- the Launcher, a SwiftUI app (`native/mac/launcher`)
- the build: `scripts/mac/build.sh` for the whole app, `build_native.py` for the game
  binary alone, and `run.sh` for test runs (`scripts/mac`)
- NotchFill's hook source, unchanged (`native/mac/third_party/notchfill`)
- the design: `GLOSSARY.md`, `docs/adr`, `docs/mac`

## What is not here

No game files, no executable, no assets, no decompiled or recompiled game
code, and none of upstream's sources. You need your own copy of the game and
a checkout of upstream; every game-derived file is generated locally from your
copy and never leaves your machine.

## Status

Playable, work in progress. Details: `docs/mac/RUNBOOK.md`.

Working, on an M2 MacBook Air:

- A native arm64 `Repentance+.app`, built from your copy of the game by one resumable
  command (`scripts/mac/build.sh`). It runs from its own bundle.
- Picture through SDL3 and ANGLE on Metal: resizable window and full screen. By default
  the game lays itself out for the window's real size, as on PC: it fills the screen,
  keeps every pixel a whole number of screen pixels, and pads the rest. Retina-sharp, at
  60 fps in full screen. Cutscenes stay 16:9, centred. Whole-number and stretch scaling
  are still options. Keyboard, mouse and gamepads (rumble, hot-plug). Native audio, with
  the intro video in sync.
- Your saves, imported from the Windows or Steam folders on first start.
- Steam Workshop mods: a Launcher syncs your subscriptions, and each mod can be switched
  on or off. The game's mods menu agrees with the Launcher.
- REPENTOGON-using mods through upstream's compatibility layer, including Epiphany (its
  Tarnished characters too) and Fiend Folio: RELOADED, whose items, enemies and characters
  now load in full. External Item Descriptions now sees REPENTOGON too: Bag of Crafting
  contents and output, TM Trainer's procedural items, transformation progress. Custom
  Stage API and Unique Items API see REPENTOGON as well; StageAPI's custom floors (its own
  and Fiend Folio's) work with it. Only REPENTOGON's console autocompletion is missing,
  since the game's own console has none. The layer decides per mod whether it can run,
  and now checks each name on the class the mod calls it on (a method one class has no
  longer counts for another). New: the player death, revive, health type, birth, bed,
  pocket swap and card and pill callbacks, and more entity methods that Fiend Folio and
  Epiphany use (familiars, enemy shields and splitting, capsule collision), the player
  health names (Candy Heart, Soul Locket, Keeper's Sack, the hearts on the HUD), tears,
  lasers, knives and effects, pickups, loot lists and item pools, and the shop purchase,
  Void, Compost, ghost pickup, Devil Room and Planetarium chance callbacks; and now the player's item and
  charge state, room cameras and effects, game-level settings and challenges, and the grid
  update, render, collision and damage callbacks; and now sprite stop, continue and overlay
  controls, points, beams, quads, text drawing checks and baby configs, plus the heart HUD,
  history HUD, completion mark, fortune text, weapon, laser, knife, sword, bone club and
  familiar laser callbacks, then level, room, throw, loot list, character select and slot
  explosion callbacks, plus real images (`CreateImage`, `RenderToImage`, `SetSpritesheet`) drawn in
  software. A recompilation gap that crashed the game
  on a rarely used accessor is fixed, and so is a crash in a script argument check that
  handed the game a pointer from outside its memory.
  It has 1,273 of REPENTOGON's 1,727 functions and 302 of its 380 callbacks, the
  ones the tested mods use first. Item settings such as `ChargeType` are writable, as
  with REPENTOGON. `scripts/mac/rgon_census.py` measures the rest.
- One pinned game version. The recompilation only works with one exact `isaac-ng.exe`
  (Repentance+ 1.9.7.17, Steam build 22878971). `scripts/mac/fetch-game.sh` downloads
  exactly that build of the base game and every DLC from Steam, and `build.sh` refuses any
  other version.
- Large mod collections: the file table holds about 95 mods' worth of files, and a mod's
  `content-repentogon/` folder is read as REPENTOGON does.
- A Launcher with a startup window that shows each loading step. A boot with about 95 mods
  takes roughly 10 seconds. With that many mods the game now runs about 20% faster than
  before, and its log is written in the background. The log records frame and game-work
  times every 10 seconds with the floor and room (`scripts/mac/framestats.py` sums them up).
- Optional full screen into the strip beside the notch (NotchFill, see below).

Not done yet:

- Steam achievements: the bridge works against a stub, but stays off until it's checked
  with a real Steam client.
- Mod status effects declared without an id (some of Epiphany's and Fiend Folio's) do
  nothing yet. They used to freeze the game on a room change; that's fixed.
- More REPENTOGON coverage: most missing callbacks need new hooks in the game. Still
  open: the character select callbacks, real image sizes and custom shaders (the names
  exist but do nothing yet).
- Floor music is missing in some setups (the game reports no track for the stage); the
  cause is being looked into.
- A from-scratch build check.
- A rare Lua garbage-collector crash, seen once and not reproducible since. Debug switches
  for it are built in and off by default (`docs/mac/RUNBOOK.md`).

## REPENTOGON

Many mods need [REPENTOGON](https://github.com/TeamREPENTOGON/REPENTOGON), the script
extender that adds a large Lua API and new callbacks to the Windows game. REPENTOGON
itself is a Windows DLL and can't run here, so this project reimplements its features
for the Mac build: a Lua compatibility layer, started by upstream, plus hooks in the
game.

- **How:** we read REPENTOGON's source to learn what each function and callback does,
  then write our own implementation. Its structure layouts are only a starting point:
  every game field is measured in this build before we use it. None of REPENTOGON's
  C++ is copied.
- **Feature-parity target:** REPENTOGON `main` at
  [`0cd8f863f`](https://github.com/TeamREPENTOGON/REPENTOGON/commit/0cd8f863fe9947a8414d9018a700269603e629e6)
  (2026-11-09, version 1.1.3, its latest snapshot). That snapshot targets game v1.9.7.12;
  this build runs v1.9.7.17, which is why fields are re-measured.
- **Progress:** 1,273 of its 1,727 functions and 302 of its 380 callbacks, the ones
  the tested mods use coming first. `scripts/mac/rgon_census.py` compares the layer
  against a REPENTOGON checkout, and `docs/mac/layer-gaps.md` has the details.
- The layer tells each mod whether REPENTOGON is there, and only says yes when every
  REPENTOGON name the mod uses actually works. A mod that can run without REPENTOGON
  otherwise keeps running without it.

This is an independent reimplementation, not affiliated with or endorsed by Team
REPENTOGON.

## Use

```sh
git clone https://github.com/chiikabu/the-browsing-of-isaac
git -C the-browsing-of-isaac checkout b986dd8
scripts/mac/apply.sh the-browsing-of-isaac
```

Then, in the checkout, get the pinned Windows build of the game. You need to own Rebirth
and all four DLCs (Afterbirth, Afterbirth+, Repentance, Repentance+) on Steam. This uses
SteamCMD (`brew install --cask steamcmd`); you type your password and Steam Guard code
into SteamCMD yourself:

```sh
scripts/mac/fetch-game.sh --user "<steam login>" --out "<game folder>"
```

Then build the app from it:

```sh
scripts/mac/build.sh --game "<game folder>"
```

It needs Python, Java, Ghidra, Steamless, SDL3 and an ANGLE build with the Metal backend.
`docs/mac/build.md` lists them with the environment variables that point at them. The
first run takes about an hour, and later runs skip the stages that are current. The app
lands in `output/mac-app/Repentance+.app`.

## Third-party software

The Mac app is built with, and bundles, these libraries under their own licences:

- [NotchFill](https://github.com/squallsama/NotchFill) by Dmitrii Tikhonov, MIT. Its
  `libNotchFillHook.dylib` is built from the unchanged source at commit `7fd6679`
  (`native/mac/third_party/notchfill/`, with its licence). When the Launcher's
  "Fill the notch area in full screen" option is on, it's loaded into the game so
  macOS full screen also uses the strip beside the notch. Off by default.
- [SDL3](https://github.com/libsdl-org/SDL), zlib licence: window, input and audio.
- [ANGLE](https://chromium.googlesource.com/angle/angle), BSD-3-Clause: OpenGL ES on Metal.
- [REPENTOGON](https://github.com/TeamREPENTOGON/REPENTOGON)'s `enums_ex.lua`,
  GPL-2.0, unchanged, for the REPENTOGON-compatible layer.
- Valve's `libsteam_api.dylib` comes from your own Steam installation at build time.
  It isn't part of this repository.

## License

GPL-3.0-or-later, as upstream (the patch modifies upstream's GPL code).
*The Binding of Isaac* belongs to Nicalis, Inc. and Edmund McMillen; this is a
fan project, not affiliated with or endorsed by them or by upstream.
