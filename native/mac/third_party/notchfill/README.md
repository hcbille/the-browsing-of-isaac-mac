# NotchFill hook (vendored, unchanged)

- Origin: https://github.com/squallsama/NotchFill, file `Hook/NotchFillHook.m`
- Commit: `7fd66794f5b96098bf76fa8d1b8200d363f4df86`
- Licence: MIT, Copyright (c) 2026 Dmitrii Tikhonov (`LICENSE` here, copied unchanged)
- Status: **unchanged**. `NotchFillHook.m` and `LICENSE` are byte-for-byte what the
  GitHub contents API returns at that commit. No binary or release was downloaded.

What it does: a small library that is loaded into a program with `DYLD_INSERT_LIBRARIES`.
In macOS native full screen it stretches the window up beside the notch (it swizzles
`-[NSWindow _frameForFullScreenMode]`, `_frameForFullScreenModeInRect:` and
`_visibleTileFrameForFullScreen`), raises the full-screen window above the menu bar level, and
makes `-[NSScreen safeAreaInsets]` report a top inset of 0. `NOTCHFILL_DISABLE=1` loads it
without any change. It logs `NotchFillHook: loaded` through `NSLog`.

How the Mac Build uses it: `scripts/mac/build.sh` (stage `dylibs`) compiles it for arm64 into
`Repentance+.app/Contents/Frameworks/libNotchFillHook.dylib` and copies `LICENSE` to
`Contents/Resources/licenses/NotchFill-LICENSE.txt`. The Launcher loads it into the game only when
the option "Fill the notch area in full screen" is on (`docs/mac/launcher.md`).
