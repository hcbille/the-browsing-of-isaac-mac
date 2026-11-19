# Where the Mac Build stands

Full status table, decisions and uncommitted-work list: the "Status overview"
at the top of `docs/mac/PROGRESS.md`.

## Resume here (2026-11-19 ~11:30)

- **Done since 04:00, committed on mac-native, not published** (public is still at 5d2e8cd):
  - EID is Shown (c42b3c0).
  - StageAPI names (3aaf55f). MC_PRE_BOSS_SELECT is lifted and fires in the play binary (rebuilt 11:14; headless M1
    reached 3,010 frames; `lab_stageapi_boss` with `ISAAC_LAB_BOSS=19` gives a boss room with subtype 19).
  - Lua heap debug tooling, off by default (869da06).
  - Game version pinning: `scripts/mac/game-pin.txt`, `fetch-game.sh`, the exe check in `build.sh` (703d09f,
    c6ce0bf). Verified with a real SteamCMD run, matching `game/` file for file.
  - `rgon.lua` loads the two new api files (5aff3b2).
  - The wip branch is fully merged in spirit and can be deleted once the user agrees.
- **Lua GC crash: set aside as not reproducible (the user's call, 2026-11-19).** About 45,000 windowed frames with heap
  checks found nothing. Don't soak for it again. Only if it comes back: run with `ISAAC_LUA_REDZONE=1
  ISAAC_LUA_HEAPCHECK=10` (RUNBOOK). Two other crash reports from that night were not investigated:
  - 01:04: an abort in `sub_0040cf00`.
  - 03:16: a null read in `lua_rawgetp` <- `sub_0085c590`, two seconds after launch, while the WIP Layer was in the tree.
- **Next:**
  1. Publish: run the check, update the README progress numbers from a fresh `rgon_census.py`, and add the pinned
     fetch to the README build steps. Ask the user before pushing.
  2. StageAPI Shown: a real `RoomConfig.GetRoomByStageTypeAndVariant`, then the room-set functions (AddRooms and the
     rest), then StageAPI's 33 `if REPENTOGON` branches. MC_CONSOLE_AUTOCOMPLETE comes last, since the game's own
     console has nothing to hook; decide with the user.
  3. The next census batches (layer-gaps section 9).
- **Still for the user to check by hand:** EID's box and a TM Trainer item in play; a StageAPI floor with water; and the
  older items (Epiphany `throwable_item_lib.lua:727`, Fiend Folio's veil curse minimap, Hot Potato Book).

## Before 11:30 (kept for history)

- **Published:** everything verified on mac-native up to 5d2e8cd is public at github.com/hcbille/the-porting-of-isaac
  (renamed from the-browsing-of-isaac-mac). That covers full-screen adapt, the Epiphany freeze, the Fiend Folio fix,
  Layer batches 1-3, Minimap and writable ItemConfigItem fields. The README has a REPENTOGON section (parity target
  `0cd8f863f`); update it at every publish (see the local project notes, CLAUDE.md).
- **Unfinished, on branch `wip/eid-stageapi-luagc` (fda1b87, unverified, not on mac-native):** three parallel tasks
  the user asked for, stopped mid-way. Resume each from that branch (`git checkout wip/eid-stageapi-luagc -- <files>`
  or a merge once verified).
  1. **EID** (`api_eid.lua`, `lab_eid_*.lua`): Bag of Crafting contents and output (match the game's own
     CalculateBagOfCraftingOutput) and RoomDescriptor:GetEntitiesSaveState pass their lab. Left: ProceduralItemManager
     and ProceduralItem (TM Trainer), GetPlayerFormCounter, IsItemInCollection, the action and condition getters,
     the gate check ("is shown REPENTOGON"), and a screenshot of EID's box.
  2. **StageAPI** (`api_stageapi.lua`, `lab_stageapi_*.lua`; plus the hook-table row for BossPool::GetBossId, c8ed18a,
     already on mac-native but not yet applied to the lift or binary): room water amount and current, rails, and
     SetBackdropType exist. Left: actually changing water and backdrop (screenshots), the deco/grid/entity save
     states, Tainted Keeper coin spawns, InitFlipState, MC_PRE_BOSS_SELECT (lift rebuild) and MC_CONSOLE_AUTOCOMPLETE,
     the gate check, and that StageAPI's dependents still change floors.
  3. **Lua GC crash** (`host_lua.c`, `host_lua_gcstate.c`, `run.sh` ISAAC_BIN): a redzone allocator, quarantine and
     heap walk, all off by default (ISAAC_LUA_REDZONE, ISAAC_LUA_HEAPCHECK, ISAAC_LUA_GC_FORCE, ISAAC_LUA_QUARANTINE_MB).
     A 2,500-frame debug run passed 5,000 heap checks: not reproduced yet. Next: lab_unlock_all on a fresh save,
     long 95-mod soaks. Finding from reading: Lua's heap is in the guest arena (same guest malloc), so a guest overflow
     or write-after-free into a Lua block gives exactly the corrupted `next` seen.
- After those: the missing REPENTOGON functions the mods use, in batches (census order in layer-gaps section 9).
- The play build (`.scratch/play/play.sh`, binary output/mac/isaac-mac built 02:17) matches mac-native and contains none
  of the WIP.

### Earlier notes (same day)

- Done today and committed on mac-native:
  - Full-screen adapt (public e6b48e5) and the Epiphany room-change freeze (public db9495c).
  - Census and Layer batches 1-3, plus the Fiend Folio fix c3985fd (file table 131,072 slots; content-repentogon/ read as content/). The user confirmed Fiend Folio works in play.
- Batch 3 verified (headless labs all PASS, the 95-mod trapdoor run through floors 1 to 5 shows the same one `ERR:` and no "failed" line as batch 2) and the global `Minimap` is in (`api_minimap.lua`, found at game+0x25ecc, GetIconsSprite is the real sprite: a replaced sheet draws after the next room change; Fiend Folio's curseveil branches run clean). Details: `layer-gaps.md` section 9, "Batch 3". Census: functions implemented 570 to 586, missing used by REPENTOGON-naming mods 256 to 245.
- Still to see by hand (RUNBOOK-style test for the user): Epiphany's `throwable_item_lib.lua:727` gone in play (the headless runs never hit it), and Fiend Folio's veil curse changing the minimap icons in a real run (steps in the report of that session: `giveitem`/curse console, then open a new room).
- ItemConfigItem field writes (2026-11-19): Fiend Folio's `i.ChargeType = ...` (`hotpotatobook.lua:45`) failed with "no member named 'ChargeType'" because the game hands out `const Item` userdata. `api_item_fields.lua` gives the const class a `__propset` that calls the game's own setters (29 fields). Verified headless (`lab_icfg_test.lua` PASS; special-charge book gains nothing on room clear; 95-mod trapdoor run clean), details in `layer-gaps.md` section 9. Still to see by hand: Hot Potato Book in a real Fiend Folio run (no error on room clear).
- Next in the census order (`layer-gaps.md` section 9, "Next batches"): ProceduralItem (External Item Descriptions), `PlaceRoom` for `TryPlaceRoom`, then the L callbacks the mods use. Every test run cleans up its big folders (RUNBOOK).

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
