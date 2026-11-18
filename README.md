# the-browsing-of-isaac-mac

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

Playable, work in progress. Details: `docs/mac/NEXT.md` and `docs/mac/RUNBOOK.md`.

Working, on an M2 MacBook Air:

- A native arm64 `Repentance+.app`, built from your copy of the game by one resumable
  command (`scripts/mac/build.sh`). It runs from its own bundle.
- Picture through SDL3 and ANGLE on Metal: resizable window, full screen, integer or fill
  scaling. Keyboard, mouse and gamepads (rumble, hot-plug). Native audio, with the intro
  video in sync.
- Your saves, imported from the Windows or Steam folders on first start.
- Steam Workshop mods: a Launcher syncs your subscriptions, and each mod can be switched
  on or off. The game's mods menu agrees with the Launcher.
- REPENTOGON-using mods through upstream's compatibility layer: 5 of 11 tested mods,
  including Epiphany (its Tarnished characters too) and Fiend Folio: RELOADED. The layer
  decides per mod whether it can run.
- A Launcher with a startup window that shows each loading step. A boot with about 95 mods
  takes roughly 10 seconds.
- Optional full screen into the strip beside the notch (NotchFill, see below).

Not done yet:

- Steam achievements: the bridge works against a stub, but stays off until it's checked
  with a real Steam client.
- Full-screen scaling that fills the whole notched display.
- More REPENTOGON coverage (External Item Descriptions, StageAPI and others).
- A from-scratch build check, and a known rare Lua garbage-collector crash.

## Use

```sh
git clone https://github.com/chiikabu/the-browsing-of-isaac
git -C the-browsing-of-isaac checkout b986dd8
scripts/mac/apply.sh the-browsing-of-isaac
```

Then, in the checkout, build the app from your Windows copy of the game:

```sh
scripts/mac/build.sh --game "<folder with isaac-ng.exe>"
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
