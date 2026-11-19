# The window: scaling, full screen and adapt

Design record of how the Mac Build sizes its picture. Moved here from the old status file; the evidence paths
refer to local scratch folders.

## Window and scaling (M5)

Status: the window is resizable and the picture is scaled into it by `native/mac/host/mac_window.c`; fullscreen
follows the game's Fullscreen option; the mouse maps through the same rectangle.

How it works:
1. The game's "default framebuffer" is an offscreen framebuffer (RGB8 + depth/stencil) of the game's own size,
   960x540 (read from the Guest's window size at `0x00c799a8/ac`; the fake display in `host_shims_win.c` is 960x540
   on the native build, and `GetClientRect` returns the client size, not the frame size, so the game's own
   fullscreen toggle keeps rendering at 960x540). `glBindFramebuffer(0)` binds it (`host_gl_webgl.c`,
   `ISAAC_NATIVE`-guarded) and `GL_FRAMEBUFFER_BINDING` answers 0 for it.
2. At every present `isaac_mac_window_compose()` clears the drawable black and `glBlitFramebuffer`s the picture
   centred: **integer** (default): the largest whole multiple of 960x540 that fits, nearest filter, bars on both
   axes; if the window is smaller than 1x the picture is fitted (linear) so it is never cut. **fill**: the largest
   aspect-preserving size, linear filter, bars on the short axis. Screenshots (`ISAAC_MAC_SHOT`) now read the
   composed drawable, bars included.
3. Setting: `--scale integer|fill`, launch-settings key `"scale_mode"`, env `REPENTANCE_MAC_SCALE` /
   `ISAAC_MAC_SCALE` (precedence in that order). Initial size: `--window WxH` / `ISAAC_MAC_WINDOW=WxH` (points;
   test knob). `ISAAC_MAC_WINDOW_BORDERLESS=1` is a test hook (macOS keeps a titled window inside the screen, so
   a 2x window on this 1710x1107-point desktop needs a borderless one). No Retina rendering: the drawable is in points.
4. Fullscreen: SDL3 desktop (borderless, Spaces) fullscreen. `Options.Fullscreen` is the byte at Manager+0x2a360
   (Manager = `[0x00c7169c]`; Options at Manager+0x2a33c, Fullscreen at Options+0x24, set by `FUN_00925c60`, which
   runs the game's own window change `FUN_00a196b0`). `mac_window.c` polls it once per frame and toggles the SDL
   window when it changes (options menu: verified by scripted menu navigation; game's F key). **F is the game's own
   fullscreen key** (found by feeding F to the game: it toggles the option), so plain F needs no Host code, it is
   forwarded as before. **Cmd+Ctrl+F** (the macOS key, not forwarded to the game) presses F for the game, so the
   option, the menu, options.ini and the window always agree; if the game has not changed the option after 12
   frames it toggles the window directly (the byte is not written then, the game re-syncs it). Neither works while
   the debug console is open (F is text there). Leaving fullscreen by the system (green button) is followed by
   the poll/event too.
5. Mouse: `isaac_mac_window_to_game()` (moved from `mac_input.c` to `mac_window.c`) maps window points through the
   same rectangle; a move in the bars clamps to the nearest game pixel, a press in the bars is ignored (a release is
   always passed). `ISAAC_MAC_MOUSE_LOG=1` logs every mapped event; button events always log.
6. Script (`ISAAC_MAC_SCRIPT`): `F:fullscreen[:on|:off]`, `F:cmdctrl-f` (SDL event with Cmd+Ctrl), `F:window:WxH`,
   `F:mousewin:X:Y` (a point in the window, bars included); with `ISAAC_MAC_SCRIPT_VIA_SDL=1` the `mouse:X:Y` token
   goes through the scaling rectangle too.

Decisions: aspect-preserving fill (not stretch); whole-number is relative to the game's 960x540; scale mode is a
launch setting, not an in-game option (the Launcher can add a setting later: key `scale_mode`); the F shortcut is
the game's own rather than a Host shortcut (it exists on PC too).

Notes: after a change to `mac_launch.h` (struct layout) touch the files that include it, `build_native.py` only
compares file times. The 1920x1080 and larger window screenshots use the borderless test hook.

Evidence (`.scratch/m5/`, commands in `.scratch/m5/run.sh`; shots in `.scratch/m5/shots/`):
- integer 1280x800: `int-1280x800.png` (1x, bars 160/130); 1100x700: `int-1100x700.png` (1x, bars 70/80); 2000x1200:
  `int-2000x1200.png` (2x, bars 40/60); 1920x1080: `int-1920x1080.png` (2x, no bars).
- fill 1280x800: `fill-1280x800.png` (x1.33, bars 40 top/bottom); 1100x700: `fill-1100x700.png`; 2000x1200:
  `fill-2000x1200.png` (x2.08).
- fullscreen (this desktop's drawable is 1710x1074 points): integer by Cmd+Ctrl+F from a 1280x800 window
  `fullscreen-integer-from1280x800.png`, back by the game's F `windowed-after-F-1280x800.png`, again by Cmd+Ctrl+F
  `fullscreen-integer-2nd.png`; fill by the game's F from 1100x700 `fullscreen-fill-from1100x700.png`, back by
  Cmd+Ctrl+F `windowed-after-cmdctrlf-1100x700.png`; by the Options menu `menu-option-fullscreen-on.png` and
  `menu-option-fullscreen-off.png` (log `.scratch/m5/menuFS.log`: "fullscreen on by the game option").
- mouse (`mouse-integer.log`, `mouse-fill.log`): game 959,539 -> window 1119.5,669.5 -> back to game 959,539; a
  press at window 5,5 in the bars is "ignored".
- headless M1: `ISAAC_MAC_RUN_FRAMES=3000 ... --headless` exits 0, "reached 3007 frames; guard intact"
  (`.scratch/m5/headless.log`). Windowed 60.0 fps in a room at 1280x800 and 2000x1200.

## Full-screen adapt (the default)

Status: the default scale mode is now **adapt**. The game is told the real window size (windowed, resized, full
screen with or without NotchFill) and lays itself out for it, as the PC build does: it picks its own whole-number
pixel scale, centres its viewport, anchors the HUD to the screen edges and shows more or less room around the
play field. `integer` and `fill` (M5) are unchanged (`--scale integer|fill`, `"scale_mode"`, `REPENTANCE_MAC_SCALE`).

How the game learns its size (all from the decompile): GLFW's window procedure (`0x00a5b7b0`, class `GLFW30`)
handles WM_SIZE and, when the size differs from what it holds, calls the framebuffer-size callback
(`0x00a19bf0`), which stores the size at `0x00c799a0/a4`, calls the game's resize callbacks and ends in
`0x009a9ef0`. That calls `FUN_00954320` (menus, runs) or `FUN_00954150` (cutscenes, state 3), which compute the
layout and write `0x00c799a8/ac` (the viewport size). The Host already ran Guest window procedures
(`DispatchMessageW`), so the change is: `isaac_native_window_resized(w, h)` in `host_shims_win.c` stores the size
(`GetClientRect` then answers it exactly) and queues a WM_SIZE (`lParam = w | h << 16`) for the game's window;
`mac_window.c` calls it from the per-frame hook when the drawable changed, and once at frame 0 (the game starts
from its option, 960x540). The fake display (`GetSystemMetrics`, `GetMonitorInfoW`, `EnumDisplaySettings`) answers
the real display (CoreGraphics, dlsym'd). No Guest memory is written.

**How the game picks its scale** (`FUN_00954320`, `Viewport: WxH` / `Framebuffer Width ...` in the log):
`dp = floor(960 / W + 0.5)`; `s = min(W/400, H/240)`; `scale = min(max(floor(dp*s)/dp, 1/dp), Options.MaxScale)`;
logical size = `floor(W/scale) x floor(H/scale)`; viewport = `dp*scale*logical` (the largest whole multiple that fits,
the rest is padding the game centres); a second cap by Options.MaxRenderScale sets the internal "Render Width"
(960 at 1920x1080: the scene is rendered at render scale 2 and shown at 4). So the game's own scale **is** whole-number.
**Game fact found: `dp` is 0 for any window wider than 1920**, so the game then lays out 0x0 and crashes (malloc of a
garbage size) -- first try at 2200x1400 pixels. The Mac drawable is bigger than that (3420x2148, Retina), so the Host
gives the game the drawable divided by the smallest whole **k** that makes it at most 1920 wide (`adapt_k`), the FBO is
that size, and compose blits it back up by k, nearest. Every art pixel is then an N x N block with N = game scale x k;
only the odd leftover device pixels (fewer than k per side) are bars. Nothing was patched in the game.

Note on the display: on this Mac the full-screen drawable is **3420x2148 pixels** (1710x1074 points x2, or 3420x2224
with NotchFill), not 2560x1664. macOS renders a "looks like 1710x1107" mode at 2x and the GPU scales that to the
2560x1664 panel (x0.75). So at the panel an 8-pixel block is about 6 physical pixels; a whole-number guarantee
cannot reach past that macOS scaling step (the panel's native mode would be needed).

Measured (`.scratch/m13-adapt/blocks.py` counts the most common run length of identical pixels; evidence below):
- windowed 1280x800 points (2560x1600 px): game window 1280x800 (k 2), game scale 3 (viewport 1278x798), **N = 6**,
  padding in the game 1 px (2 device px) each side; runs 6/12/18/24 horizontally and vertically.
- default window 960x540 points (1920x1080 px): k 1, game scale 4, viewport 1920x1080, N = 4, no padding.
- 1100x700 points: k 2, game 1100x700, scale 2, viewport 1100x700, N = 4.
- full screen without NotchFill (3420x2148): game 1710x1074, scale 4 (viewport 1708x1072), **N = 8**, padding 2 device px
  left/right and 2 top/bottom.
- full screen with NotchFill (3420x2224): game 1710x1112, scale 4 (viewport 1708x1112), **N = 8**, padding 2 device px
  left/right, none top/bottom; the picture covers the notch strip.
- fps: 59.9-60.0 steady in a room at 3420x2148 and 3420x2224 and at 2560x1600 (the 54 fps lines are asset loading).
- mouse (`mouse.log`): window point -> game pixel is 1:1 with the game's window (640,400 -> 640,400 at 1280x800;
  10,5 -> 10,5; 1279,799 -> 1279,799), and the click goes to the same pixel.
- full screen on and off by the game's F key (script `fullscreen`) re-lays the game out each way (Viewport 1920x1080 ->
  1708x1072 -> 1920x1080 in `fs1.log`); resizes 960x540 -> 1100x700 -> 1280x800 follow (`t6.log`).
- headless M1 regression: `ISAAC_MAC_RUN_FRAMES=3000 ... --headless` exits 0, "reached 3011 frames; guard intact"
  (`headless.log`). integer and fill still draw as in M5 (`integer-1280x800.png` bars 160/130, `fill-1280x800.png` x1.33).

Files: `native/mac/host/mac_window.c` (adapt_k, layout, FBO follow, tell the game), `mac_gl.c` (high-pixel-density
window in adapt), `mac_launch.[ch]` (`scale_adapt`, default), `mac_main.c` (early display query),
`scripts/recomp/host/src/host_shims_win.c` (dynamic display size, `isaac_native_window_resized`, GetClientRect),
Launcher `Model.swift` / `Views.swift` / `Tests/main.swift`. Headless and the Browser Build are unchanged.

Cutscenes (intro, title art; `FUN_00954150`): the game keeps a fixed 960x540 viewport (`0x00c799a8/ac`) at the origin its
glViewport uses (`0x00c79984` when bit 0x40 of `0x00c798e4` is set, else `0x00c7b640`), which used to sit in the corner of
the window. In adapt, compose now blits only that viewport region, scaled by the largest whole number that fits the
drawable (nearest) and centred, black around it (`layout()` in `mac_window.c`; rooms use the same rule, their viewport is the
game window less a few padding pixels, so they still fill the window). The mouse maps through the same rectangle (the shown
part's corner is added back, in the game's top-left coordinates: window centre at 1280x800 points -> game 480,530, the
middle of the 960x540 region). At 1280x800 points the 960x540 picture is shown at x2 (1920x1080, bars 320/260 device px);
in full screen (3420x2148) at x3 (2880x1620, bars 270/264), art runs multiples of 6 (`intro-*.png`).

Not done / open: a menu item clicked by scripted mouse was not shown (the mapping is logged).

Evidence (`.scratch/m13-adapt/`, commands in `run.sh`; shots in `shots/`): `t6c.png` (1280x800 room, HUD at the edges),
`fs1-fullscreen.png`, `fs2-notchfill-fullscreen.png`, `fs1-back-windowed.png`, `integer-1280x800.png`,
`fill-1280x800.png`, `menu-1280x800.png`, `intro-nicalis-1280x800.png`, `intro-art-1280x800.png`, `intro-a-fullscreen.png`, `intro-b-fullscreen.png`, `room2-windowed-2560x1600.png`, `room2-fullscreen-3420.png`; logs `t6.log`, `fs1.log`, `fs2.log`, `mouse.log`, `headless.log`.

