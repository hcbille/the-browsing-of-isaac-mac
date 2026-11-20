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
| !Unique Items API | 2836316643 | hidden | **Shown** | 6 / 0 | Shown (round 2 hooks; MC_MENU_INPUT_ACTION and MC_PRE/POST_GLOWING_HOURGLASS_LOAD fire for real since batch B4). |
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
| 173 | `LuaCallbackCaller::CallInputAction` | 0x00874910 (REPENTOGON's signature matches once) | unused since batch B4: nothing calls it outside a run in this build; `MC_MENU_INPUT_ACTION` is on slots 175-177 |

The hooks are generic wrappers that do nothing until the Layer arms them; the Browser Build's lifted code has the same wrappers (not re-lifted or run here: the host sources compile with `emcc -fsyntax-only`; the Browser Build was not rebuilt or run).

Lua-derived: `MC_MAIN_MENU_RENDER` (after `Manager::Render` while the menu manager exists), `Level:HasMirrorDimension`, `MainMenu`, `NightmareScene`, the menu offsets (measured with `lab_scan.lua`: Menu_Character at manager+0x1534, view positions +0xf704, colour blocks +0xf7c0/d8/f0, nightmare scene +0x21628; Menu_Game at +0xb90 as REPENTOGON has it), `Isaac.WorldToMenuPosition`, enum tables from `enums_ex.lua` (`BossType`) counted as implemented, and the character wheel's ids (id 0 random, 1 to 17 the game's characters in order Isaac ... Jacob, checked against the menu; from 0x12 a mod's).

**MC_MENU_INPUT_ACTION:** superseded by batch B4 below (slots 175-177; it fires in the menu now).

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

## 6. M8 on the Mac Build (done: see launcher.md "Game side")

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

## 9. Census of REPENTOGON's whole API (M8, 2026-11-19)

Tool: `python3 scripts/mac/rgon_census.py --repentogon <REPENTOGON checkout> --implemented <log> --mods <mods dir> --gate <layer-gate.json> --json <out>`.
It reads REPENTOGON's own documentation tables (every class method, variable, constructor, global and every
`ModCallbacks` id of the checkout, version 1.1.3), the names the running Layer reaches (`lab_census.lua`: one `lab_census: E`
line per function or property found on a class table, plus the `lab_names` dump of `REPENTOGON.Implemented`), the names each
Workshop mod uses (`modscan.lua`) and the gate log. Run the lab with `LAB=lab_census.lua` on a support folder (it needs a mod
chunk to run, so use a folder with any one mod). The Layer stages REPENTOGON 1.1.2; the checkout is 1.1.3, so about 18 callbacks
count as missing that the staged copy does not know.

Classes: "implemented" means in `REPENTOGON.Implemented` (really works); "inert" means a stand-in exists and does nothing or
approximates (counted as a gap); "game" means the game had the name before REPENTOGON (not counted); "missing" is the rest.
Effort: S pure Lua over what the Layer already has, M a game field must be measured or an existing game function called, L a new
engine hook plus a lift rebuild. The effort column is a first guess from the REPENTOGON body: reviewing the 15 S names mods use
showed most of them need a structure (ProceduralItem fields, PocketItem slot, Camera clamp), so S is an upper bound.

| | total | implemented | inert | game | missing | S / M / L | missing and used by a mod that names REPENTOGON |
|---|---|---|---|---|---|---|---|
| functions, before | 1727 | 485 | 40 | 27 | 1175 | 65 / 1049 / 61 | 295 |
| functions, after batch 1 | 1727 | 535 | 40 | 27 | 1125 | 59 / 1005 / 61 | 283 |
| functions, after batch 2 | 1727 | 570 | 37 | 27 | 1093 | 58 / 974 / 61 | 256 |
| functions, after batch 3 | 1727 | 586 | 39 | 27 | 1075 | 57 / 957 / 61 | 245 |
| variables | 66 | 6 | 0 | 2 | 58 | 5 / 53 / 0 | 4 |
| constructors | 17 | 3 | 0 | 4 | 10 | 0 / 10 / 0 | 1 |
| callbacks | 380 | 135 | 0 | 0 | 245 | 0 / 0 / 245 | 96 |

Every missing callback is L (a hook per callback). The biggest missing classes by name count: EntityPlayer 173, ImGui 58, Isaac 45,
Room 40, EntitiesSaveState 39, EntityPickup 29, ChallengeParam 28, EntityNPC 27, EntityConfigEntity 25, Options 25.
Missing names used by mods that name REPENTOGON, by class: EntityPlayer 48, EntityPickup 15, EntityKnife 11, Room 11, Weapon 11,
ProceduralItem 10, RoomDescriptor 10, EntityNPC 9, Game 8. The full per-name table (status, effort, mods) is the `--json` output;
the names a mod uses and the Layer lacks are in `layer-gate.json` ("missing" per mod).

### Batch 1 (`api_census.lua`): 50 names, all checked in the running game

Added: `RNG` GetShiftIdx, PhantomNext, PhantomInt, PhantomFloat, Previous, PhantomPrevious; `EntityConfigPlayer` GetRedHearts,
GetSoulHearts, GetBlackHearts, GetCoins, GetBombs, GetKeys, CanShoot (config record offsets 0xb0 to 0xd0, name checked first);
`ItemConfig:GetTaggedItems(tags)` (the game's own tag lookup, 0x730b80) and `GetItemsWithCustomTag` (fixes an earlier registration
that took custom tags under the wrong name and never registered); `Isaac.GetCutsceneIdByName`; `Game:IsHardMode`, `Level:HasPhotoDoor`,
`Room` CanPickupGridEntity, CanSpawnObstacleAtPosition, GetBossVictoryJingle; `EntityPlayer` AddLeprosy, AddBoneOrbital,
CanAddCollectibleToInventory, CanUsePill, DropCollectible, DropCollectibleByHistoryIndex, GetBodyMoveDirection,
GetCambionPregnancyLevel, GetEnterPosition, GetFocusEntity, GetGlitchBabySubType, HasPoisonImmunity, IsEntityValidTarget,
IsHeadless, IsInvisible, MorphToCoopGhost, RemoveCollectibleByHistoryIndex, SetActiveVarData, SetControllerIndex, SpawnClot,
TryPreventDeath, VoidHasCollectible; `EntityPickup` GetAlternatePedestal, SetAlternatePedestal, SetForceBlind; `Entity:GiveMinecart`;
`EntityFamiliar` CanBlockProjectiles, GetFollowerPriority; `GridEntityRock:TrySpawnLadder`.
Each call into the game uses a REPENTOGON signature that matches this exe once; the callee's `ret N` was checked against the
argument words. `lab_census_test.lua` ran 41 checks (all pass): the read-only names are called and compared (RNG against the
game's own `Next`, Isaac's and Magdalene's starting hearts, tagged items all carry the tag); names that change the game
(SpawnClot, DropCollectible, TrySpawnLadder, GiveMinecart, ...) are checked by address and arity only and still need a play test.
Not done: `Isaac.GetLoadedModules` (mods get no `package` table, so `_LOADED` is out of reach); `GetCutsceneIdByName` of a mod
cutscene follows its XML node.

Measured with all 95 mods (`.scratch/m8-census`): no mod flipped (6 of 12 REPENTOGON-naming mods still shown; Epiphany missing
114 to 107, Fiend Folio 215 to 210, StageAPI 13 to 12), and the headless `lab_trapdoor` run through floors 1 to 5 shows the same 17
"failed" lines before and after (the Fiend Folio and Throwable Item Library errors; none new).

### Fiend Folio did not finish loading (fixed, two causes in the Host, not the Layer)

Fiend Folio's `main.lua` stopped at line 825 (`FiendFolio.FF` nil), and every later Fiend Folio error followed from it
(main.lua:581, :642, :672, Epiphany's `throwable_item_lib.lua:1187`). Found with a lab that wrapped `include`
(it returned "no file '//mods/fiendfolio-reloaded.../ffscripts/config.lua'" for files that exist, and the game's `include` does not raise):

1. **The Host's file table was full.** It has 65,536 slots (`FS_SLOTS`, `host_shims_fs.c`), and the Mac Build seeds each mod file
   under both mod roots: the 95 mods are 32,702 files, 65,404 entries, plus folders and the game's own files. The last ~4,000 were dropped without a word, Fiend
   Folio's `ffscripts` among them. The Mac Build now has 131,072 slots (the Browser Build keeps 65,536); the boot line
   `[mac] mods: ... 65404 file-table entries seeded` now equals twice the file count.
2. **`content-repentogon/` was never read.** Fiend Folio ships only that folder (REPENTOGON reads it in place of `content/`; the Browser
   Build's packer already lists it as `content/`); this game reads only `content/`, so none of its items existed
   (`Isaac.GetItemIdByName` gave -1, `inaba.lua:143` stopped `main.lua`). With the Layer on, `mac_fs.c` now also lists each
   `content-repentogon/` file as the `content/` file unless `content/` has its own (log line: `153 file(s) of content-repentogon/ listed as content/`). It is
   only Fiend Folio among the 95 mods.

Measured: the headless 95-mod `lab_trapdoor` run through floors 1 to 5 has no Fiend Folio `main.lua` error and none of the five
follow-on "failed" lines (they were 8 lines, 6 of them Fiend Folio's, in `trap-after.log`). The only `ERR:` left is
`! fired's_angels_and_statues` (`AngelStatueMod` is a nil global in that mod's own `main.lua:18`; it was hidden before because
that file was among the dropped ones).

### Batch 2 (`api_batch_two.lua`): 35 names, offsets measured in this build

REPENTOGON's `Entity_Player` offsets are not this build's (the struct drifted by +4 to +36 words along its length), so each field was found by
a diff probe (`lab_b2probe*.lua`: snapshot the player's words, make the game change the value, diff) and checked by the
game's own reaction, then by a read-back check (`lab_batch2_test.lua`, 85 checks, all pass headless):

| name(s) | REPENTOGON offset | here | how it was confirmed |
|---|---|---|---|
| Get/Set Speed, FireDelay, Damage, TearRange, ShotSpeed, Luck `Modifier` | 0x1518.. | 0x1520.. (+8) | writing 5 into each and re-evaluating moved exactly that stat (MoveSpeed 1 to 2, MaxFireDelay 10 to 4.74, Damage 3.5 to 8.5, TearRange 260 to 760, ShotSpeed 1 to 2, Luck 0 to 5); `EvaluateItems` reads them, Init clears them as a block. Replaces the Layer's own approximation in `api_gaps.lua` (now the game's own field, so Experimental Treatment's changes are seen) |
| Get/SetTearDisplacement | 0x156c | 0x1578 (int8) | flips 1 and -1 on every tear |
| Get/SetBombPlaceDelay | 0x1604 | 0x1610 | 0 to 28 when a bomb is placed |
| Get/SetHeadDirectionLockTime, SetHeadDirection | 0x133c / 0x161c | 0x1338 / 0x1628 | the vanilla HeadFrameDelay counts down from the written value and the direction returns at -1; `GetHeadDirection` returns what was set; the head animation name (std::string at 0x1320) is rewritten too, as the game does not derive it |
| GetUrnSouls | 0x1dbc | 0x1dd0 | 0 to 3 across the game's own `AddUrnSouls(3)` (`AddUrnSouls` already existed) |
| GetMarkedTarget | 0x1408 | 0x140c | the pointer holds the TARGET effect (1000, 30) Marked spawns; nil before |
| SetLaserColor | | 0x14c4 | the game's own `LaserColor` property; read back through it |
| IsItemCostumeVisible, IsNullItemCostumeVisible | | calls 0x75d900, 0x75d980 | the game's own checks (`ret 8`); the check compares against `IsCollectibleCostumeVisible` (both false for Sad Onion in the lab: weak) |
| EntityPlayer:GetWeapon(0..4) and `Weapon` GetOwner, Get/SetFireDelay, GetMaxFireDelay, Get/SetCharge, GetWeaponType, Get/SetModifiers, GetNumFired, IsAxisAligned | 0x13d8 | slot array at 0x13dc | the primary weapon is slot 1 and its +4 is the player; type 1 for tears (3 with Technology), max fire delay equals the player's MaxFireDelay |
| EntityKnife Get/SetIsSwinging, Get/SetIsSpinAttack, Is/SetPrismTouched, Is/SetMultidimensionalTouched, SetKnifeVelocity, SetKnifeDistance, GetHitList, InHitList, AddToHitList, RemoveFromHitList | same as REPENTOGON (delta 0) | 0x465, 0x4ea, 0x4e5, 0x4e6, 0x448, 0x444, vector at 0x4d8 | the initialiser clears them as a block; a bone club's 0x465 reads 1 while it swings and 0 after; the hit list is the vector the game's own `GetHitEntities` (0x9612e0) returns. `AddToHitList` past the vector's capacity grows it with the guest allocator (the game's own `free` is the same allocator) and survived 12 entities |

Census (`rgon_census.py` over the lab dump, then the gate): functions implemented 535 to 570, inert 40 to 37 (the three stand-ins that
are now real), missing used by REPENTOGON-naming mods 283 to 256. Per mod (the gate's "missing"): Epiphany 107 to 97, Fiend Folio 210 to 191,
StageAPI 12, External Item Descriptions 14, Unique Items API 2 (unchanged); no mod flipped. The headless trapdoor run through floors
1 to 5 shows no new "failed" line against `trap-after.log` (it has none of Fiend Folio's now, see above).

Approximate or inert, not counted as implemented: the suplex accessors (REPENTOGON's offsets at 0x1f08.. hold a sentinel -100000 and
floats here; the game's own Suplex! changed none of them, so there is no field to place), `Get/SetHitboxParentKnife` (REPENTOGON keeps
it in a side field its own hook fills when a melee weapon makes its hitbox; here a mod's own `Set` is remembered and the game's hitboxes
answer nil; Fiend Folio calls Get in every damage callback and errored without the name).

Not done (names): `Weapon` GetDirection (the direction words read 0 after shooting; not verified while firing), GetMaxCharge and
GetMainEntity (virtual functions; the slot and a float return were not checked), SetHeadLockTime, IsItemAnimFinished,
`Isaac.CreateWeapon`/`DestroyWeapon`; `EntityKnife:FireSplitTear` (REPENTOGON patches the code: L). Player fields not found, still missing:
KeepersSackBonus, Immaculate and Cambion Conception state, Eve's Sumptorium charge, RedStewBonusDuration, PonyCharge, PlanC, bladder/urethra,
PlayerFormCounter (a Guppy-item test showed no change), HeldEntity, RUAWizardTimer, the poop spell queue.

Needs a hand test (not seen in a screenshot): the two scripted screenshots (`lab_b2shot.lua`: Technology with `SetLaserColor`, then
Mom's Knife) were taken but at the wrong frame (the intro text and a tiny tracer line only; `lab_b2shot` logged weapon type 3 and the
knife's fields), and a second headless run after the last edit (`GetHitboxParentKnife`) was stopped when the user's game started.
Steps: start a run with a mod set containing Fiend Folio, open the console (grave), `giveitem c68` (Technology), then
`lua Isaac.GetPlayer(0):SetLaserColor(Color(0,1,0.2,1,0,0,0))` and shoot: the laser should be green. Then `giveitem c114` (Mom's Knife) and hit a
monster with Fiend Folio on: no "GetHitboxParentKnife" line in the log.

### Batch 3 (`api_pocket_item.lua`, `api_batch_three.lua`, `api_minimap.lua`): 19 names, verified 2026-11-19

Added: `EntityPlayer:GetPocketItem` and the `PocketItem` class (GetSlot, GetType; the game's own function 0x00753900, the record is
two words {id, type}); `Weapon` GetMaxCharge, GetMainEntity (vtable slots 6 and 10; the charge comes back in st0), GetDirection (vector
at +0x20); `Level` CanPlaceRoom, CanPlaceRoomAtDoor, FindValidRoomPlacementLocations, GetNeighboringRooms and `RoomDescriptor`
GetNeighboringRooms, GetValidNeighborPlacementLocations (over the game's level-generator helpers; `TryPlaceRoom` is not here: the
game's PlaceRoom signature matches nothing); the global `Minimap` (next section); `ItemPool:ResetCollectible` (the game's own function
0x007340b0).

Checks (headless, `.scratch/m8-batch3`): `lab_batch2_test.lua` 81 PASS 0 FAIL; `lab_batch3_test.lua` 69 PASS 0 FAIL (it had 5 FAIL lines in
the worker's own log from before its last edit; none now); `lab_pocket_test.lua` 31 PASS; `lab_minimap_test.lua` 20 PASS; `lab_pool_test.lua`
8 PASS. The 95-mod `lab_trapdoor` run through floors 1 to 5 (6 levels reached): the log has no "failed" line and one `ERR:` (the mod's own
`! fired's_angels_and_statues` `AngelStatueMod`), the same as `.scratch/m8-batch2/trap-batch2.log`; the other differences are random rooms
and seeds. Epiphany's `throwable_item_lib.lua:727` (`GetPocketItem(...)` returned a bare userdata) did not appear in the batch 2 headless
log either, so that run cannot show it gone: what shows it is `lab_pocket_test.lua` (`GetPocketItem(PillCardSlot.PRIMARY):GetSlot()` is a
number, 0 for an empty slot, so `GetSlot() - 1 == -1` returns) and the hand test below.

Census: functions implemented 570 to 586 (incl. 2 more the census dump misses: `RoomDescriptor`'s two names are installed at the first
player init, after the dump), inert 37 to 39, missing used by REPENTOGON-naming mods 256 to 245. Gate: 9 mods use REPENTOGON names, 6
shown, as before. Per mod (the gate's "missing"): Epiphany 97 to 93, Fiend Folio 191 to 187, External Item Descriptions 14, StageAPI 12,
Unique Items API 2; no mod flipped.

### Writable ItemConfigItem fields (`api_item_fields.lua`, 2026-11-19)

The bug: Fiend Folio's `hotpotatobook.lua:45` (`Isaac.GetItemConfig():GetCollectible(id).ChargeType = ItemConfig.CHARGE_SPECIAL`, also
`bookOfLeviathan.lua`, `creativeAI.lua`, and `.Description`/`.Name` in `briend.lua` and `friend.lua`, `.GfxFileName` in `double_dog.lua`)
failed in `MC_PRE_PLAYER_TRIGGER_ROOM_CLEAR` with "no member named 'ChargeType'".

What the game has (`lab_icfg_probe.lua`): `ItemConfig:GetCollectible/GetTrinket/GetNullItem` hand out a `const Item` userdata. luabridge's
plain `Item` class has a `__propset` table with a setter for all 29 fields (AchievementID, AddBlackHearts, AddBombs, AddCoins,
AddCostumeOnPickup, AddHearts, AddKeys, AddMaxHearts, AddSoulHearts, CacheFlags, ChargeType, ClearEffectsOnRemove, CraftingQuality,
Description, DevilPrice, GfxFileName, Hidden, ID, InitCharge, MaxCharges, MaxCooldown, Name, PassiveCache, PersistentEffect, Quality,
ShopPrice, Special, Tags, Type); the `const Item` class has `__propget` only, so a write through it ends in luabridge's "no member
named". REPENTOGON's source has no per-field setters for ItemConfigItem (its `LuaItemConfig.cpp` adds only the custom-tag functions);
what it does is push the plain `Item` metatable for the items it hands to callbacks, `GetTaggedItems` and `GetItemsWithCustomTag`
(CustomCallbacks.cpp, LuaItemConfig.cpp), and a mod that reaches items from `GetCollectible` is the case the Layer must cover.

What the Layer does: the const metatable gets a `__propset` whose entries take the const userdata, build a plain-class userdata at the
same address (`B.obj`) and call the game's own setter. So the game's own code writes its own record (strings included); the Layer
knows no offset. `lab_icfg_test.lua` (Book of Belial, headless, `.scratch/m8-batch4/test2.log`) writes every field, reads it back through
the vanilla getter and diffs the record's words before and after. The record, in this build, agrees with REPENTOGON's struct for every
field: Type +0x0, Name +0x8 (std::string, 24 bytes), Description +0x20, GfxFileName +0x38, AchievementID +0x50, CacheFlags +0x54,
AddMaxHearts +0x58, AddHearts +0x5c, AddSoulHearts +0x60, AddBlackHearts +0x64, AddBombs +0x68, AddKeys +0x6c, AddCoins +0x70,
MaxCharges +0x74, MaxCooldown +0x78, DevilPrice +0x7c (short), ShopPrice +0x7e (short), ChargeType +0xac, Special +0xb0, PassiveCache
+0xb1, AddCostumeOnPickup +0xb2, Hidden +0xb3, PersistentEffect +0xb4, ClearEffectsOnRemove +0xb5, Tags +0xb8 (64-bit), Quality +0xc0,
CraftingQuality +0xc4; and InitCharge +0xc8 (REPENTOGON has no name for it). `Costume` stays read-only (a getter only, in the game too).
The game uses the written values: `EntityPlayer:GetActiveMaxCharge` follows MaxCharges (3, then 7 after the write, 3 after restoring),
and ChargeType decides the room-clear charge (a Book of Belial with ChargeType 0 gains 1 charge when a room is cleared, with
CHARGE_SPECIAL (2) it gains 0). Lab result PASS, 0 failures. The 95-mod `lab_trapdoor` run through floors 1 to 5
(`.scratch/m8-batch4/trap-batch4.log`): 6 levels, no "failed" line, the one `ERR:` of `! fired's_angels_and_statues` as in
`.scratch/m8-batch3/trap-batch3.log`.

Other REPENTOGON-writable fields, probed in this build (`lab_icfg_probe.lua`): `Entity.Variant` and `SubType` already write; `Options.SFXVolume`
writes; `ProjectileParams.Damage` and `TearParams` fields write. Still refused (not implemented): `Game.Difficulty`, `ItemConfigCard.Weight`
(and its getter), `ItemConfigCostume.SkinColor` (the getter is a bare userdata), `RoomDescriptor.AllowedDoors`. None of these is
written by a mod in the 99-mod Workshop set (grep of `.Difficulty =`, `.Weight =`, `.SkinColor =`, `.AllowedDoors =`): the only
hits are the mods' own tables. `RoomDescriptor` fields such as `Flags` are refused on `Level:GetCurrentRoomDesc()` (a const descriptor,
as in vanilla; REPENTOGON does not change that).

### The global `Minimap` (`api_minimap.lua`)

REPENTOGON's `Minimap` is a plain table of functions that all act on the game's one Minimap and ignore `self` (so `Minimap:GetIconsSprite()`
and `Minimap.GetState()` both work; its setters read argument 1, so only `Minimap.SetState(1)` is valid there; the Layer also accepts the
colon form). Implemented: GetState, SetState, GetHoldTime, SetHoldTime, GetIconsSprite, GetItemIconsSprite. Not implemented (not
registered): GetDisplayedSize and Refresh (no game function found), Get/SetShakeDuration and Get/SetShakeOffset (+0x61c, +0x620 never seen
change).

Where it is, measured, not guessed: REPENTOGON has it at game+0x25444; here the object is at **game+0x25ecc** (the game object's pointer is
at 0x00c71678). `lab_minimap_scan.lua` lists every sprite (ANM2 objects point at themselves at +0x30 and +0x50) in the game object with the
file it loaded: `gfx/ui/minimap1.anm2` at +0x25ee4, `minimap2.anm2` at +0x26044, `mapitemicons.anm2` at +0x261a4, `hudpickups.anm2` at
+0x262b8, `minimap_icons.anm2` at +0x263cc. REPENTOGON's `Minimap` struct puts exactly these four at +0x18, +0x178, +0x2d8, +0x3ec, +0x500, so
all agree on 0x25ee4 - 0x18 = 0x25ecc. (Two more `minimap_icons.anm2` sprites sit elsewhere in the game object, at +0x238e8 and +0x2449c; the
Minimap's own is the one in the struct.) State (+0x0) and hold time (+0x8) were measured with `lab_minimap_state.lua` and scripted Tab
presses: state 0 at rest, 1 while Tab is held with the hold time counting up one a frame (5, 10, ... 30), back to 0 after, 2 after a tap; the
enum is `MinimapState` (NORMAL 0, EXPANDED 1, EXPANDED_OPAQUE 2, from REPENTOGON's `enums_ex.lua`).

`GetIconsSprite` returns the real sprite: its `GetLayer(0):GetSpritesheetPath()` reads `gfx/ui/minimap_icons.png`, and after
`ReplaceSpritesheet(0, <png>)` + `LoadGraphics()` the minimap draws the new sheet. Seen in a screenshot (`lab_minimap_test.lua` with
`ISAAC_LAB_MM_SHOT=sheet`, `ISAAC_LAB_MM_MOVE=1`): with the compass and map shown, the minimap's room icons are a shop key, a crown, a boss
skull and question marks; after replacing the sheet with `gfx/ui/ui_hearts.png` they look unchanged until the next room change, after which
the same cells draw hearts (the game redraws the icons only then, which is how it is with REPENTOGON too, and Fiend Folio replaces the sheet in
`MC_POST_NEW_ROOM`). As a control, the same call on the player's own sprite changes it at once (the player drawn as hearts). The shots are in
`.scratch/m8-batch3/shots/` (`s-before.png`, `move2.png`).

Fiend Folio: `lab_ff_veil.lua` (95 mods) gives the level Curse of the Veil and changes room: Fiend Folio's callback sets
`gfx/ui/minimap_icons_veil.png` on the minimap sprite and the global flag; after the curse is removed and a room change it puts the original
sheet back; a third change from a hand-set state also resets it. No "failed" line, no `Minimap` error. `Minimap.GetState()` (Fiend Folio's
`heat.lua:94`) works.

Hidden by `--luadebug`: with `LUADBG=true` in `run.sh` the game's `require` of a mod's own files fails ("module ... not found"), so most mods stop
at their `main.lua`; don't use it for mod runs.

### External Item Descriptions (`api_eid.lua`, 2026-11-19)

All 14 names EID was missing are in: `EntityPlayer` GetPlayerFormCounter, GetBagOfCraftingContent/Slot/Output/OutputItemPool,
`PersistentGameData:IsItemInCollection`, `RoomDescriptor:GetEntitiesSaveState` with the `EntitiesSaveStateVector` and
`EntitySaveState` getters, the global `ProceduralItemManager` (GetProceduralItem, GetProceduralItemCount), `ProceduralItem` and
`ProceduralEffect` (condition/action type and property, trigger chance, score). Every offset is in the header of the file and was measured in
v1.9.7.17 (they differ from REPENTOGON's .12: form counters +0x17dc, bag +0x1f44/0x1f4c/0x1f50, procedural list at game +0x67758; the
ProceduralItem record, the effect record and SavedEntities +0x74 are laid out as REPENTOGON has them). Not done: `CreateProceduralItem`
(no signature match in this exe), the setters.
Verified headless (`lab_eid_test.lua`, support folder without mods, `.scratch/eid/test3.log`): 92 PASS, 0 FAIL. The game is the other side of
each check: its own HasPlayerForm (counter > 2) against the form counters, `TryAddToBagOfCrafting` and `CalculateBagOfCraftingOutput`
(0x007d5170, 0x007d9d00) against the bag names (output item equals the game's), `AddToCollection` against IsItemInCollection, spawned pickups
against the saved states (seeds, positions, shop price in I3), and 17 TM Trainer procedural items against the player's stats after
`AddCollectible(-1 - n)`: damage 0.2717 = player delta 0.2717; speed 0.616 -> 0.1232 (x0.2), range 1.18 -> 70.8 (x60), fire delay 0.42 -> -1.47
(the game converts fire delay to tears), so each stat sits at the field its name says; luck and shot speed were 0 in all 17 items (same
layout as REPENTOGON, not seen non-zero). Gate: `layer-gate.json` of the 95-mod census run has "external item descriptions is shown
REPENTOGON (30 name(s) used, 0 missing)". Windowed screenshot (`lab_eid_shot.lua`, `.scratch/eid/shot1300.png`): EID's box over the Bag of
Crafting pedestal shows the bag's eight pickups. The 95-mod `lab_trapdoor` run through floors 1 to 5 (7 levels reached, `.scratch/eid/trap.log`) has no "failed" line and the one known
`ERR:` (`! fired's_angels_and_statues`, `AngelStatueMod`).

### Custom Stage API (`api_stageapi.lua`, 2026-11-19): names done, the mod is still not Shown

Done and measured in v1.9.7.17 (offsets in the file header): `Room` SetBackdropType (the game's `Backdrop::Init` 0x0093d960), Get/SetWaterAmount (+0x7240),
Get/SetWaterCurrent (+0x7460), Get/SetRail (+0x746c, one byte per grid index); `RoomDescriptor` Get/SetTaintedKeeperCoinSpawns (short at +0x52, the
game's own coin-drop code reads it), GetDecoSaveState, GetGridEntitiesSaveState (with `Clear` and the `GridEntityDesc` fields StageAPI zeroes),
GetEntitiesSaveState (+ `Clear`; `api_eid.lua` provides the richer vector class, the two share it); `EntityPickup:InitFlipState` (builds the same
flip record the game's spawn code builds; the game's own `TryFlip` then morphs into it); `MC_PRE_BOSS_SELECT` (hook slot 174, `BossPool::GetBossId`).
Verified: `lab_stageapi_test.lua` 55 PASS, 0 FAIL (headless, `.scratch/stageapi/test5.log`). Windowed screenshots (`lab_stageapi_shot.lua`,
`.scratch/stageapi/shots/`): `SetBackdropType(12, 1)` turns the Basement's brick walls into red-veined flesh walls, `SetWaterAmount(1)` darkens the floor
and puts a reflection of Isaac under him, and 0 puts the room back (`SetBackdropType(orig, 1)` re-rolls the decoration). `lab_stageapi_boss.lua`: the callback
fires from level generation (`lab_stageapi_boss: CALL 1: bossId=56 pool=nil levelType=1 levelVariant=0 -> answer 19`), and the floor's boss room then has
subtype 19 (without an answer the game's own pick 2 is the subtype). The Tainted Keeper coin count is a round trip plus the disassembly only (no
Tainted Keeper run was played).
**Hook 174 is in the lift but only in a binary built from it**: `.scratch/stageapi/mac-out/isaac-mac` (a copy of `output/mac`, built with
`build_native.py --out`); `output/mac/isaac-mac` (what `play.sh` runs) was not rebuilt, so there the callback never fires. `hook_table.py --write`
only works with the prebuilt reg/sig outputs; the row was added by hand to `lift_patches.py` `RGON_HOOKS` and `hook_slots.lua` (the table's own check of
row 174 says ok, ret 0xc), and `apply_wrap_patches` (about 2 minutes: it scans all 112 lifted files) wrapped `sub_00422830` in `lifted_000.c`.
**Gate, still "not shown"**: the census run (`lab_census.lua`, 95 mods, `.scratch/stageapi/census.log`) lists one missing name, `MC_CONSOLE_AUTOCOMPLETE`: it only
fires from REPENTOGON's ImGui console, and the game's own console has no completion to hook, so it is not registered (it would be a lie). It is not just
a gate nuisance: with `ISAAC_RGON_ALLOW=stageapi15_1348031964` (`.scratch/stageapi/trap-allow.log`) StageAPI fails at load (`extrarooms.lua:115`,
`RoomConfig.GetRoomByStageTypeAndVariant` is nil; the Layer only has a nil-returning `RoomConfigHolder` stand-in), then `StageAPI.GetCurrentLevelMap` is nil
and its `MC_POST_UPDATE` fails 1,335 times in the run. StageAPI's other `if REPENTOGON` branches (33 sites: minecarts, entity config, boss intro, HUD
callbacks, `RoomConfig.AddRooms`/virtual room sets) are not looked at. So Shown is a project of its own: a real `RoomConfig.GetRoomByStageTypeAndVariant`
(the game's room tables) first, then those branches, then the autocomplete callback last.
Regression (95 mods, `lab_trapdoor`, floors 1 to 5, `.scratch/stageapi/trap.log`): 5 levels reached, no "failed" line, the one known `ERR:`
(`! fired's_angels_and_statues`); StageAPI-dependent mods load and floors change as before.

### Batch B1: render quads (`api_misc.lua`, 2026-11-19)

`DestinationQuad` and `SourceQuad` were already in the Layer as value types, but only the class name was registered, so the census counted their
methods missing. Now registered, each checked by `lab_quad_test.lua` (45 PASS, 0 FAIL): the four corner getters and setters, `Copy`,
`Translate`, `Flip` (both classes) and `SourceQuad:IsUVSpace`. Two behaviours were wrong and are fixed: `Flip` with an argument left out means
true (REPENTOGON's default: `q:Flip()` flips both axes; before, it did nothing), and `SourceQuad:Copy` kept no UV flag. `Flip`, `Translate` and
the setters mutate and return nothing; `Copy` and the getters hand out new objects. Census: functions implemented 628 to 641; the REPENTOGON
mods' missing names 216 to 208; gate lists lose Copy, the four getters and Translate. Regression run (95 mods, floors 1 to 5): same as before.
Still not registered (work, but not checked against REPENTOGON's maths): Scale, Rotate, Shear, ApplyMatrix; ConvertToPixelSpace/UVSpace need an
image. No screenshot: the Layer has no render path that consumes a quad (`Renderer`/`Image` are accepted, not supported), so a flipped quad
cannot be drawn yet.

#### `RoomConfig.GetRoomByStageTypeAndVariant` (`api_roomconfig.lua`, 2026-11-19, chunk A1)

Real now (it was a nil-returning `RoomConfigHolder` stand-in), on `RoomConfig` and `RoomConfigHolder`. It calls the game's own lookup
(`__thiscall`, `ret 0x10`) at `0x0082c720` with `this` = game+0x18910; REPENTOGON's signature for it matches nothing in this exe (the
function was found as the one just before `RoomConfig::GetRandomRoom` at `0x0082c7d0`, whose signature matches once, and read from its
disassembly: stage record = stage * 0xc4, room set = +0x64 + mode * 0x2c, loaded flag +0x28 (the function loads the set itself), room array
+0x18, count +0x1c, 0x5c bytes a room, matched on type +8 and variant +0xc, 0 and a log line when not found). The fourth argument is
REPENTOGON's `Mode` (0 normal, 1 greed, -1 the current game's), not a difficulty; anything outside -1..1 is answered as -1 (REPENTOGON
passes -2 on, which the game's lookup would index before the stage record). Argument errors as REPENTOGON's (fewer than 3 arguments,
stage outside 0..36, type outside 1..29). The result is the game's own RoomConfigRoom class: the layer asks the game's `RoomDescriptor.Data`
getter through a throw-away descriptor-shaped block whose word at +0x10 (the Data pointer: found by scanning a real descriptor) holds the
room, so it also works at mod load time, before any level has a descriptor with Data (Custom Stage API asks then, `extrarooms.lua:115`).
`T.roomconfig_room(address)` hands that wrapper to other Layer parts. Not done: REPENTOGON's virtual room sets (`RoomConfig.AddRooms`,
custom rooms found by this call), so a room a mod adds is not found. Verified: `lab_roomconfig_test.lua` 53 PASS, 0 FAIL (headless,
`.scratch/a1/lab3.log`): on Basement I, Caves I, Depths I and Flooded Caves I every room's own StageID/Type/Variant/Mode gives the same address
the game holds for the descriptor, with equal Type, Variant, Subtype, Shape, Name, Weight, Difficulty, Doors and spawn count, the same class,
and `RoomConfigHolder` the same room; unknown and negative variants are nil; the call also works at load time.

#### `RoomConfig.GetRandomRoom` and `RoomTransition.StartBossIntro` (`api_roomconfig.lua`, `api_extra.lua`, 2026-11-19, chunk "rooms")

Both are the game's own functions, reached by the Layer. `RoomConfig.GetRandomRoom(seed, reduceWeight, stage, type, shape = 13, minVariant = 0,
maxVariant = -1, minDifficulty = 0, maxDifficulty = 10, requiredDoors = 0, subtype = -1, mode = -1)` calls `RoomConfig::GetRandomRoom` at
`0x0082c7d0` (`__thiscall`, `this` = game+0x18910, twelve stack words, `ret 0x30` at `0x0082cdc6`; REPENTOGON's signature matches here).
`requiredDoors` is a pointer to a mask in the game (a four-byte block is made), `reduceWeight` a byte. Argument checks as REPENTOGON's (stage
0..36 except 18..26, type 1..29, shape 1..13, max variant not below min, max difficulty not below min, mask and subtype not negative,
mode -1..1) plus one of ours: seed 0 is a Lua error because the game stops on it (`int3` at `0x0082c896`). The result is the game's own
RoomConfigRoom (`T.roomconfig_room`); nil when nothing matches. Boss rooms of the floors live in stage 0 (`StbType.SPECIAL_ROOMS`), not in
the stage's own record. Lab `lab_roomrandom_test.lua`: 43 PASS, 0 FAIL (`.scratch/rooms/lab4.log`): type/shape/stage/variant/difficulty/doors
filters hold over 40 seeds on eight stage/type cases, the same seed gives the same room, `reduceWeight` true picks the same room as false and
sets its weight to 0.1 (false changes nothing). Not done: REPENTOGON's custom (AddRooms) sets.

`RoomTransition.StartBossIntro(boss1, boss2 = 0)` calls the game's function at `0x0082f1c0` (`__thiscall` on game+0x1b83c, two stack words,
`ret 8` at `0x0083093e`; it stores boss1 at +0x238 and sets the mode, +0, to 2 at the end). The word at +0x14 (the room the transition
targets, looked up as a descriptor inside) is first set to the current room index, as REPENTOGON sets the starting room, so a stale value
cannot crash it. The existing `MC_POST_BOSS_INTRO_SHOW` hook (slot 24) fires from the direct call. Lab `lab_bossintro_test.lua`: 17 PASS,
0 FAIL (`.scratch/rooms/boss4.log`): mode 2, `IsRenderingBossIntro`, the callback gets `(MONSTRO, 0)`, the intro ends by itself after 199
render frames, and the path StageAPI takes (`StageAPI.PlayBossAnimation` on a Fiend Folio boss with `UseRepentogonBossIntro`) runs
StageAPI's own callback on `GetVersusScreenSprite` (`Scene`, `ReplaceSpritesheet`, `LoadGraphics`). Windowed screenshots of both intros
(`.scratch/rooms/sa_460.png`: "Isaac VS Mo..." with Mo's own portrait) look right. A stale stand-in `RT.StartBossIntro` in `api_menus.lua`
was removed and `lint.py` forbids a second one.

#### `BlendMode.Flag1..Flag4` (`api_gaps.lua`, 2026-11-19, chunk A3a)

With the allow flag, Custom Stage API's `MC_POST_NEW_ROOM` failed at `roomgfx.lua:253` (`no member named 'Flag1'`, 4 times in a floors 1 to 5 run).
REPENTOGON keeps `Flag1..Flag4` as deprecated names for the four factors (`RGBSourceFactor`, `RGBDestinationFactor`, `AlphaSourceFactor`,
`AlphaDestinationFactor`, in that order), readable and writable. Added as aliases of the four ints already measured at layer state +0x78 (the
default read back is `(0,1,7,1,7)`); same range check, `Equation` untouched. `lab_stageapi_blend.lua`: 7 PASS, 0 FAIL (default read, alias equals the
named factors, a write reaches the layer on a fresh read, invalid factor and `Flag5` are errors). Allow-flag run: 0 `Flag1` errors, no new
failed or `ERR:` line against the plain run. Survey of the other StageAPI branches: `notes/a3-stageapi.md`: only `RoomTransition.StartBossIntro`
(inert, opt-in `UseRepentogonBossIntro` only) and `MC_CONSOLE_AUTOCOMPLETE` (held) are not real; most branches are not reached by a vanilla floor run.

**Decision (user, 2026-11-19): `MC_CONSOLE_AUTOCOMPLETE` is ignored by the gate, and StageAPI is shown REPENTOGON.** Only
REPENTOGON's own ImGui console fires it, and the game's console has no completion to hook, so it stays unregistered (registering it
would be a name that never fires). `rgon.lua`'s `GATE_NOT_REQUIRED` lists it: a mod using it is not counted as lacking it, and
loses only console completion (StageAPI's commands still run through `MC_EXECUTE_CMD`). StageAPI's REPENTOGON branches were checked
first with `ISAAC_RGON_ALLOW=stageapi15_1348031964`: the 95-mod `lab_trapdoor` run through floors 1 to 5 shows nothing new, and
`lab_stageapi_floors.lua` enters Catacombs, Fiend Folio's Lab, Stony Level and MorbusReal (24 PASS, screenshots). Only names that
come from REPENTOGON's own windows belong in `GATE_NOT_REQUIRED`; a game-side callback that is merely missing never does.

### Batch B2: hit shapes (`api_shapes.lua`, 2026-11-19)

`Capsule` (constructor and getters) is a Lua value type. `DebugRenderer.Get` and `Shape` (`Capsule`, `Circle`, `GetTimeout`, `SetTimeout`) are accepted, not supported: they record and draw nothing, so they are left out of `REPENTOGON.Implemented` (lint rule `rule_inert_not_implemented`).
Checked by `lab_shape_test.lua` (43 PASS, 0 FAIL). Registered: `Capsule.Capsule`, `GetPosition`, `GetDirection`, `GetF1`, `GetF2`, `GetVec2`,
`GetVec3` (and the new names `GetStartPoint`, `GetEndPoint`, `GetSize`, `GetSizeDifference`), and `Isaac.FindInCapsule`. The field meanings are read from how the mods use them, not measured in the
game (a game capsule cannot be built from Lua here): `GetVec2`/`GetVec3` the two ends, `GetPosition` the middle, `GetDirection` the unit vector
start to end (zero for a point), `GetF1` the radius, `GetF2` the half length. `Capsule(start, end, radius)` is exact by that reading;
`Capsule(position, multiplier, rotation, size)` (radius `size * min(multiplier)`, half length `size * (max - min)`, rotation in degrees) is
inferred: it matches the one four-argument caller (FiendFolio `palinsopia.lua:65`, `Capsule(pos, Vector.One, 0, rad)`, a circle) and is unchecked for
non-uniform multipliers or rotation. `Isaac.FindInCapsule` does not call the game's `QueryCapsule`: it treats an entity as a circle of
`Size * max(SizeMulti)` at its position (`FindInRadius` first), which is exact for round hitboxes. Left out: `Capsule:Collide` (the game's function
also writes a hit location; its signature has to be matched in the executable), `Entity:GetCollisionCapsule`/`GetNullCapsule`/`GetDebugShape`
(need the game's capsule builders, so an engine call). Census: functions implemented 641 to 654 (run with the other worker's tree, so some of
the 13 may be theirs); constructors 3 to 4; gate lists lose `Capsule` (Epiphany, FiendFolio), `DebugRenderer`, `GetF1`, `GetF2`, `GetPosition`,
`GetVec2`, `GetVec3` (FiendFolio). Regression run (95 mods, floors 1 to 5): same six floor changes and the same failed and `ERR:` lines.
No screenshot: shapes are not drawn.

### Batch B3: small state getters (`api_state_getters.lua`, 2026-11-19)

Eleven census names (plus three trivial neighbours) done, every field measured in this build (`lab_state_probe.lua`: diffs while the game changed
the state, the sprites inside the structure, and the game's own code found with `pequery.py fieldrefs`). Checked by `lab_state_test.lua`
(53 PASS, 0 FAIL, headless with the 95 mods; Escape twice in the first room, a Delirium spawned, the Death Certificate used).

| Name | Where | How measured |
|---|---|---|
| `Game.IsPauseMenuOpen`, `Game.GetPauseMenuState`, `PauseMenu.GetState/SetState` | pause screen at game+0x23a74 (REPENTOGON 0x2363c, +0x438), state at +0 | the only word that went 0 to 1 while Escape held the pause menu open and back to 0 after; its four sprites sit at +0x2c, +0x140, +0x254, +0x368 (self pointers). State 1 open, 2 options. `IsPauseMenuOpen` is `state > 0`; it differs from `Game:IsPaused()`, which is also true for the game's first frames and during a room transition (the lab logs both) |
| `Game.GetGenericPrompt`, `GenericPrompt.IsActive` | prompt at game+0xa88 past REPENTOGON's 0x25cf0 = 0x26778, the same shift the minimap and frame counter have; first member is its ANM2 | self pointers at +0x30/+0x50 there; `IsActive` is the game's own function 0x00838210 (unique signature match, `cmp [ecx+0x34],0 / cmp byte [ecx+0x44],0`, bool in AL); the lab calls it on a block of its own with those two fields set and cleared |
| `DailyChallengeMenu.GetState/SetState` | manager+0xdd4 (REPENTOGON 0xdd0, +4 like the character menu), state at +0 | the manager's render calls `Menu_DailyChallenge::Render` 0x008cfd40 with `lea ecx,[esi+0xdd4]`; that function reads its position at +0xc/+0x10 and `cmp [ebx],0` for the state; the sprite at +0x14 has its self pointers; idle value 0, round trip checked. Not seen: the game moving the state itself, or reacting to it on screen (the daily menu cannot be reached by script: the "Lost connection to Steam" popup returns on every menu and `SetActiveMenu` only sets a byte) |
| `EntityDelirium.BossType`, `BossVariant`, `Cycle`, `TransformationTimer` | on the NPC (`ToDelirium` answers with it): +0xbc8, +0xbcc, +0xbe0 (u32), +0xbd0 (u16): REPENTOGON's offsets, unchanged | Delirium's AI function 0x004a6500 reads and writes boss type and variant as a pair (compares with 0x111 and 0xa), the timer with word instructions, the cycle with `and 0x1ffffff`; on a spawned Delirium the type reads 412, the variant 0, the timer counts down one a frame and is reloaded at 0 (a timer set to 3 was reloaded 13 frames later: the game acted on it). Other NPCs answer nil, and setting is an error |
| `RoomDescriptor.GetDimension` | not a field: the level's lists answer which dimension holds the descriptor (api_more.lua) | it was already installed, but only at the first player init, so the census (a menu-time run) counted it missing; the global's class table gets it at load too. Checked: 0 in the start room, 2 after the Death Certificate (`lab_state_test.lua`). A slot with no room in it (the empty descriptor `GetRoomByIdx(84, 1)` gives on a normal floor) answers 0 |

`Entity:ToDelirium` now also answers for a Delirium that has transformed into a boss (its own type and variant equal the boss fields, as REPENTOGON's
rule), not only for type 412. Not done: `GenericPrompt`'s constructor and other functions (the object size 0x134 and the game's constructor are not
checked), `PauseMenu`/`DailyChallengeMenu` `Get/SetSelectedElement` and the sprite getters, Delirium's other fields (`AttackID`, `Angle`,
`RemainingAttacks`, `StateD`: their bit layout was not worked out), a transformed Delirium (the room spawn never changed type in the lab, so
`BossType` different from 412 was not seen). The mods that name these (the census heuristic matches method names, so `GenericPrompt.IsActive` and
`PauseMenu.GetState` are listed for Epiphany, Fiend Folio and the music mod without those mods calling the class) lose
`IsPauseMenuOpen` from the gate lists of Epiphany and Fiend Folio; Shown flags are unchanged. Census: functions implemented 649 to 658, variables 6 to 10,
missing and used by REPENTOGON mods 202 to 195 (variables 4 to 0). Regression run (95 mods, floors 1 to 5): same six floor changes, same failed
and `ERR:` lines. Screenshot: `PauseMenu.SetState(2)` in a run made the game draw its Options page (`.scratch/b3/shots/pause_2.png`), and
`SetState(0)` took it away (`pause_3.png`).

### Batch B4: MC_MENU_INPUT_ACTION and MC_PRE/POST_GLOWING_HOURGLASS_LOAD (hooks 175-178, 2026-11-19)

Both callbacks fire for real now, and Unique Items API is Shown (gate: hidden for exactly these two names before, `used`/`missing` 0 after).
- **Glowing Hourglass load.** Slot 21 `Game::RestoreState` 0x006bb790 was a false signature match (it is slot 25 of the NPC vtable, pops 0; it saw
  three calls per room entry with 0xf60-spaced "states", never the hourglass). The real function is 0x006f8140 (`ret 8`, one `ret`, found from the
  hourglass use 0x00706020, which calls it at 0x00706349 with `push 0; push state+4; ecx = game`; two more callers 0x0090e305, 0x00959b21):
  new slot 178 `Game::RestoreStateReal`, checked by `hook_table.py` (ret 0x8 ok). `MC_PRE/POST_GLOWING_HOURGLASS_LOAD` get the slot (0/1) of the
  hourglass state, as before. Slot 21 stays (nothing listens).
- **Menu input.** The game's input accessors (`Input::IsActionPressed` 0x00956110, `IsActionTriggered` 0x009561e0, `GetActionValue` 0x009562d0, stdcall
  `ret 0x10`, all ten `ret`s checked) call the Lua caller (0x00864c30/0x00864d80, then `CallInputAction` 0x00874910) only while the Manager's state
  (`[0x00c7169c]+8`) is 2, a run. The menu is state 1, so REPENTOGON's swap of the callback id inside `CallInputAction` (slot 173) never runs in
  this build; slot 173 is unused now and its handler is gone. Slots 175-177 hook the three accessors: outside state 2 the callback
  runs with `(nil, hook, action)`, param = hook, and an answer replaces the query (boolean for hooks 0 and 1, boolean or float for hook 2).
  Seen: the title menu's own queries reach it (hook 1: actions 14, 16, 17, 20, 21 about 440 times each in 600 frames).
  **Not covered:** the menu's CONFIRM/BACK queries that sit inline in other functions (0x0095e2ae, 0x0095e309: the same `state == 2` guard), and the
  float answer reaching XMM0 is not observed (`B.call` does not return XMM0); the callback is seen to run for value queries.
- Lab `lab_b4_cb.lua`: 9 PASS 0 FAIL (`.scratch/b4/lab4.log`): hourglass save/load callbacks fire (load once per use, args 0), the use takes the player
  back to the first room; the callback fires in the menu, its `true` replaces `IsActionTriggered(MENUUP)` (called from the lab through the accessor) and
  MENUDOWN stays 0. Headless M1 3000 frames: guard intact. Census: callbacks 136 to 139, functions 658; gate shown 7 to 8.
  Epiphany's save manager now gets its `MC_MENU_INPUT_ACTION` (it marks the menu active), which reached `SaveMenu.GetDeletePopupSprite` (nil, a Lua
  error): it returns an empty sprite now.
- Binary: `.scratch/b4/mac-out/isaac-mac`; `output/mac/isaac-mac` (play) does not have hooks 175-178 until it is rebuilt from the lifted output.

### PRE_x_COLLISION table answers and MC_USE_PILL's colour (`api_hooks.lua`, 2026-11-19)

The game reads any answer from its PRE_x_COLLISION callbacks that is not nil as a boolean, so a table (Custom Health API
answers `{}` for a vanilla heart) counted as "no collision": red, soul and black hearts could not be picked up. The
engine-runner wrapper at the end of `api_hooks.lua` now follows REPENTOGON for all eight classes: a boolean skips the game's
collision code and is the result; `{ Collide = c, SkipCollisionEffects = s }`: `Collide` makes the result `not c`, `s = true`
skips the game's code, and without `s` the game's code runs and a `Collide` answer replaces its result afterwards (the
`HandleCollision` after handler; POST runs first). Anything else is nil. MC_USE_PILL gets the pill colour as its 4th argument:
the colour `Player::UsePill` was called with is kept on a stack and added by the engine-runner wrapper. Lab:
`lab_pickup_test.lua` (hearts with the 95 mods, every answer form on a coin, the pill colour; 29 PASS, was 11 FAIL).

### Familiar weapon, status copy, marked-target aim (worker familiar)

- `EntityFamiliar:GetWeapon()`: the Weapon pointer is at familiar +0xdc4 (REPENTOGON 0xdb0 + 0x14), found by scanning a familiar's words for a
  pointer whose target's +4 (owner) is the familiar. Only Incubus (80), Twisted Pair (235) and variant 76 have one among 30 familiars tried;
  the rest hold 0 and get nil, as in REPENTOGON. Epiphany's `attack_util.lua:74/81` no longer errors (it errored 4,680 times in one play).
- `Entity:CopyStatusEffects(target = nil, overwrite = false)`: the game has its own copy (0x006a8a20, `ret`, parent to child chain, only for
  segmented-boss types) with the exact field list and flag mask (0x015d8000:21001fe0) and the boss-cooldown test; all offsets agree with
  `api_objects.lua`. Done in Lua from that list, plus the colour-params vector (+0x1ac, 0x3c-byte entries, shared priority-255 entries
  are pushed, grown with the game's allocator). The lab runs the game's own function on one Larry Jr. pair and ours on another: same result.
  Not like REPENTOGON: its non-overwrite flag merge ORs every differing bit (its two XORs cancel); here only the status bits move.
- `EntityFamiliar:TryAimAtMarkedTarget(aim, direction, target)`: the next missing name behind `GetWeapon` in Epiphany (777 errors in a lab
  run). Game function 0x006146e0 (`ret 0xc`; aim Vector*, int*, target Vector*; bool in al), REPENTOGON's two return shapes.
- Lab `lab_familiar_test.lua`: 40 PASS 0 FAIL (`.scratch/familiar/lab.log`). Probe: `lab_familiar_probe.lua`. Census 663 functions (was 658),
  callbacks 139, 9 mods shown; trap: same as base.

### The gate matches names with their class (worker gateclass, 2026-11-19)

The gate used to match a mod's use by the bare method name: `EntityFamiliar.GetWeapon` counted as present because `EntityPlayer.GetWeapon` existed.
Now (`gate_weigh` in `rgon.lua`):
- **Scan** (`mods.mjs` `receiverHint`, ported in `ModScan.swift`, scanner version 2; Tests/run.sh checks both byte for byte): every call name is
  written with what the code shows of its receiver: `Name.var` for `var:Name(`, `Name!callee` for `callee(...):Name(`, bare `Name` otherwise.
- **Tables** (`gate_names.lua`, from `tools/gate_names.py`; reads REPENTOGON from `REPENTOGON_DIR`, `third_party/REPENTOGON` or `../REPENTOGON`):
  `pairs` (`Name=Class,Class`: every class REPENTOGON registers a REPENTOGON-only name on, from its docs and C++ registrations), `vpairs` (names the
  game also has, REPENTOGON adds on another class; a short hand list `VANILLA_NAME_NEW`, because the docs list some vanilla methods as additions:
  `EntityPickup:CanReroll` is the game's own), and the inference tables shared with `rgon_backlog.py` (call results, constructors, variable names, parents).
- **Rule** (same as the backlog tool): the class is a class name (`Isaac.X`), a call result (`Game():GetRoom():X`, `x:ToNPC():X`), or a variable named like
  its class (`player`, `bossPortraitLayer`, `entitySaveState`); anything else keeps every class that registers the name. A use is missing when the
  layer lacks `Class.Name` on any candidate (Entity.X covers EntityPlayer). A name the game also has counts only where the receiver is known
  (`GetWeapon.familiar`). `REPENTOGON.Extras.Gate.mods[mod].missing_on` lists the classes.
- Class spellings: docs/C++ names that differ from the Layer's are mapped in `CLASS_ALIAS` (`EntitiesSaveState` is `EntitySaveState`).
- Test: `lab_gate_class_test.lua` (first room; needs the scan). Boot cost of the gate, 95 mods: about 30 to 50 ms (was about 8 ms; the scan file is 3 times larger).
- Effect on the 12 mods that use REPENTOGON: no change of shown/not shown (a 13th, MusicPicker, is new in the scan and not shown: it lacks
  two callbacks). Fiend Folio's missing names went from 154 to 163 and Epiphany's from 82 to 91, although the Layer gained about 20 functions
  in between (census 690 -> 711 implemented): names that exist on another class and were counted as present (the backlog tool lists 171 and 88).
  Unknown receivers that are really a game class the tables cannot name still make a superset; a mod can be hidden by such a name until the
  Layer has it on every candidate or the inference learns the variable (extend `VAR_SUFFIX` / `RET_EXTRA` in `gate_names.py`).

### Entity classes (`api_npc.lua`, lab `lab_entity_classes.lua`, 2026-11-19)

Entity:GetCollisionCapsule / AddMagnetized / TryThrow / GetEntityConfigEntity, EntityFamiliar:IsCharmed / GetItemConfig / GetRandomWisp /
UpdateDirtColor, EntityNPC:Get/SetShieldStrength / GetDarkRedChampionRegenTimer / ApplyTearflagEffects / TrySplit / UpdatePickupGhosts /
ReplaceSpritesheet, EntityBomb:GetHeight / SetHeight / UpdateDirtColor, EntityConfigEntity:GetShadowSize / GetEntityTags / HasEntityTags,
Capsule:Collide. Every address and offset is in the header of `api_npc.lua`. Lab: 99 PASS, 0 FAIL. Lessons from the lab:
- A `TakeDamage` is applied on the next update: the hit points are unchanged right after the call (the shield test reads them two frames later).
- `Kill()` on a dark red champion leaves its hit points up and the regen counter at 0; a lethal hit (hit points below 0) starts the counter (+2 a frame).
- The variant-loop bombs of a lab explode about 100 frames after spawn and kill everything near: remove them before later tests.
- `ri32((call(..)))` reads memory at eax. `GetRandomWisp` raised a garbled error because of it; lint rule `read-of-call-result`.
Not registered: `Entity:GetNullCapsule` (0x006b6c20 is understood, but no null layer of size > 0 was found to prove it); ThrowMaggot(AtPos), NPC:GetLootList,
Shape.Capsule (see the header). `EntityNPC:ReplaceSpritesheet` is the plain case: it does not append the champion/stage suffix.

### Tears, lasers, knives, effects, projectiles (`api_tears_more.lua`, lab `lab_tears_more.lua`, 2026-11-19)

TearParams Mass/Knockback/SpeedMultiplier and TearDisplacement (+0x54, 0x58, 0x5c, 0x60, as REPENTOGON); EntityTear SetMultidimensionalTouched,
In/Add/RemoveFromHitList, Set/GetBackdropType, ResetSpriteScale(force) (writes -1 to the scale animation number 0x45c, which the game's
0x00677780 compares), SetInitSound; EntityLaser Get/SetShrink, Get/SetDisableFollowParent, GetOneHit, Get/SetTimeout, SetScale/ResetSpriteScale
(game function 0x0097fc10), RotateToAngle (0x0096f470, xmm1/xmm2), RecalculateSamplesNextUpdate (empties the four sample vectors at once),
hit list, SetMultidimensionalTouched; EntityKnife InitHomingPath (0x00961320, ret 0xc, path offset in xmm3); EntityProjectile Deflect
(Reflect 0x00652fa0, ret 8); EntityEffect Get/Set/Add/Clear/HasTearFlags (+ Aquarius aliases; variants 54, 113, 126, 167, variant data at 0x420) and
the static CreateLight; FireSplitTear on tear/laser/knife written in Lua (fires MC_POST_FIRE_SPLIT_TEAR for its own calls).
- Init sound: the tear's first Update runs init_tear (0x00666d40), which plays the fire sound unless byte 0x7ea is set; the laser's is byte 0x518.
  SetInitSound(SOUND_NULL) sets that byte; another sound sets it and plays the chosen one at the entity's first POST_*_UPDATE.
- Deferred: `EntityKnife:GetHitboxParentKnife` (a bone club's hitbox knife, SubType 4, has the player as Parent and byte 0x4e7 = 0: the link needs
  an engine hook in update_bone_swing), `EntityEffect:CreateLootPreview`/`GetGridEntityDesc` (no user), laser `HomingType` (the game has it).
- Backlog tool: `TearFlagsLib.FireSplitTear` (Fiend Folio's own library) and `Mod.Room():SetBackdropType` are no longer counted against these classes.

### CB-b: shop, pickups, devil and planetarium chances (hooks 193-204, lab `lab_cb_b.lua`, 2026-11-19)

Needs an engine build. Callbacks real (10): MC_POST_PICKUP_SHOP_PURCHASE 1062, MC_PRE_PICKUP_VOIDED 1265, MC_PRE_PICKUP_COMPOSTED 1267, MC_PRE_PICKUP_UPDATE_GHOST_PICKUPS 1335, MC_PRE_DEVIL_APPLY_ITEMS 1130 / SPECIAL_ITEMS 1132 / MC_POST_DEVIL_CALCULATE 1133 and MC_PRE_PLANETARIUM_APPLY_ITEMS 1113 / APPLY_TELESCOPE_LENS 1114 / MC_POST_PLANETARIUM_CALCULATE 1115.

| Callback | Site (v1.9.7.17) | Notes |
|---|---|---|
| 1062 | `Entity_Pickup::TriggerShopPurchase` 0x006e84d0, ret 8 (slot 193) | `(pickup, player, moneySpent)`, param = variant. REPENTOGON's signature matches 0x00918d00 here, which is wrong. Lab calls the game function directly; the natural collision path calls the same address. |
| 1265 | Void: insn 0x005be7e1 in the item-use giant (slot 194); Black Rune: 0x007b451c in `UseCard` (slot 195) | `(pickup, isBlackRune)`; `false` keeps the pedestal. Void's case is found from collectible 477 through the byte table 0x005c7760 and jump table 0x005c7598 (lint rule `insn-hook-giant-entry`). |
| 1267 | Compost: insn 0x005bde0c in the giant (slot 196) | `(pickup)`; `false` keeps the pickup. |
| 1335 | `UpdatePickupGhosts` 0x006e17c0, insn 0x006e17f0 (slot 204) | `(pickup)`; `false` removes the ghost (lab: proven). `true` jumps past the game's own Guppy's Eye test; the lab could not show a ghost for a chest without Guppy's Eye (the loot list is probably empty), so that half is unproven. |
| 1130 1132 1133 | `Room::GetDevilRoomChance` 0x007f92b0, insns 0x007f92cb / 0x007f962b / 0x007f96a4 (slots 197-199) | Chance is the local [esp+0xc]; a number answer replaces it (lab: proven for each). |
| 1113 1114 1115 | `Game::GetPlanetariumChance` 0x0074dbd0, insns 0x0074dd3f / 0x0074ddc1 / 0x0074ddcf / 0x0074de05 (slots 200-203) | Chance is [ebp-8] or xmm0; with Telescope Lens the callback sees the chance before the game's own lens term (lab: proven, applied once). |

Deferred: MC_PRE_LOOT_LIST_PUT / LootList callbacks 1334 and 1336 (the game's loot list is an MSVC `std::deque<LootListEntry>` and the Layer has no LootList class; the producer is 0x006dae40); MC_PRE_SLOT_DROP / explosion drops 1123 (the drops are inlined per variant in `Entity_Slot::TakeDamage` 0x00664560, no function boundary). Lint rule `insn-hook-giant-entry` was added.

### CB-c: grid entities, grid collisions, grid hurt, CanReroll, Room item pool, knife hitbox parent (hooks 205-223, lab `lab_cb_c.lua`, 2026-11-19)

Needs an engine build. Callbacks real (31): grid update 1402 1403 (door), 1410 1411 (pit), 1412 1413 (poop), 1416 1417 (rock), 1426 1427 (trapdoor);
grid render 1446 1447 (door), 1438 1439 (trapdoor), 1462 1463 (wall); 1011 `MC_POST_GRID_ROCK_DESTROY`; 1012 1013 `MC_GRID_HURT_DAMAGE` and its post;
grid collisions 1171-1174 (player, tear), 1177-1184 (bomb, pickup, projectile, NPC). Names real (7): `EntityPickup` Get/Set/ClearCanRerollOverride,
`Room` GetItemPool/SetItemPool, `EntityKnife` Get/SetHitboxParentKnife. Lab: 97 PASS, 0 FAIL. Census with the binary: callbacks 165 -> 196,
functions 799 -> 806. Trap run: same failed and `ERR:` lines as the base.

| What | Site (v1.9.7.17) | Notes |
|---|---|---|
| Update 1402-1427 | the five classes' `Update` (vtable slot 2): Door 0x0070c5c0, Pit 0x00712180, Poop 0x00714770, Rock 0x00718fe0, TrapDoor 0x0071fef0 (slots 205-209), thiscall, pop 0 | `(grid)`, param = the grid type; any boolean answer skips the update (the post is then not run). The Pit's body runs past the index end, so the index shows no `ret`. |
| Render 1438-1463 | Door 0x0070d1e0, Wall 0x00720fe0, and one function 0x0071edb0 for the Gravity, Spikes, Stairs, TNT, TrapDoor and Web classes (slots 210-212), `ret 4` | `(grid, offset)`; a boolean skips the draw, a Vector replaces the offset for the call (put back afterwards). The shared function is told apart by the grid type at `[this+4]`: only the trapdoor's names are wired. |
| 1011 | `GridEntity_Rock::Destroy` 0x007197d0, `ret 8` (slot 213) | `(rock, type, immediate, source)`, after a `true` return. The game's own `GridEntity:Destroy` passes a source EntityRef too (it is not nil). |
| 1012 1013 | `GridEntity::hurt_func` 0x00709fb0, `ret 0x14` (slot 214) | `(grid, entity, playerDamage, damageFlags u64, enemyDamage (xmm2), ignoreGridCollisionClass)`; any boolean from 1012 skips the damage. Spikes hurting the player is the lab's case. |
| 1171-1184 | `Entity::move_with_grid` 0x006a9c90: one hook per collision class right after `Room::GetGridCollision` 0x007f0800 (Bullet 0x006aa0d8 slot 215, Ground 0x006aa1ea 216, PitsOnly 0x006aa2da 217, NoPits 0x006aa467 218, Walls 0x006aa85e 219), and the player's own `Entity_Player::CollideWithGrid` 0x006ab440 at 0x006ab77b (220) | `(entity, gridIndex, gridEntity)`, param = variant (NPC: type). `true` ignores that cell for that entity (the collision test answers "none", the Bullet and NoPits sites jump past the push-back); an entity tests one cell many times a frame, so the answer is kept per (entity, cell, frame). `GetGridCollision(-1)` answers 4, so no -1 trick. |
| CanReroll | `Entity_Pickup::CanReroll` 0x006ee200 (slot 221), thiscall, AL, plain `ret` | The override is kept per InitSeed (survives leaving the room). Lab: the game's own D6 leaves the pedestal alone with the override false and rerolls it after `ClearCanRerollOverride`. |
| Room item pool | the static `Room::GetItemPool` 0x007ebe20 (slot 222): ecx = seed, edx = RoomDescriptor, boss id at `[esp+4]`, caller cleans | `SetItemPool(pool)` answers it; -1 hands the choice back; forgotten at `Room::Init` (slot 89, ahead of the PRE_NEW_ROOM callback). Lab: with the Devil pool set and one heavy item added, the pedestal's first random draw is that item; the pool is -1 again after `ChangeRoom`. `ItemPool:GetPoolForRoom` (0x006fe2f0) is a different function and does not go through it. In the start room the game's function answers -1 (POOL_NULL) for seed 1234. |
| Knife hitbox parent | `Entity_Knife::update_bone_swing` 0x009688d0, insn 0x0096a936 (slot 223): esi the weapon knife, edi the hitbox knife just spawned | The Forgotten's bone club hitbox (SubType 4) answers the club (SubType 0); the club answers nil. A recorded link is checked against both InitSeeds, so a freed and reused address answers nil. |

Findings. (1) `type(run(...))` raises when no callback answers (`run` returns no value): lint rule `type-of-callback-result`. (2) The lab's collision checks were
vacuous at first because `Room:RemoveGridEntity` plus `Isaac.GridSpawn` of the same cell left a collision with no grid entity (`GetGridEntity` nil); the lab keeps
the rock instead and proves each "ignored" answer against a case that was blocked. (3) A Gaper's AI paths around a rock, so the NPC case pushes its velocity each frame.
(4) Tears and projectiles need a height (-23) to collide with a rock.
Deferred: `MC_PRE_FAMILIAR_GRID_COLLISION` / `MC_FAMILIAR_GRID_COLLISION` (1175, 1176): a Brother Bobby pushed into a rock at 8 px/frame passed it and no hook site ran (familiars have no
grid collision class), so the names stay out of Implemented. The other grid classes' update and render names (fire, spikes, lock, pressure plate, teleporter, decoration, web, TNT, staircase,
statue; the shared render's non-trapdoor classes) are not wired: their per-class `Update` addresses are the next step (same recipe; the shared render needs only more ids).

### Pickups and pools (`api_pickup_more.lua`, lab `lab_pickup_more.lua`, 2026-11-19)

EntityPickup: Get/SetVarData, Get/SetDropDelay, AddCollectibleCycle, RemoveCollectibleCycle, HasFlipData, GetPickupGhost, GetPriceSprite,
TryInitOptionCycle, SetNewOptionsPickupIndex, TriggerTheresOptionsPickup, MakeShopItem, TryRemoveCollectible, UpdatePickupGhosts, ReloadGraphics,
CanJeraDuplicate, GetLootList, and the statics SetupCollectibleGraphics and GetRandomPickupVelocity; Room:GetShopItemPrice; the Lua classes LootList
(constructor, PushEntry, GetEntries) and LootListEntry (GetType, GetVariant, GetSubType, GetSeed, GetRNG); ItemPool HasCollectible, HasTrinket,
PickCollectible, AddCollectible. Every address, offset and calling convention is in the header of the module. Lab: see the hand-back (all PASS).
- The item pool: `GetPool(i)` = pool + 4 + i * 0x34 (31 pools); a pool is { float total weight, vector of 0x18-byte PoolItems }. The total weight
  includes the locked items (measured: it is the sum of all weights, not only the unlocked ones). The removed-collectible and room-blacklist
  bitsets are at pool + 0x67c / + 0x68c, the trinket vector (8-byte entries, byte 4 = in pool, byte 5 = unlocked) at pool + 0x794.
- AddCollectible grows the pool's own vector and is replayed at MC_POST_GAME_STARTED (pools are rebuilt per run, an entry already there is not
  doubled). The lab proves the game draws the added item (weight 1e7 -> `GetCollectible` answers it) and that PickCollectible sees it.
- `Pickup::GetLootList` (0x006dae40) takes a hidden deque return, `shouldAdvance`, and a player (0 works). The deque has one entry per block.
- `Isaac.GetItemIdByName` takes the English name ('The Inner Eye'); `ItemConfigItem.Name` is the localization key. Lint rule `item-name-is-a-key`.
- `ItemPool:GetTrinket(dontAdvance, seed)`: the first argument is NOT "decrease"; `GetTrinket(false)` removes the drawn trinket from the pool.
Done later by CB-c (section above): EntityPickup Set/Get/ClearCanRerollOverride (hook on `CanReroll` 0x006ee200) and Room Get/SetItemPool (hook on the static `Room::GetItemPool`).
Not done (deferred): ItemPool:GetCollectible's Flags argument: the game's Lua binding ignores a fifth argument (BAN_ACTIVES and BAN_PASSIVES drew the same items),
the game function 0x00733610 does take the flags (ecx pool, stack poolType, seed, flags word = flags << 1 | not-decrease, default), but a wrapper
put over the class table's entry was never the function a lookup answered (the lab's call went to the game's own binding), so it is not Implemented.

### Player items (`api_player_items.lua`, lab `lab_player_items.lua` 130 PASS / 0 FAIL, 2026-11-19)

Offsets measured in v1.9.7.17 (displacement-xref index, decompile, then the game's own reaction in the lab; the header of `api_player_items.lua` lists
each with its anchor). EntityPlayer: R U a Wizard timer 0x145c, Metronome item 0x1388, held entity 0x1410, Pony charge 0x1888 (the item-use switch sets 60,
only while the player's velocity squared is over 1), Plan C countdown 0x188c, Dead Eye charge 0x18b0 (`ClearDeadEyeCharge` 0x007c78e0 clears it at random: 20,
33, 50 percent per call), pee burst 0x18f8/0x18fc, bladder 0x1900/0x1904 (max starts at 0), urethra 0x1908/0x190c, Suplex state/aim/target/land
0x1f2c/0x1f30/0x1f34/0x1f3c (Suplex! writes state 1, aim 0x10), poop queue 0x1f58, Eve's Sumptorium 0x1f7c, costume vector 0x1220 (stride 0x130), layer map
0x122c, afterimage deque 0x1e54. Game functions called: SpawnLocust 0x005b35c0, InitPostLevelInitStats 0x007bc740, TryAddToBagOfCrafting 0x007d5170,
IncrementPlayerFormCounter 0x0075e320, GetTotalActiveCharge 0x007910f0, CheckPoopSpellQueue 0x007d6670, deque push 0x007dc740.
Done: 43 EntityPlayer names (the Suplex stand-ins in `api_gaps.lua` replaced), PlayerManager AnyPlayerTypeHasCollectible/Trinket, FirstBirthrightOwner,
GetRandomCollectibleOwner/TrinketOwner, CostumeSpriteDesc (14 names). Census 858 functions / 165 callbacks (was 799 / 165); the 95-mod trap run reaches all
five floor changes with the same failed and `ERR:` lines as the baseline. Backlog (regenerated from that census): EntityPlayer missing 27 -> 4, inert 5 -> 0,
PlayerManager 6 -> 1, CostumeSpriteDesc 2 -> 0, ItemConfig_Card 2 -> 1; AddCustomTag/HasCustomTag/GetTaggedItems were already done (the old census log
predated them) and `ItemConfig_Card.Weight` is the mods' own boss weights (the tool now drops it).
Not done: ModdedCardFront (REPENTOGON's own field at card config +0x6c, no vanilla counterpart: needs an engine hook), EntityPlayer Teleport / InitTwin /
ShuffleCostumes and PlayerManager.RemoveCoPlayer (engine functions not located; REPENTOGON's signatures match nothing here), SpawnAquariusCreep (the
effect's varData offset is unmeasured). A native `EntityPlayer:GetBombVariant(0, false)` called from a lab once stopped the run with "Host pointer handed to the
Guest is outside the Guest region" (not looked into). A transformation reached by `IncrementPlayerFormCounter` shows the unlock paper, which pauses the game.
Lint rule: `lab-prints-done`.

### Rooms and levels (`api_rooms_more.lua`, lab `lab_rooms_more.lua`, 2026-11-19)

Measured in v1.9.7.17 (a dump of a live room, `lab_rooms_probe.lua`): Room clear delay +0x11ec, Camera* +0x11f8 (the camera's +0x68 is its room), FXLayers +0x1618
with FXParams at +0x4f8 inside it (+0x1c use-water byte, +0x20 water colour, +0x34 colour multiplier, +0x48 shadow alpha, +0x4c shadow colour, +0x60 light colour,
+0x74 water effect colour), room TemporaryEffects +0x7310; RoomDescriptor Doors[8] at +0x1c (-1 = none) and BossDeathSeed +0x64. `Camera` (thiscall, ret 4):
IsPosVisible 0x00944a40 (bool in AL), SetFocusPosition 0x00944d10, SnapToPosition 0x00944d40. `Game::PlaceRoom` 0x00738ab0 `ret 0x14` (ecx = the Game; stack: a
LevelGenerator_Room, the RoomConfigRoom, seed, 0, 0), its constructor 0x007381b0 `ret 0xc` (ecx = a 0x50-byte buffer; column, line, shape); descriptors from
game+0x14 (0xb8 bytes each), their count at game+0x182cc, the current dimension at game+0x1830c.
Done: Room SetRoomClearDelay, GetCamera, GetFXLayers, GetFXParams, GetEffects; Camera IsPosVisible, SetFocusPosition, SnapToPosition; FXParams (variables
UseWaterV2, ShadowAlpha, WaterColor, WaterColorMultiplier, ShadowColor, LightColor, WaterEffectColor); RoomDescriptor GetErrorTrinketEffect, Doors,
BossDeathSeed (the two variables are put into the real descriptor's property tables at the first room, so the census flags are set at load); GridEntity ToWall;
Level TryPlaceRoom / TryPlaceRoomAtDoor (the door of the CURRENT room is not spawned: `Room::MakeDoor` was not found; the descriptors' doors are linked).
Lab 41 PASS / 0 FAIL. Left out because the effect was not proven: GridEntity:HurtDamage (`hurt_func` 0x00709fb0 `ret 0x14` takes the damage SOURCE entity first
and reads its type; with the first player it returned true but a poop and a rock did not change), Room:IsChampionBossSeed (no way to check REPENTOGON's
rules against the game), RoomConfigRoom.Mode (the game's own class already answers `Mode`).
Deferred: Room GetItemPool/SetItemPool (engine hook, cb-c); Camera SetClampEnabled (patch of `Camera::ClampedToRoom` 0x00943490); GridEntity
SetWaterClipFlags/ResetWaterClipFlags (override of the vtable GetWaterClipInfo); RoomConfig LoadStb and CcpContainer_RoomConfigSet AddRooms (REPENTOGON's
VirtualRoomSet, about 1300 lines); LevelGeneratorRoom GenerationIndex (only reachable through MC_PRE_LEVEL_PLACE_ROOM, `LevelGenerator::place_room`
0x009b0330, cb-c); Room UpdateColorModifier (stays inert: `Room::ComputeColorModifier` not found), DoLightningStrike / GetLightningIntensity (the Game's
lightning offset in .17 is unknown), ClearBossHazards, FXLayers AddPoopFx, RoomDescriptor InitSeeds (functions not located). Careful: `names.json` is only a
signature match; "FXLayers::AddPoopFx 0x4dd7f0" is really an NPC update.
False positives of the backlog tool, now suppressed: `REVEL.RoomLists[...]:AddRooms` (a Revelations function, not RoomConfig.AddRooms) and StageAPI's
`:SetMusic` (not RoomConfigStage). Lint rules: `hurt-func-source`, and `backlog-narrowing` now needs MODLIB.

### CB-d: HUD, history HUD, completion, fortune and item text callbacks (hooks 224-232, `api_hooks_hud.lua`, lab `lab_cb_d.lua`, 2026-11-19)
13 callbacks fire now (census 196 -> 209; functions 937 unchanged): MC_PRE/POST_PLAYERHUD_RENDER_HEARTS (1118/1091, `PlayerHUD::RenderHearts`
0x00843df0 `ret 0x18`, found through HUD::Render's call that pushes the hearts sprite at HUD+0x39a0), MC_PRE/POST_COMPLETION_MARKS_RENDER (1216/1217,
`CompletionWidget::Render` 0x004252f0 `ret 8`; the widget is `{ int characterId; ANM2 at +4 }`, read back), MC_PRE_FORTUNE_DISPLAY (1483,
`HUD::ShowFortuneText` 0x009a3ac0 `ret 4`: REPENTOGON's signature matches 0x00912560, another class's function; the right one is what the Lua
binding HUD:ShowFortuneText calls), MC_PRE/POST_HISTORYHUD_RENDER (1027/1028, `HistoryHUD::Render` 0x0083ac20 `ret 8`, `this` = HUD+0x5c54) and
MC_POST_HISTORYHUD_RECOMPUTE (1029, `Recompute` 0x0083b280 `ret 8`, only when immediate), MC_PRE/POST_COMPLETION_EVENT (1049/1052,
`Manager::RecordPlayerCompletion` 0x0095a7b0 static `ret 4`), MC_COMPLETION_MARK_GET / MC_POST_COMPLETION_MARK_GET (1047/1048, from
`PersistentGameData::IncreaseEventCounter` for the game's characters, by the counter names PROGRESSION_KILL_<boss>_WITH_<character>, and from
RecordPlayerCompletion for a mod's characters, not tested with a real mod character), MC_PRE_ITEM_TEXT_DISPLAY (1484, `HUD_Message::Show` 0x00840b00 `ret 0x10`
and `HUD::ShowStackedItemTextCustomUTF16` 0x009a34a0 `ret 0x10`). Facts: the history HUD is drawn only when `Options.ExtraHUDStyle` > 0
(`[options+0x2a384]`, test at 0x009a40f1); in this build the item text goes through those two functions only (no separate UTF-8 stacked function).
Limits: the HistoryHUD object handed to 1027-1029 has no methods yet (GetCollectibles, GetPosition, ... are not implemented, and neither is
HUD:GetHistoryHUD); a table returned from MC_PRE_HISTORYHUD_RENDER (HideCollectibles/HideTrinkets) is accepted and ignored. Lab 34 PASS / 0 FAIL
(the pause screen shows the widget). Deferred: MC_CAN_SELECT_CHARACTER and the character select page/portrait callbacks (1328-1332): REPENTOGON patches
the middle of `Menu_Character::Render` and the wheel loop (five mid-function patches, `ModManager::RenderCustomCharacterMenu` 0xa54150 only for mods),
which needs instruction hooks inside the menu's render code and a way to check them in a window; not located in this time box.

### Fiend Folio key blocks: `AddCacheFlags(flags, true)` (`api_cacheflags.lua`, lab `lab_ffkeys.lua`, 2026-11-19)
Play report: in "Vermin - Key Chain" the coloured key blocks never opened. Cause: Fiend Folio's `CollectPuzzleKey` calls
`player:AddCacheFlags(CACHE_FAMILIARS, true)` to get its key-ghost familiars from `MC_EVALUATE_CACHE` -> `CheckFamiliar`. The game's own
`AddCacheFlags` takes only the flags and evaluates nothing (nothing runs `EvaluateItems` on its own), so no ghost ever appeared. REPENTOGON's
second argument means "EvaluateItems now"; the Layer now wraps the game's function and does that. The `[warn] Pickup variant 752 not implemented
yet!` line is the game's harmless message for a mod pickup variant. Lab: a key is collected next to a custom grid `FFKeyBlock{Blue,Green,Red}`, the ghost
spawns, binds to the key, flies to the block and the block opens (`Breaking`, then the grid is cleared): 3 colours pass. The mod opens a block only when
the player is within `Size + 26` (36) of its centre; a player against a wall is about 30 away. Not tested: chain blocks (`FFChainBlock*`, broken by
the BFS from a key block), the tricolor key and the gray block (Dungeon Master). Lint rule: `add_cache_flags_one_arg`.

### Next batches (proposed)

0. (done, B1: quad getters, Copy, Translate, Flip. B2: Capsule value type, Shape, FindInCapsule; left: Capsule:Collide and the game-made capsules. B3: pause/daily/prompt state, Delirium fields, GetDimension; left: the rest of GenericPrompt, selected elements, Delirium's other fields.)
1. (done, batch 2: see above) EntityPlayer weapon/laser fields, `Weapon`, EntityKnife. What is left of them: Weapon's virtual
   functions, the unmeasured player fields, hitbox parent knife (a hook).
2. (done, batch 3, except `TryPlaceRoom`/`TryPlaceRoomAtDoor` and `MC_PRE_LEVEL_PLACE_ROOM`, which need the game's PlaceRoom: its
   REPENTOGON signature matches nothing, so find it by the callers of the level generator.) RoomDescriptor and Level room placement.
3. (PocketItem done, batch 3) ProceduralItem (External Item Descriptions, 10 used names: GetItem, GetEffect, GetEffectCount,
   GetTargetItem, GetDamage, GetFireDelay, GetLuck, GetRange, GetShotSpeed, GetSpeed): the procedural item record is still to measure.
   Other names the user's mods use, with a unique signature match and a simple call (found by `.scratch/m8-census/mcand.py` over the batch 3
   census): `Entity:CopyStatusEffects` (done, worker familiar), `EntityPickup`
   GetRandomPickupVelocity (static, ecx/edx), InitFlipState (REPENTOGON's wrapper does more than the game function), `EntityFamiliar`
   GetRandomWisp, `Game.DevolveEnemy` (TryAimAtMarkedTarget done). Field-only names to find with the diff probe: Room GetWaterAmount/SetWaterAmount,
   GetLightningIntensity, Game IsRerun/IsErased, the Dizzy amount, EntityPlayer GetKeepersSackBonus, Immaculate/Cambion conception state.
4. The L callbacks the mods use (96), grouped by hook site: grid (rock destroy, grid collision, trapdoor/door update), player
   (trigger death, revive, drop card/pill), HUD (hearts, history HUD), planetarium/devil, completion marks and shop purchase.
5. ImGui (58 names, none used by a mod) last.

**Entity batch (worker entity, 2026-11-19; `api_entity_more.lua`, lab `lab_entity_more_test.lua`).** `EntityFamiliar:GetMultiplier` (the game's
own function 0x0060dc40, float in XMM0, which `B.call` now answers as its fourth value), `EntityNPC:GetPathfinder` and
`EntityFamiliar:GetPathfinder/GetPathFinder` (embedded struct at NPC +0x414, familiar +0x5fc, measured by `lab_entity_probe.lua`; class taken from
the game's `npc.Pathfinder` userdata), `Entity:GetColorParams/SetColorParams` and the `ColorParams` class (vector at +0x1ac, 0x3c entries),
`Entity:Get/Set/ResetWaterClipFlags` (the game's `Entity::GetWaterClipInfo` 0x006b6ed0, hook slot 179 rewrites the flags from a per-entity
override; needs an engine build). GridEntity's water clip flags are not done.

**Ambush batch (worker ambush, 2026-11-19; `api_ambush.lua`, lab `lab_ambush_test.lua`).** The global `Ambush`: GetCurrentWave, Get/SetMaxBossrushWaves,
Get/SetNumBossesPerWave, GetMaxChallengeWaves (3), GetMaxBossChallengeWaves (2), StartChallenge, SpawnWave, SpawnBossrushWave. The object is at
game+0x265c8 (REPENTOGON v1.9.7.12: +0x25b40; found from every caller of StartChallenge); fields +0x18 max boss-rush waves, +0x1c bosses per wave,
+0x20 active, +0x24 wave (0 when idle). Functions 0x00833420 / 0x00833600 / 0x00833c40, plain `ret` (REPENTOGON's byte signatures for the first two
do not match). Measured in a challenge room: active byte 1 after StartChallenge, wave counter 1, 2, 3 as the waves are killed. GetNextWave(s) and
Set*ChallengeWaves are not done (the game computes those maxima itself). `RoomDescriptor.Data` is writable (`__propset` on the descriptor class
tables, address written at descriptor +0x10, type-checked); the game follows (teleporting into a rewritten room gives the new room type).
`RoomConfigHolder` carries every `RoomConfig` function (`on_both` in api_roomconfig.lua; lint rule `roomconfig-both-tables`).
Lab 57/0 with 95 mods; Fiend Folio's Toadsie now passes SetWaterClipFlags and GetPathfinder and stops at `Sprite:Copy` (toadsie.lua:142, not in the Layer yet).

**Player death, health type, births, cards and pills (worker cba, 2026-11-19; lab `lab_player_death_cb.lua`, 74 PASS / 0 FAIL).** Hook slots 180 to 192
(the slot space is now 224: `RGON_HOOKS` in host_rgon.c, `RGON_SLOTS` in lift_patches.py, `MAX_SLOTS` in hook_table.py; `api_hooks.lua` arms the new
code only when `B.HOOKS >= 224`, so an older binary still runs the Layer). Needs an engine build.

| Callback | Game function / site (v1.9.7.17) | Notes |
|---|---|---|
| MC_PRE_TRIGGER_PLAYER_DEATH 1050 | `Entity_Player::TriggerDeath` 0x007a1090, ret 4 (slot 180) | `(player)`; a `false` answer revives (Revive, damage cooldown kept) and the function answers true without dying. The check-only call (arg 0) is skipped. |
| MC_TRIGGER_PLAYER_DEATH_POST_CHECK_REVIVES 1051 | insn hook at 0x7a1c97 (slot 185), `ebx` = player | Fires only when no vanilla revive (1UP, Dead Cat, ...) applies; `false` revives. |
| MC_PRE_PLAYER_REVIVE 1481 | `Entity_Player::Revive` (existing hook) | `false` returns 0 without reviving, and POST_PLAYER_REVIVE does not fire. |
| MC_PLAYER_HEALTH_TYPE_CHANGE 1128 | the `GetHealthType` hook | `(player, new, previous, default)`; the previous type is kept per player (keyed by the entity's init seed at +0x3ec, checked in the lab), re-entry guarded. No heart conversion (see below). |
| MC_PRE_PLAYER_GIVE_BIRTH_IMMACULATE 1475 | insn 0x7c81a6 (slot 190), `ebx` = familiar flag | Collectible 413 = 0x19d; `(player, flag)`, a `false` answer clears the flag. |
| MC_PRE_PLAYER_GIVE_BIRTH_CAMBION 1474 | insn 0x775e1b (slot 191), `eax` = flag | Collectible 412 = 0x19c. Same shape. Lint rule `conception-pairing`: the two were swapped once by guessing from function order. |
| MC_PRE_TRIGGER_BED_SLEEP_EFFECT 1285 | insn 0x9acd4e in `ItemOverlay::Update` (slot 189) | `(player)`; `true` skips the heal (jump to 0x9ace00). Fires once the bed overlay starts. |
| MC_PRE_PLAYER_POCKET_ITEMS_SWAP 1287 | insn 0x7a4331 in `control_drop_pocket_items` (slot 188) | `(player)`; `true` skips the swap (jump to 0x7a43f6). The game runs this code twice per update, so one button press swaps twice unless answered. |
| MC_PRE/POST_PLAYER_ADD_CARD 1350/1351, ADD_PILL 1352/1353 | `AddPocketItem` 0x007a3af0, `SetCard` 0x007a3cb0, `SetPill` 0x007a3cf0 (all ret 8; slots 181 to 183) | `(player, id, slot)`, param = id; slot 1 when slot 0 holds a pocket active. `false` cancels, a number replaces the id. |
| MC_POST_PLAYER_COLLECT_CARD 1357 / COLLECT_PILL 1359 | insns 0x6ec1c1 / 0x6ec1e7 in `Entity_Pickup::HandleCollision` (slots 186, 187), `esi` pickup, `edi` player | `(player, pickup)`, param = subtype. |
| MC_POST_PLAYER_DROP_CARD 1360 / DROP_PILL 1361 | `DropPocketItem` 0x007a48f0, ret 8 (slot 184) plus insn 0x7a4986 (slot 192) that catches the spawned pickup | `(player, pickup, slot)`, param = subtype. |

Not done: the twin (Jacob/Esau) sync and the Forgotten/Keeper heart fixes after a revive; heart conversion on a health type change and the XML `healthtype`
default; MC_POST_PLAYER_REMOVE_CARD/PILL (1354/1355); the drops caused by a lost pocket slot; MC_PRE_PLAYER_COLLECT_CARD/PILL (1356/1358).
REPENTOGON's byte signatures for the two birth sites match nothing in this build; the sites were found from the store `or [player+0x1938], reg`.

**Player health batch (worker player-health, 2026-11-19; `api_player_health.lua`, lab `lab_player_health.lua` 115 PASS / 0 FAIL, probe `lab_player_health_probe.lua`).**
Offsets measured in v1.9.7.17 (REPENTOGON's are 0x14 to 0x24 lower): Candy Heart ups 0x1db0 and Soul Locket ups 0x1dbc (six u16: Damage, FireDelay,
TearRange, ShotSpeed, MoveSpeed, Luck; 5 written into each word moved exactly that stat in `EvaluateItems` 0x00763570), Keeper's Sack 0x1fbc (raises Damage, Range,
Speed; not Luck), Red Stew duration 0x1e74 (Damage), Maggy drain cooldown 0x1ef4 (300 as Tainted Magdalene, one fewer a frame), Immaculate state 0x1930
(+1 per heart pickup, birth at 15), Cambion state 0x1934, ConceptionFamiliarFlags 0x1938 (0x200 after a birth). `UpdateIsaacPregnancy(bool)` is 0x007c7d70
(thiscall, ret 4); `HUD::GetPlayerHUDLayout(index, player)` 0x009a6570 (thiscall, ret 8). PlayerHUD slot: flash counters are the bytes at +0xc, +0xd, +0xe
(the game counts one down every two frames), 24 hearts of 0x10 bytes from +0x10 as in REPENTOGON (animation names are C strings at +8 and +0xc).
Done: EntityPlayer Get/Set Immaculate/Cambion ConceptionState (Immaculate clamped 0..14), Get/SetConceptionFamiliarFlags, Get/SetKeepersSackBonus,
Get/SetMaggyHealthDrainCooldown, Get/SetRedStewBonusDuration, UpdateIsaacPregnancy, Get/AddCandyHeartBonus, Get/AddSoulLocketBonus; PlayerHUD
Get/SetFlashRed/Soul/GoldHearts, GetLayout, GetHearts, GetHeartByIndex; PlayerHUDHeart (all seven). Backlog EntityPlayer missing 42 -> 27, PlayerHUD 7 -> 0,
PlayerHUDHeart 2 -> 0; census 743 functions / 155 callbacks (was 710 / 155). Not done: Red Stew's own use did not set the duration when `UseActiveItem`
was called from a script (only the field and its effect on Damage were measured); PlayerHUD hearts are copies (as REPENTOGON's); a co-op layout (1 to 3) was not seen.
Lint rules: `health-field-offsets`, `cacheflag-literals`.

**Game and miscellaneous globals batch (worker game-misc, 2026-11-19; `api_game_misc.lua`, lab `lab_game_misc.lua` 123 PASS / 0 FAIL).**
Game struct in v1.9.7.17: debug flags 0x26544, IsRerun byte 0x26588, dizzy 0x67738 (current) and 0x6773c (target), colour modifier current/target/lerp
0x676b8 / 0x676d0 / 0x676e8, erased-enemy vector 0x6774c..0x67750 (12-byte records). Functions: `SetColorModifier` 0x007067c0 (xmm3 = rate), `DevolveEnemy` 0x00705480,
ChainLightning 0x00707e10, random unit vector 0x0067f0d0, erased-id fix-up 0x007076d0, `AddErasedEnemy` 0x00707840, `GetCurrentStageConfigId` 0x0082d030,
`ItemOverlay::Show` 0x009ad210 (ret 0xc; id past 0x2f is ignored), PlayJingle 0x007e2580 (ret 0xc, countdown at +0x364). Challenges 1..45 are a byte array in the
persistent data; higher ids are kept by the Layer. Done: RNG RandomVector/PhantomVector, REPENTOGON.MeetsVersion and the WeightedOutcomePicker constructor (flags only),
Game Add/GetDebugFlags piece, IsRerun, Get/SetDizzyAmount, IsErased, the three colour getters and SetColorModifier (the old Lua-only stand-ins are gone), DevolveEnemy,
ChainLightning, MoveToRandomRoom, Isaac GetCurrentStageConfigId / RenderToWorld / AllTaintedCompletion / the four challenge calls, the ItemOverlay table,
MusicManager PlayJingle/StopJingle, BossPool and BossPoolManager, the EntitySaveState setters. Not done (no Lua-only way, or no match in this exe): ProceduralItemManager.CreateProceduralItem
and EntityNPC.ThrowMaggot (REPENTOGON's byte signatures match nothing), the HistoryHUD family with its two callbacks, Input.GetMouseWheel, Options.BetterVoidGeneration,
Isaac LoadModDataFromFolder/SetIcon/ShowErrorDialog/StartNewGame/DrawLine/RenderCollectionItem. `RenderToWorld`'s screen-shake and scroll terms read as zero in the lab, so only the
zero-offset case is verified. Lint rules: `stdstring-read`, `backlog-narrowing` (MODFIELD).

**Render batch (worker render, wave 4 chunk 8; `api_render.lua`, `api_misc.lua`, lab `lab_render_test.lua` 218 PASS / 0 FAIL).**
Sprite animation state in v1.9.7.17: two 0x20-byte blocks at +0x30 (main) and +0x50 (overlay): +0x04 animation record, +0x08 per-layer frame
indices, +0x10 frame (float), +0x14 playing byte. The game's own `Stop()` clears the main byte only; its `IsFinished(name)` is also true while an
animation is merely stopped, so the Layer works "finished" out from the animation data (non-looping and at its last frame). Done: `Sprite` Stop (with
REPENTOGON's overlay flag), StopOverlay, Continue, ContinueOverlay, IsOverlayEventTriggered and WasOverlayEventTriggered (the two state blocks swapped
around the game's own event check), Copy (file, animations, spritesheets, every layer field, colour, flips, speed, flags), ReplaceSpritesheet's third
argument, SetOverlayFrame(frame); `Point` (new, a Lua value class); the quad constructors and `NewFromBounds/NewFromRectangle` UV flag (all quad names, counted
as "Class.Class"); `Font` argument checks in front of the game's own Lua legacy functions (DrawString, DrawStringScaled, UTF8 pair, GetStringWidth pair);
`EntityConfig.GetBaby` and `EntityConfigBaby` (resources/babies.xml: id, name, skin, achievement). Beam now holds Points and a copy of its sprite.
Stand-ins, not in `REPENTOGON.Implemented` (they need render targets and shader programs inside the engine): Image (width/height answer 0, GetTexelRegion,
Render, RenderWithShader), Sprite/LayerState GetSpritesheet and SetSpritesheet (an Image from `Renderer.LoadImage(path)` really replaces the layer's graphics;
one from CreateImage is not drawn), the custom shader functions on Sprite and LayerState, `Beam:Render`, `SourceQuad` Convert functions (they divide by an Image's size).
No texture pointer exists in a layer state (+0x08 is the path string), so an Image needs an engine hook: deferred. Lint rule: `ctor-implemented-name`.
Census: functions 937 to 961, constructors 7 to 13, callbacks 196. Missing names used by Fiend Folio 54 to 39 and by Epiphany 20 to 17.
**Weapon, laser and knife callbacks (worker cb-e, wave 4, 2026-11-20; `api_weapons.lua`, hook slots 272-281, lab `lab_cb_e.lua` 28 PASS / 0 FAIL; census callbacks 196 -> 207).**
Hooks (all `asm` rows of `hook_table.py`, every `ret` checked): `Weapon::Fire` 0x009f3540 (ret 0xc; the base class's, every weapon's Fire calls it first; it runs about three times per shot),
`Weapon::TriggerTearFired` 0x009f7930 (ret 8; REPENTOGON's signature matches nothing, frame grew to 0x2a8), `Entity_Laser::DoDamage` 0x0097e3c0 (ret 4, damage in xmm2),
`Entity_Player::FireBoneClub` 0x007ae030 (ret 0x10), `Weapon::FireSword` 0x009f6c50 (ret 0x18), `Entity_Familiar::FireBrimstone` 0x00638c10 (ret 8), `Entity_Familiar::FireTechLaser` 0x00613c20
(ret 4; found by shape, called for variant 53 = Robo-Baby 2.0), `Weapon_Brimstone::do_baby_brimstone_burst` 0x00a01020 (ret 0xc, damage scale in xmm3), and two instruction hooks in
`Entity_Laser::damage_entities` 0x00971810 for MC_PRE_LASER_COLLISION: sample lasers at 0x009720b5 (entity eax, laser [ebp-0x7c], ignoring goes to 0x0097207c) and straight lasers at
0x00973201 (laser edi, entity esi, ignoring goes to 0x00973651). MC_POST_FIRE_KNIFE (1257) reuses the existing FireKnife hook (slot 68). `lift_patches.apply_insn_hooks` now gives a redirect target
that the lifter left without a label (the fall-through of a conditional branch) an `L_xxxxxxxx: ;` right after its `RECOMP_VA` marker. Proven in the lab by the game's own reaction: tears
(1105, 1098 with args and a WeaponType filter), Mom's Knife (1257), Spirit Sword (1258), Forgotten's club (1254), Technology / Technology 2 / Brimstone / `EntityLaser.ShootAngle` variants 1-14 on a frozen
Monstro (1248 and 1249 with args; a `true` answer on 1248 silences 1249 on both the sample and the straight path), Lil Brimstone (1261), Robo-Baby 2.0 (1262, variant filter), Tainted Azazel's sneeze
(1036 and 1037, source player, direction, damage scale 1.0). Not seen: Hemoptysis's sneeze did not fire in 900 frames (the callback is armed and fires for Tainted Azazel; Azazel himself never sneezes).
Deferred: FireSplitTear split sites (not reached).
