# Where the Mac Build stands

Full status table, decisions and uncommitted-work list: the "Status overview"
at the top of `docs/mac/PROGRESS.md`.

## Done and verified

- M1 headless main, M2 picture (ANGLE Metal via SDL3), M3 input (keyboard,
  mouse, gamepads with rumble and hot-plug), M4 sound (SDL3 audio, native
  mixer). Each one is committed on `mac-native` and published.

## Done, awaiting a Verifier (uncommitted)

- M5 window: integer / fill scaling, resizable window, fullscreen (game's F key, Options menu, Cmd+Ctrl+F), mouse mapping.
  Evidence and commands: "M5 Window" in `docs/mac/PROGRESS.md`, `.scratch/m5/`. Verifier: re-run `.scratch/m5/run.sh` cases,
  headless M1, and check a real mouse click in the bars and a resize by hand.

## In progress (uncommitted)

- Game-side integration of the Launcher contracts (launch settings, save dir, mods dir with the `disable.it`
  toggle, Vanilla Launch, `--lua-debug`, `--debug-console`, the Layer served at `mods/repentogon/`, `modscan.lua`,
  gate log): implemented and tested with scratch mods, see the "Integration" section of PROGRESS.md and the
  "Game side" notes in `docs/mac/launcher.md`. Awaiting a Verifier.
- The Launcher (`native/mac/launcher/`): UI, save import, Workshop sync and the Swift mod scan.

## Done 2026-11-19

- The app runs end to end from its own bundle. ANGLE loads from the app's Frameworks folder. Play reached the
  first room with 95 mods. The user still has to check a double-click from Finder (real save folder).
- Boot with Fiend Folio takes 10 s instead of 181 s (file-table delete fix). The Launcher's startup window
  shows the game's `[mac][boot] status:` steps (user-checked).
- Fiend Folio RELOADED sees REPENTOGON: the gate retries a Workshop folder name without its `_<id>`.
- Upstream PRs: chiikabu/the-browsing-of-isaac#23 (file table) and #24 (gate).
- `scripts/mac/run.sh` and `fresh.sh` promoted. The Steam bridge is off unless `ISAAC_STEAM=1` (M9 not
  verified, and builds bundle the real Steam library).

## Next step (as of 2026-11-19: RUNBOOK section 11 step 3, M9 with the stub)

1. Verifier: re-run the Integration checks (headless M1 3,000 frames, Vanilla Launch, `--lua-debug`,
   `--debug-console`, off-mod greying and toggling), then the M6 save and M7 Workshop cases against the real game.
2. Commit the integration work on `mac-native`, publish.
3. M8 (rounds 1 and 2 done, see PROGRESS.md and `docs/mac/layer-gaps.md`): 5 of 11 REPENTOGON-using mods Shown (verified 2026-11-18), Epiphany runs Basement I to
   Dark Room with no Lua error, three engine hooks added. Next: Custom Stage API and External Item Descriptions (structures to
   measure), Epiphany's remaining callbacks (more hooks by the same recipe), its characters on the wheel in a window, where the
   menu reads input for `MC_MENU_INPUT_ACTION`; then a Verifier (and a Browser Build rebuild and run, never done for the new hooks).
4. Then M9 (`docs/mac/steam-bridge.md`), M10 against the real game, M11, M12.
