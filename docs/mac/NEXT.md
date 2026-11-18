# Where the Mac Build stands

Full status table, decisions and uncommitted-work list: the "Status overview"
at the top of `docs/mac/PROGRESS.md`.

## Done and verified

- M1 headless main, M2 picture (ANGLE Metal via SDL3), M3 input (keyboard,
  mouse, gamepads with rumble and hot-plug), M4 sound (SDL3 audio, native
  mixer). Each one is committed on `mac-native` and published.

## In progress (uncommitted)

- Game-side integration of the Launcher contracts (launch settings, save dir, mods dir with the `disable.it`
  toggle, Vanilla Launch, `--lua-debug`, `--debug-console`, the Layer served at `mods/repentogon/`, `modscan.lua`,
  gate log): implemented and tested with scratch mods, see the "Integration" section of PROGRESS.md and the
  "Game side" notes in `docs/mac/launcher.md`. Awaiting a Verifier.
- The Launcher (`native/mac/launcher/`): UI, save import, Workshop sync and the Swift mod scan.

## Next step

1. Verifier: re-run the Integration checks (headless M1 3,000 frames, Vanilla Launch, `--lua-debug`,
   `--debug-console`, off-mod greying and toggling), then the M6 save and M7 Workshop cases against the real game.
2. Commit the integration work on `mac-native`, publish.
3. M8: fix Epiphany's `epiphany_menu.lua:35` error ("no animation named Overlay"), give the Layer a mod-less save
   hook, re-measure gaps with a real `implemented.txt`, grow the Layer (Epiphany first).
4. Then M5, M9 (`docs/mac/steam-bridge.md`), M10 against the real game, M11, M12.
