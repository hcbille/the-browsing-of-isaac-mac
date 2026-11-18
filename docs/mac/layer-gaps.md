# Layer gaps (milestone M8)

Measured on the Mac Build: the 95 mods of the user's Workshop collection (<workshop dir>, copied by the Launcher's pre-launch into a scratch support folder), the Layer run for real (`lab_names.lua` dump of `REPENTOGON.Implemented`, the gate decisions the game logs and writes to `layer-gate.json`), a scripted run (Welcome, title and Continue/New Run with Enter, then the debug console's `stage 2`). Round 1 of M8 (this document replaces the earlier read-only estimate, which counted a name as implemented when it appeared anywhere in `rgon.lua`).

## How the numbers were taken

- `ISAAC_RGON_LAB=lab_names.lua` (the file is copied beside the staged Layer; labs are not packed) writes one log line per implemented name (`lab_names: I <class>.<name>`) and per callback id: 657 names and 138 callbacks after round 2. The gate matches a mod's names by the part after the last dot, as it always has.
- The gate's own log is the measurement: `[isaac][rgon] gate: <folder> is shown|not shown REPENTOGON: <why> (N name(s) of REPENTOGON's used, M missing: ...)`, and `layer-gate.json` (`mods.<folder>.missing`, full lists).
- A run with all 95 mods on (`.scratch/m8/all4.log`): the only Lua error in the whole run is `! fired's_angels_and_statues/main.lua:18` (a mod bug, see below). Names of a mod's own that happen to match REPENTOGON's are counted as uses (the rule matches by name only), so a few "missing" entries below are the mods' own methods.

## 1. Per-mod results (the REPENTOGON-using Target Mods), before and after round 2

11 of the 95 mods name REPENTOGON in code (Fiend Folio, packed by the Browser Build's page, is not in this Workshop folder). The rule is unchanged (upstream's gate): round 1 had **3 shown, 8 hidden**; after round 2 **6 shown, 5 hidden**; the Verifier's run (2026-11-18) shows **5 shown**: Unique Items API is hidden again for `MC_MENU_INPUT_ACTION` and `MC_PRE_GLOWING_HOURGLASS_LOAD`, not counted until seen firing. A floor change in the Run column is the game's own trapdoor driven by `lab_trapdoor.lua`; the run's `NEW LEVEL` lines and the transition screenshots are under `.scratch/m8/` (`f2.log`, `deep.log`, `b3.png`, `b4.png`).

| Mod | Workshop id | Round 1 | Now | Names used / missing now | Run |
|---|---|---|---|---|---|
| [Rep] The Binding of Isaac: Epiphany | 3012430463 | Shown | **Shown** | 311 / 112: AddCandyHeartBonus, AddCollectibleCycle, AddLocust, AddMagnetized, AddNullItemEffect, AddSoulLocketBonus, CanPlaceRoom, CanPlaceRoomAtDoor, Capsule, CreateAfterimage, DropCollectibleByHistoryIndex, Dump and 100 more | Shown. Loads to the end of main.lua; Continue/New Run; 10 real floor changes by the game's own trapdoor (lab_trapdoor) Basement I to Dark Room; no Lua error (11,500 frames). |
| Door Overhaul (Aaron's) | 3585879877 | Shown | **Shown** | 2 / 0 | Shown. 3 real floor changes (to Caves II) in the 95-mod run, no Lua error. |
| 'Enhanced Boss Bars | 2635267643 | Shown | **Shown** | 1 / 0 | Shown. Same run, no Lua error. |
| Custom Stage API | 1348031964 | hidden | **hidden** | 38 / 13: GetDecoSaveState, GetEntitiesSaveState, GetGridEntitiesSaveState, GetTaintedKeeperCoinSpawns, GiveMinecart, InitFlipState, MC_CONSOLE_AUTOCOMPLETE, MC_PRE_BOSS_SELECT, SetBackdropType, SetRail, SetTaintedKeeperCoinSpawns, SetWaterAmount and 1 more | Not shown; loads, 3 real floor changes, no Lua error. |
| !!miniMAPI | 1978904635 | hidden | **Shown** | 2 / 0 | Shown now (round 2: Level:HasMirrorDimension). 3 real floor changes in the 95-mod run, no Lua error. |
| !![REP]Music Mod Callback | 2491006386 | hidden | **Shown** | 2 / 0 | Shown now (round 2: NightmareScene, MC_POST_NIGHTMARE_SCENE_RENDER by a new engine hook). 3 real floor changes through the nightmare scene, no Lua error. |
| !Unique Items API | 2836316643 | hidden | **Shown** | 6 / 0 | Shown now (round 2: MC_PRE/POST_GLOWING_HOURGLASS_SAVE/LOAD by engine hooks, MC_MENU_INPUT_ACTION by a hook; see "MC_MENU_INPUT_ACTION" below). Same run, no Lua error. |
| !!~External item descriptions | 836319872 | hidden | **hidden** | 30 / 14: GetActionProperty, GetActionType, GetBagOfCraftingContent, GetBagOfCraftingOutput, GetConditionProperty, GetConditionType, GetEffect, GetEffectCount, GetEntitiesSaveState, GetI3, GetPlayerFormCounter, GetTargetItem and 2 more | Not shown; runs on its own code, no Lua error. |
| Mantle Rework | 2665189152 | hidden | **hidden** | 0 / 0 | Not shown (steps aside); no Lua error. |
| Giant Props! | 2846951982 | hidden | **hidden** | 0 / 0 | Not shown (uses nothing); no Lua error. |
| Planetarium Chance [REP+] | 2489006943 | hidden | **hidden** | 0 / 0 | Not shown (uses nothing); no Lua error on a new run. |

The other 84 mods never name REPENTOGON and get no decision (REPENTOGON reads nil for them).

Mods that stay hidden do so by the rule, not by a bug: Giant Props, Planetarium Chance and Mantle Rework use nothing the Layer could provide or step aside for REPENTOGON; Custom Stage API, miniMAPI, Music Mod Callback, Unique Items API and External Item Descriptions run without REPENTOGON and are hidden while a name they use is missing (each list is under "Remaining names" below).

## 2. What is not a Layer gap

- **`! fired's_angels_and_statues/main.lua:18`** (`attempt to index a nil value (global 'AngelStatueMod')`): the mod's own bug. `AngelStatueMod` is never defined in any file of any of the 95 mods; the line indexes it while registering a callback. The same error appears with Vanilla Launch (`.scratch/m8/allv.log`), so it is not the Layer, the Host or the file layer. A Workshop update of the mod fixes it; nothing is done here (the Workshop folder is never edited).
- **`controls.anm2: no layers` (ASSERT)** and the `invalid tag: uniquefamiliar` and `Got invald parameter lockeddesc` / `hidden` lines of Epiphany's content: the game's own parser objecting to the mod's XML, as on a PC.

## 3. Fixed in round 123 (Epiphany first)

| Where Epiphany stopped | Root cause | Fix |
|---|---|---|
| `epiphany_menu.lua:35` "no animation named Overlay", "no member named 'A'" | REPENTOGON's `Sprite(path, load)` constructor: the game's makes an empty sprite and ignores the arguments, so Epiphany's sprites had no animations (also the Browser Build's gap, not a Mac Host gap: the same Layer) | `api_sprite.lua`: the Sprite constructor loads the path; `api_gaps.lua`: `Sprite.Color` hands out a writable Color view (the game's is a const copy); `BlendMode`, `BlendMode.New/NewFromType`, `LayerState:GetBlendMode` on the layer state's five ints at +0x78 (the default read back is `(0,1,7,1,7)`, the game's normal mode, which confirms the offset) |
| `ISAACSAVEMANAGER_POST_DATA_LOAD` `pairs(nil)` at `epiphany_achievements.lua:313` | a consequence: `main.lua` had stopped, so the save manager's tables were never made | gone once `main.lua` runs to its end |
| `epiphany_menu.lua:138` `ColorModifier` | REPENTOGON's value class missing | `api_gaps.lua`: `ColorModifier(r,g,b,a,brightness,contrast)` with `+ - * /` and `==` |
| `main.lua:363` `GetCostumeID`; `stageapi.lua:15` no config for a character | EntityConfigPlayer fields; and character numbers: a mod that names several characters alike (Epiphany's placeholder "NotDone" entries) made the lookup by name hand out the first one's number | `api_gaps.lua`: `GetCostumeID`, `GetNameImagePath`, `GetPortraitPath`, `GetExtraPortraitPath`, `GetSkinColor`, `GetCollectibles` and others, read from the player config record (name at +4 checked); `api_data.lua`: a mod's characters take the numbers the game gave them, in load order |
| `main.lua:1307` `Console.RegisterMacro` | missing | kept, not run (accepted, not supported) |
| `epiphany_menu.lua:392` `CharacterMenu` (and `MenuManager`, `SaveMenu`, `ItemOverlay`, `RoomTransition`, `BossPoolManager`, `RoomConfigHolder`) | globals only stand-ins in the gate | `api_menus.lua`: `MenuManager` (active menu +0x40, view position, colour correction), `CharacterMenu` (selected entry +0x10, page +0xdb4, unlocked +5, wheel fields, the 13 page sprites inside the menu), the rest accepted as inert; the others answer neutral values |
| `converter_beggar.lua:9` `Isaac.GetPoolIdByName`; `pool_table.lua:55` `GetCollectiblesFromPool` | missing | `api_gaps.lua`: from itempools.xml (a mod's custom pool gets a number of the Layer's own, not the game's) |
| `house_rules_card.lua:9` `Isaac.GetGiantBookIdByName`; `misc_util.lua:421` `Isaac.GetString` | missing | `api_gaps.lua`: giantbook.xml ids; `stringtable.sta` (English) |
| `suplex_explode.lua:3`, `samson.lua:1362`, `stock_fluctuation.lua:199` | player accessors | `api_gaps.lua`: custom cache tags with `MC_EVALUATE_CUSTOM_CACHE`, the six stat modifiers (applied at stat evaluation), suplex state (kept, not the game's), costume visibility (false) |

Also: a mod-less save mechanism (below) and `stage_layer.py --help`.

### The Layer's save file with no mod

The Layer's mod shell (`main.lua`, `RegisterMod("REPENTOGON (browser)")`) is not run in the Mac Build, so there was no mod to hang `Isaac.SaveModData` on and the Layer's save data (a mod's achievements, the innate items of a quit run) had nowhere to go. `host_rgon.c` now has `save_read(name)` / `save_write(name, text)` (bridge, with `isaac_fs_store` in `host_shims_fs.c`): the text goes into the file layer as a file of the game's save folder and through the persist hooks as if the game had written and closed it. `api_data.lua` uses it when the bridge says `SAVEFILE` (set only under `ISAAC_NATIVE`) and no layer mod registered: file `documents/my games/binding of isaac repentance+/rgon-layer-<slot>.dat`, one per save slot, same sectioned text as before. The Browser Build keeps its mod's data. Checked with `lab_store.lua` (written in one launch, read back in the next).

## 3b. Round 2 (engine hooks and menus)

Scope: the lift/hook pipeline may be edited. Three engine hooks were added (each is a wrapper installed by `lift_patches.py` `RGON_HOOKS`, applied with `apply_wrap_patches` on `output/recomp/lift/gu`, then only the two touched lifted TUs recompile; the hook table is in `rgon/tools/hook_table.py` and `hook_slots.lua`):

| Slot | Function | Address | Gives |
|---|---|---|---|
| 171 | `NightmareScene::Render` | 0x00920940 (REPENTOGON's signature matches nothing here; found as the only callee of `Manager::Render` next to `NightmareScene::Show` 0x00921ce0) | `MC_POST_NIGHTMARE_SCENE_RENDER` |
| 172 | `Game::SaveState` | 0x006f9000 (REPENTOGON's signature matches twice; this one is next to `RestoreState`) | `MC_PRE/POST_GLOWING_HOURGLASS_SAVE`; with the existing `Game::RestoreState` hook, `MC_PRE/POST_GLOWING_HOURGLASS_LOAD`. The hourglass states are the game's array at game+0x269ec, 0x20660 bytes each (read off `Game::GetGlowingHourglassState`); slot 0 and -1 seen in a run |
| 173 | `LuaCallbackCaller::CallInputAction` | 0x00874910 (REPENTOGON's signature matches once) | `MC_MENU_INPUT_ACTION`: outside a run the callback id the function runs is changed to 1464, as REPENTOGON does |

The hooks are generic wrappers that do nothing until the Layer arms them; the Browser Build's lifted code has the same wrappers (not re-lifted or run here: the host sources compile with `emcc -fsyntax-only`; the Browser Build was not rebuilt or run).

Lua-derived: `MC_MAIN_MENU_RENDER` (after `Manager::Render` while the menu manager exists), `Level:HasMirrorDimension`, `MainMenu`, `NightmareScene`, the menu offsets (measured with `lab_scan.lua`: Menu_Character at manager+0x1534, view positions +0xf704, colour blocks +0xf7c0/d8/f0, nightmare scene +0x21628; Menu_Game at +0xb90 as REPENTOGON has it), `Isaac.WorldToMenuPosition`, enum tables from `enums_ex.lua` (`BossType`) counted as implemented, and the character wheel's ids (id 0 random, 1 to 17 the game's characters in order Isaac ... Jacob, checked against the menu; from 0x12 a mod's).

**MC_MENU_INPUT_ACTION, honestly:** the hook is REPENTOGON's and it is armed while something listens, but in the main menu of this build `CallInputAction` was never seen running (a vanilla `MC_INPUT_ACTION` listener sees events only from the first frame of a run; none in the title, game or character menu). So the callback is implemented as REPENTOGON implements it and has not been seen to fire; a mod that needs menu input through it will get nothing until the menu code is found to call that function. Unique Items API (its one use is in its save manager) is Shown on this basis.

**Epiphany's characters in the character menu, not shown yet:** with every mod achievement unlocked (`lab_unlock.lua`, 137) the wheel still showed only the game's characters (screenshots `.scratch/m8/c3.png`, `c4.png`); Epiphany's own pages belong to its menu (`MENU.ID.TARNISHED`) and its tainted/Tarnished entries were not reached. Not verified.

## 4. Remaining names, from the gate log

### Epiphany (Hard Dependent): 112 names missing (311 used)

Callbacks not fired (34; each said once in the game log). Most are hooks from the middle of a game function (L).

| Callback | Effort | Reason |
|---|---|---|
| MC_CAN_SELECT_CHARACTER | ? | not classified yet |
| MC_CONSOLE_AUTOCOMPLETE | S | handler on an existing menu/HUD/console function (hook table row + api_hooks.lua) |
| MC_POST_CHALLENGE_DONE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_COMPLETION_MARKS_RENDER | M | render hook of a HUD/menu draw function; needs the draw call found |
| MC_POST_COMPLETION_MARK_GET | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_FIRE_BONE_CLUB | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_FIRE_KNIFE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_FIRE_SWORD | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_GRID_ENTITY_DOOR_UPDATE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_GRID_ENTITY_ROCK_UPDATE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_GRID_ENTITY_TRAPDOOR_UPDATE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_GRID_ROCK_DESTROY | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_PICKUP_SHOP_PURCHASE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_PLAYERHUD_RENDER_HEARTS | M | render hook of a HUD/menu draw function; needs the draw call found |
| MC_POST_PLAYER_DROP_CARD | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_PLAYER_DROP_PILL | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_RENDER_CHARACTER_SELECT_PAGE | ? | not classified yet |
| MC_POST_RENDER_CHARACTER_SELECT_PORTRAIT | ? | not classified yet |
| MC_POST_START_GREED_WAVE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_COMPLETION_MARKS_RENDER | M | render hook of a HUD/menu draw function; needs the draw call found |
| MC_PRE_DEVIL_APPLY_SPECIAL_ITEMS | ? | not classified yet |
| MC_PRE_GRID_ENTITY_TRAPDOOR_RENDER | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_LEVEL_PLACE_ROOM | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_M_MORPH_ACTIVE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_PICKUP_GET_LOOT_LIST | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_PICKUP_UPDATE_GHOST_PICKUPS | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_PICKUP_VOIDED | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_PLAYERHUD_RENDER_HEARTS | M | render hook of a HUD/menu draw function; needs the draw call found |
| MC_PRE_PLAYER_GRID_COLLISION | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_PLAYER_REVIVE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_RENDER_CHARACTER_SELECT_PAGE | ? | not classified yet |
| MC_PRE_RENDER_CHARACTER_SELECT_PORTRAIT | ? | not classified yet |
| MC_PRE_SLOT_CREATE_EXPLOSION_DROPS | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_TRIGGER_PLAYER_DEATH | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |

Functions and classes (78): some are Epiphany's own methods that match a REPENTOGON name, some are accepted and inert (below), the rest real gaps.

| Name | Effort | Reason |
|---|---|---|
| AddCandyHeartBonus | M | player field or call; player offsets moved vs REPENTOGON |
| AddCollectibleCycle | M | pickup fields/ghost/option-cycle logic |
| AddLocust | M | player field or call; player offsets moved vs REPENTOGON |
| AddMagnetized | M | field or call into the game, address to find |
| AddNullItemEffect | M | player field or call; player offsets moved vs REPENTOGON |
| AddSoulLocketBonus | M | player field or call; player offsets moved vs REPENTOGON |
| CanPlaceRoom | L | room placement/level-generator internals |
| CanPlaceRoomAtDoor | L | room placement/level-generator internals |
| Capsule | ? | not classified yet |
| CreateAfterimage | M | player field or call; player offsets moved vs REPENTOGON |
| DropCollectibleByHistoryIndex | M | player field or call; player offsets moved vs REPENTOGON |
| Dump | S | class method already on other classes (Knife/Tear/Laser InHitList; Delirium BossType; Dump is a render helper) |
| FindValidRoomPlacementLocations | L | room placement/level-generator internals |
| GenerationIndex | S | plain accessor |
| GetAlternatePedestal | M | pickup fields/ghost/option-cycle logic |
| GetBagOfCraftingContent | M | player field or call; player offsets moved vs REPENTOGON |
| GetCostumeLayerMap | M | player field or call; player offsets moved vs REPENTOGON |
| GetEntries | S | plain accessor |
| GetHeartAnim | S | plain accessor |
| GetImmaculateConceptionState | M | player field or call; player offsets moved vs REPENTOGON |
| GetIsSpinAttack | M | field or call into the game, address to find |
| GetIsSwinging | M | field or call into the game, address to find |
| GetKeepersSackBonus | M | player field or call; player offsets moved vs REPENTOGON |
| GetMarkedTarget | M | player field or call; player offsets moved vs REPENTOGON |
| GetMultiplier | M | field or call into the game, address to find |
| GetNeighboringRooms | L | room placement/level-generator internals |
| GetPickupGhost | M | pickup fields/ghost/option-cycle logic |
| GetPonyCharge | M | player field or call; player offsets moved vs REPENTOGON |
| GetRandomPickupVelocity | M | pickup fields/ghost/option-cycle logic |
| GetRandomWisp | M | field or call into the game, address to find |
| GetShiftIdx | M | field or call into the game, address to find |
| GetSuplexState | M | Suplex fields on the player; offsets must be measured |
| GetTaggedItems | ? | not classified yet |
| GetTearDisplacement | M | player field or call; player offsets moved vs REPENTOGON |
| GetUrnSouls | M | player field or call; player offsets moved vs REPENTOGON |
| GetWeaponType | M | field or call into the game, address to find |
| HasFlipData | M | pickup fields/ghost/option-cycle logic |
| InHitList | S | class method already on other classes (Knife/Tear/Laser InHitList; Delirium BossType; Dump is a render helper) |
| IncrementPlayerFormCounter | M | player field or call; player offsets moved vs REPENTOGON |
| InitFlipState | M | pickup fields/ghost/option-cycle logic |
| IsEternalHeartOverlayVisible | S | plain accessor |
| IsHardMode | M | field or call into the game, address to find |
| IsItemCostumeVisible | M | player field or call; player offsets moved vs REPENTOGON |
| IsNullItemCostumeVisible | M | player field or call; player offsets moved vs REPENTOGON |
| IsOverlayEventTriggered | M | field or call into the game, address to find |
| IsPauseMenuOpen | M | field or call into the game, address to find |
| IsPosVisible | M | Camera object fields, new class wrapper |
| MakeShopItem | M | pickup fields/ghost/option-cycle logic |
| PhantomFloat | M | field or call into the game, address to find |
| PickCollectible | M | field or call into the game, address to find |
| PushEntry | S | plain accessor |
| RemovePoopSpell | M | player field or call; player offsets moved vs REPENTOGON |
| SetActiveVarData | M | player field or call; player offsets moved vs REPENTOGON |
| SetBackdropType | M | room fields; Backdrop needs rendering effect |
| SetClampEnabled | M | Camera object fields, new class wrapper |
| SetEveSumptoriumCharge | M | player field or call; player offsets moved vs REPENTOGON |
| SetFocusPosition | M | Camera object fields, new class wrapper |
| SetGroup | L | save-state structures of the room (entities/grid) |
| SetHeadDirection | M | player field or call; player offsets moved vs REPENTOGON |
| SetImmaculateConceptionState | M | player field or call; player offsets moved vs REPENTOGON |
| SetItemPool | M | room field |
| SetKeepersSackBonus | M | player field or call; player offsets moved vs REPENTOGON |
| SetNewOptionsPickupIndex | M | pickup fields/ghost/option-cycle logic |
| SetRoomClearDelay | M | room field |
| SetSuplexAimCountdown | M | Suplex fields on the player; offsets must be measured |
| SetSuplexLandPosition | M | Suplex fields on the player; offsets must be measured |
| SetSuplexState | M | Suplex fields on the player; offsets must be measured |
| SetSuplexTargetPosition | M | Suplex fields on the player; offsets must be measured |
| SetVarData | M | pickup fields/ghost/option-cycle logic |
| ShuffleCostumes | M | player field or call; player offsets moved vs REPENTOGON |
| SnapToPosition | M | Camera object fields, new class wrapper |
| TriggerTheresOptionsPickup | M | pickup fields/ghost/option-cycle logic |
| TryAimAtMarkedTarget | M | field or call into the game, address to find |
| TryInitOptionCycle | M | pickup fields/ghost/option-cycle logic |
| TryPlaceRoom | L | room placement/level-generator internals |
| TryRemoveCollectible | M | pickup fields/ghost/option-cycle logic |
| UpdateColorModifier | M | room field |
| UpdatePickupGhosts | M | pickup fields/ghost/option-cycle logic |

### Accepted, not supported (inert or approximate; counted as gaps)

`Game:SetColorModifier` and the Game/Room colour-correction readers (kept in Lua; the game's own is not changed), `ItemOverlay.Show` (no banner), `RoomTransition.StartBossIntro`/`GetTransitionMode` (0)/`GetVersusScreenSprite` (empty sprite), `BossPoolManager.GetPool` (empty), `RoomConfigHolder.GetRoomByStageTypeAndVariant` (nil), `SaveMenu.GetDeletePopupSprite`, `MenuManager.SetActiveMenu`/`SetInputMask`, `CharacterMenu.SetSelectedCharacterMenu` (flag only, no page-turn animation), player Suplex state and `Get*Modifier`/`Set*Modifier` (the Layer's own, applied at stat evaluation, not the game's field), `GetMarkedTarget`, costume visibility, `Console.RegisterMacro`, custom item pools (a number of the Layer's, nothing draws from it). Not checked in a window: whether Epiphany's character menu shows its pages (the menu sprites are the game's own and are reached through the offsets above; no screenshot of it yet).

### Other mods (full lists in `layer-gate.json`)

- **Custom Stage API** (38 used, 13 missing): GetDecoSaveState, GetEntitiesSaveState, GetGridEntitiesSaveState, GetTaintedKeeperCoinSpawns, GiveMinecart, InitFlipState, MC_CONSOLE_AUTOCOMPLETE, MC_PRE_BOSS_SELECT, SetBackdropType, SetRail, SetTaintedKeeperCoinSpawns, SetWaterAmount, SetWaterCurrent
- **!!~External item descriptions** (30 used, 14 missing): GetActionProperty, GetActionType, GetBagOfCraftingContent, GetBagOfCraftingOutput, GetConditionProperty, GetConditionType, GetEffect, GetEffectCount, GetEntitiesSaveState, GetI3, GetPlayerFormCounter, GetTargetItem, IsItemInCollection, ProceduralItemManager

Why these stay hidden: Custom Stage API needs engine-state writes the Layer cannot make without measured offsets (room backdrop type, rails, water amount and current, tainted Keeper coin spawns, the room descriptor's entity/grid/deco save states, a minecart, a pickup's flip state) and two callbacks (`MC_PRE_BOSS_SELECT`, `MC_CONSOLE_AUTOCOMPLETE`). External Item Descriptions needs the Bag of Crafting's contents and output, the TM Trainer's procedural items (`ProceduralItemManager`, item effects), player form counters, the collection (`IsItemInCollection`) and `RoomDescriptor:GetEntitiesSaveState`. Each of those is a structure to find and measure in this build, not a name to add.

## 5. How the Layer is built (for later Layer engineers)

**Pieces.** The Layer is Lua (`scripts/recomp/rgon/rgon.lua` plus `api_*.lua`, loaded by name) on top of a small native bridge (`scripts/recomp/host/src/host_rgon.c`, table `BRIDGE`: `ptr/r8..r32/rf32/w*` memory access, `call`/`cfn` to call a game function by address, `obj`/`ud` to wrap a game object, `hook`, `gate`, `log`, `load`, `env`, `xmm`, `reg`).

**Where to add something.**
- A function on an existing class that reads a field or calls into the game: Lua only, in the matching `api_*.lua` (player in `api_player.lua`, entities in `api_objects.lua`/`api_game.lua`, XML/config in `api_data.lua`, sprites in `api_sprite.lua`). Registering it through the helpers in `rgon.lua` adds `Class.Name` to `REPENTOGON.Implemented`. The address or offset must be found in this executable (game's own Lua registration, a unique signature, a vtable slot, or disassembly) and checked in `test_layer*.lua`; REPENTOGON's offsets are for v1.9.7.12 and are not trusted.
- A function that needs only what the game exposes (or data the layer already holds): Lua only, no address.
- A callback that is "before/after a game function": a row in `tools/hook_table.py` (then `--write`, regenerates `hook_slots.lua` and `RGON_HOOKS` in `lift/lift_patches.py`), and `on("Class::Function", {ids}, before, after)` in `api_hooks.lua`. This changes the lifted game, so it needs an engine (lift) rebuild.
- A callback REPENTOGON fires from the middle of a game function: an instruction hook (`INSN_HOOKS`, `RGON_INSN_HOOKS`, handler `at("Class::Function@Point", ...)`), same rebuild. These are the L items in the tables above.
- C side (`host_rgon.c`) only changes for new bridge primitives; the 152 slot wrappers call `isaac_rgon_hook_pre/post`, which test one byte (`g_rgon_hook_on[slot]`) before any Lua runs.

**Callback dispatch.** Mod callbacks the game does not have (ids 1000 and up) are run by `api_callbacks.lua`: it keeps its own list per callback id, finds the functions for a parameter once (index), and applies REPENTOGON's result rules (additive, stop on false, first answer, merged tables) with an error in one function not stopping the others. The engine's own runner is watched and swapped so that mod registrations reach the Layer's list. A callback nobody fires logs `[REPENTOGON] not implemented: callback MC_X` once per mod.

**Layer load.** `isaac_rgon_before_chunk` (C) runs before the first chunk under `mods/` in each Lua state: it peeks `mods/repentogon/rgon.lua` from the game's file table (`RGON_DIR`), runs it once with the bridge as argument, and logs `the REPENTOGON-compatible layer is loaded`. `ISAAC_RGON=0` in the environment skips it entirely (this is the natural Vanilla Launch switch). The Layer loads its siblings with `B.load("<file>")` from the same folder.

**Gate.** At the end of `rgon.lua`: reads `modscan.lua` (written at launch) and `gate_names.lua` (generated, which names are REPENTOGON-only), weighs each mod (`gate_weigh`, `gate_rule`) and logs one line per mod: `[isaac][rgon] gate: <folder> is shown|not shown REPENTOGON: <why> (N name(s) of REPENTOGON's used, M missing: ...)`. The decisions are also in `REPENTOGON.Extras.Gate.mods`. `B.gate` installs the global `__index` that answers `REPENTOGON` per calling mod (found from the Lua source path `mods/<folder>/`). Test overrides: `ISAAC_RGON_ALLOW=<folder>,...|*`, `ISAAC_RGON_AUTO=0`, `ISAAC_RGON_PROBE=1` (every missing name answers with a stand-in and is logged, for measuring only).

**Version and coverage numbers.** The version is the constant `VERSION = "1.1.2"` in `rgon.lua` (REPENTOGON release followed; used by `MeetsVersion`), logged as `API 1.1.2 for game v1.9.7.17: N name(s) implemented` where N counts `REPENTOGON.Implemented` (names as `Class.Method` plus callback ids). There is no stored coverage percentage. The honest denominators are: REPENTOGON's surface of about 1,130 functions and 365 callbacks (README of `rgon`), `gate_names.lua` (1,132 function names + 35 globals only REPENTOGON has) and the callback ids of `enums_ex.lua` (1000 and up). The Launcher should compute coverage as implemented / (gate_names function names + callback ids >= 1000), from the `lab_names.lua` dump (`lab_names: I <name>` / `C <id>` log lines, produced with `ISAAC_RGON_LAB=lab_names.lua`) captured at build time, and show the Layer version beside it.

**Browser Build.** The page packs the Layer with `rgon/pack.mjs` into a mod archive named `repentogon` (own files + `hook_slots.lua`, `gate_names.lua` + REPENTOGON's `enums_ex.lua` with its GPL-2.0 licence), installs it like any mod (IndexedDB), seeds it into the Guest file table at `mods/repentogon/`, and as it seeds every other enabled mod runs `createModScan().add(id, files)` and then writes `mods/repentogon/modscan.lua` (`seedMods` in `web/mods.mjs`, called from `boot_web.mjs`; `?modscan=0` disables, `?ISAAC_RGON=0` turns the Layer off). Only mods that name REPENTOGON in code are listed.

## 6. M8 on the Mac Build (done: see PROGRESS.md "Integration")

The Mac Build runs `host_rgon.c` unchanged (it is part of the Host), so the Layer works if `mods/repentogon/{rgon.lua, api_*.lua, hook_slots.lua, gate_names.lua, enums_ex.lua, modscan.lua}` is visible through the Host's file table and `ISAAC_RGON` is not 0. Needed:

1. **Layer files built into the app.** `build.sh` copies the Layer files (and `enums_ex.lua` + licence) into the app bundle's Resources. Following ADR 0003 the Layer must not appear in the MODS list: serve it from a virtual path `mods/repentogon/` in the Host file layer (a small fallback in `mac_fs.c` mapping that prefix to Resources, so no folder appears in the user's `mods/`), instead of copying it into the instance `mods/`. If that Host change is not wanted yet, a first milestone step is acceptable: the Launcher copies the files to `mods/repentogon/` with a `disable.it` that it removes only for non-vanilla launches, and the ADR is revisited.
2. **Loaded before mods unless Vanilla Launch.** The Launcher sets `ISAAC_RGON=0` in the game's environment for Vanilla Launch (no other change; the C code already honours it).
3. **Mod scan at launch, writing `modscan.lua`.** The scan must see the same files the game will load: the enabled mods in the Mac Build's `mods/` (not those with `disable.it`), after the Workshop sync (M7).
4. **Gate log** is already one line per mod in the game log; the Launcher parses `[isaac][rgon] gate:` lines to show Shown/hidden and the missing names.

### Recommended approach for the scan: port `createModScan` to Swift in the Launcher

Options considered:
- Ship node: rejected (not acceptable for end users).
- Run the scan in C inside the Host: possible (the scanner is a tokenizer plus about 100 lines of rules) but it adds game-adjacent C to the patch, runs at the worst moment (inside the game's boot, before the first mod chunk) and cannot show results in the Launcher.
- **Swift in the Launcher (recommended).** The Launcher already reads the Workshop folder and copies the mods (`WorkshopSync.swift`), so the files are at hand at the moment the scan is needed; the scan runs before the game starts, can run in the background while the user looks at the Play page, can be cached per mod by (workshop update time, size), and the Launcher can show the gate preview (which mods will be Shown, what each hidden mod misses) before launch, which M10 wants. It writes `modscan.lua` to the place the Host serves as `mods/repentogon/modscan.lua` (a launch-time generated file, outside Resources). Port: `stripLua`, `scanLua`, the five regexes and `createModScan.add/lua` (about 180 lines of JS); use `NSRegularExpression` or hand tokenizing. Output format is the existing `return { version = 1, mods = { ["id"] = { says, quits, yields, files, names } } }`.
- **Test for the port:** this directory's `result.json` and `scan.mjs` give golden data. A Swift test must produce byte-identical `modscan.lua` to `createModScan().lua()` over the 95 Workshop mods (the node module is the reference; it can generate the golden file once at development time).

For the gate's name weighing, the Layer's Lua already does it at game start; the Launcher's preview can use `gate_names.lua` and the implemented list from the build-time `lab_names` dump.

## 7. Suggested order of Layer work (updated after round 2)

1. Custom Stage API (13 names) and External Item Descriptions (14): each needs structures measured in this build (see "Other mods").
2. Epiphany's remaining callbacks (grid entity updates, `MC_PRE_PLAYER_REVIVE`, `MC_PRE_TRIGGER_PLAYER_DEATH`, fire knife/sword/bone club, pickup voided/purchase, completion marks, `MC_PRE_LEVEL_PLACE_ROOM`, ...): one more batch of wrappers by the recipe above (find the function, add a row to `hook_table.py`, `lift_patches.py` `RGON_HOOKS`, `hook_slots.lua`, run `apply_wrap_patches`, `build_native.py`).
3. Epiphany's inert accessors (suplex, stat modifiers in the game's own field, marked target, costume visibility) and its character pages in a window.
4. Find out where the menu reads its input (so `MC_MENU_INPUT_ACTION` can fire).
5. Re-run the 95-mod measurement after each batch (`lab_names.lua`, `layer-gate.json`).

## 8. Epiphany's Tarnished menu page (fixed 2026-11-18)

Cause: `Sprite:Continue()` (REPENTOGON, resumes a stopped animation) is missing in the Layer. Epiphany's
`MENU:StartMenuTransition(TARNISHED)` (`epiphany_menu.lua:288`) calls it on the tainted-background deco sprite, errors
(`attempt to call a nil value (method 'Continue')`), so `MENU.swapping` is never set and the page is never loaded.
Everything before it works (`lab_tarn.lua`): `MC_POST_SAVESLOT_LOAD` args, `AreTaintedsUnlocked`, `IsMenuUnlocked`,
`IsMenuAccessible` all true, the BG sprite's `SwapOut`/`SwapIn` frames follow the real menu. Fix: `Sprite:Continue([continueOverlay])` in `api_more.lua`, built from the game's own `Play(name, true)` and
`Update()` (walks back to the frame it was paused on; no AnimationState offset needed). With it the swap runs
(`lab_tarn`: `M.next = 2`, `M.swapping = true`) and the user saw the Tarnished page in a hand test. Limitation: a
paused overlay restarts at frame 0 instead of keeping its own frame. `lab_tarn2.lua` (AnimationState dump) is kept
for a later exact version.
