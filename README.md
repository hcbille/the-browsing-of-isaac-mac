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
- the Mac Host: the Guest memory region, real file I/O and a headless driver
  (`native/mac/host`)
- a native build script (`scripts/mac/build_native.py`)
- a graphics spike proving SDL3 + ANGLE on Metal (`native/mac/spike`)
- the design: `GLOSSARY.md`, `docs/adr`, `docs/mac`

## What is not here

No game files, no executable, no assets, no decompiled or recompiled game
code, and none of upstream's sources. You need your own copy of the game and
a checkout of upstream; every game-derived file is generated locally from your
copy and never leaves your machine.

## Status

See `docs/mac/NEXT.md`. Today: the upstream browser pipeline is verified on an
M2, every Host file compiles natively for arm64, and the first native link is
being brought up. There is no playable Mac app yet. Planned: SDL3 window,
input and audio, Steam achievements and Workshop mods, a Launcher, and
REPENTOGON support through upstream's compatibility layer.

## Use

```sh
git clone https://github.com/chiikabu/the-browsing-of-isaac
git -C the-browsing-of-isaac checkout b986dd8
scripts/mac/apply.sh the-browsing-of-isaac
```

Then build upstream as its `docs/DEVELOPING.md` describes, through
`build_boot.py`, and run `python scripts/mac/build_native.py` in the checkout.

## License

GPL-3.0-or-later, as upstream (the patch modifies upstream's GPL code).
*The Binding of Isaac* belongs to Nicalis, Inc. and Edmund McMillen; this is a
fan project, not affiliated with or endorsed by them or by upstream.
