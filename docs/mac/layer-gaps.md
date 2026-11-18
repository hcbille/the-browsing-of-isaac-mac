# Layer gaps (milestone M8 preparation)

Read-only analysis; nothing was run in the game. Inputs: the 95 mods of the user's Workshop collection (<workshop dir>), the real mod scan (`web/mods.mjs` `createModScan`, unchanged), the gate rule of `rgon.lua`, and REPENTOGON's source checkout and docs. Drivers are in `.scratch/layer-analysis/` (`scan.mjs`, `classify.mjs`, `gen.mjs`).

## Caveat: how "the layer has it" was decided

The gate normally compares against `REPENTOGON.Implemented`, which only exists in a running engine (`lab_names.lua`). No engine was run here, so a name counts as "implemented" when its identifier (or callback name) appears anywhere in `rgon.lua` / `api_*.lua`. That over-counts the layer: the true missing lists will be somewhat longer, and some mods listed as Shown may be hidden by the live gate. Re-run with a real `implemented.txt` through `rgon/tools/gate_preview.mjs` (or read the `[isaac][rgon] gate:` log lines) once M8 can boot. Mod-folder Lua is scanned at any depth; one mod of the user's 96 (Fiend Folio, packed by the page) is not in this Workshop folder, which holds 95.

## 1. Per-mod results

11 of 95 mods name REPENTOGON in code (the other 84 never do and get no gate decision). By the static estimate: **3 shown** (1 of them with stand-ins), **8 hidden**.

| Mod | Workshop id | Uses REPENTOGON | Decision | Why | Names used / missing |
|---|---|---|---|---|---|
| Custom Stage API | 1348031964 | yes | hidden | layer lacks part of what it uses | 38 / 19: BossType, GetBlendMode, GetDecoSaveState, GetEntitiesSaveState, GetExtraPortraitPath, GetGridEntitiesSaveState, GetNameImagePath, GetPortraitPath, GetTaintedKeeperCoinSpawns, GiveMinecart, ... |
| !!miniMAPI | 1978904635 | yes | hidden | layer lacks part of what it uses | 2 / 1: HasMirrorDimension |
| Planetarium Chance [REP+] | 2489006943 | yes | hidden | names REPENTOGON, uses nothing of it | 0 / 0 |
| !![REP]Music Mod Callback | 2491006386 | yes | hidden | layer lacks part of what it uses | 2 / 1: MC_POST_NIGHTMARE_SCENE_RENDER |
| 'Enhanced Boss Bars | 2635267643 | yes | Shown | layer has all it uses | 1 / 0 |
| Mantle Rework | 2665189152 | yes | hidden | steps aside for REPENTOGON | 0 / 0 |
| !Unique Items API | 2836316643 | yes | hidden | layer lacks part of what it uses | 6 / 3: MC_MENU_INPUT_ACTION, MC_PRE_GLOWING_HOURGLASS_LOAD, MC_PRE_GLOWING_HOURGLASS_SAVE |
| Giant Props! | 2846951982 | yes | hidden | names REPENTOGON, uses nothing of it | 0 / 0 |
| [Rep] The Binding of Isaac: Epiphany | 3012430463 | yes | Shown-with-stand-ins | main.lua quits without REPENTOGON | 311 / 136: AddCandyHeartBonus, AddCollectibleCycle, AddLocust, AddMagnetized, AddNullItemEffect, AddSoulLocketBonus, BossType, CanPlaceRoom, CanPlaceRoomAtDoor, CreateAfterimage, ... |
| Door Overhaul | 3585879877 | yes | Shown | layer has all it uses | 2 / 0 |
| !!~External item descriptions | 836319872 | yes | hidden | layer lacks part of what it uses | 30 / 16: GetActionProperty, GetActionType, GetBagOfCraftingContent, GetBagOfCraftingOutput, GetCollectiblesFromPool, GetConditionProperty, GetConditionType, GetEffect, GetEffectCount, GetEntitiesSaveState, ... |

The 84 remaining mods: uses REPENTOGON = no (no decision, REPENTOGON reads nil for them).

Full missing lists per mod are in `.scratch/layer-analysis/result.json`.

## 2. Union of missing names

161 distinct missing names across the mods that use REPENTOGON (37 callbacks, 124 functions or classes). 14 are needed by more than one mod; the rest are needed by Epiphany alone, except as marked.

### Ranked by number of mods (names needed by 2+ mods)

| # mods | Name | Kind / class | Mods | Effort | Reason |
|---|---|---|---|---|---|
| 3 | MC_MAIN_MENU_RENDER | callback | Custom Stage API; [Rep] The Binding ; !!~External item d | S | handler on an existing menu/HUD/console function (hook table row + api_hooks.lua) |
| 2 | BossType | unknown | Custom Stage API; [Rep] The Binding  | S | class method already on other classes (Knife/Tear/Laser InHitList; Delirium BossType; Dump is a render helper) |
| 2 | GetBagOfCraftingContent | function / EntityPlayer | [Rep] The Binding ; !!~External item d | M | player field or call; player offsets moved vs REPENTOGON |
| 2 | GetBlendMode | function / LayerState | Custom Stage API; [Rep] The Binding  | S | plain accessor |
| 2 | GetCollectiblesFromPool | function / ItemPool | [Rep] The Binding ; !!~External item d | M | field or call into the game, address to find |
| 2 | GetEntitiesSaveState | function / RoomDescriptor | Custom Stage API; !!~External item d | L | save-state structures of the room (entities/grid) |
| 2 | GetNameImagePath | function / EntityConfigPlayer | Custom Stage API; [Rep] The Binding  | S | read of XML data the layer already parses (api_data.lua) |
| 2 | GetPortraitPath | function / EntityConfigPlayer | Custom Stage API; [Rep] The Binding  | S | read of XML data the layer already parses (api_data.lua) |
| 2 | HasMirrorDimension | function / Level | !!miniMAPI; [Rep] The Binding  | S | one dimension query |
| 2 | InitFlipState | function / EntityPickup | Custom Stage API; [Rep] The Binding  | M | pickup fields/ghost/option-cycle logic |
| 2 | MC_CONSOLE_AUTOCOMPLETE | callback | Custom Stage API; [Rep] The Binding  | S | handler on an existing menu/HUD/console function (hook table row + api_hooks.lua) |
| 2 | MC_MENU_INPUT_ACTION | callback | !Unique Items API; [Rep] The Binding  | S | handler on an existing menu/HUD/console function (hook table row + api_hooks.lua) |
| 2 | MC_PRE_GLOWING_HOURGLASS_LOAD | callback | !Unique Items API; [Rep] The Binding  | L | fires on the game-state save/restore; needs engine hooks and the layer save file |
| 2 | SetBackdropType | function / Room | Custom Stage API; [Rep] The Binding  | M | room fields; Backdrop needs rendering effect |

Names needed by exactly one non-Epiphany mod: GetActionProperty (!!~External it), GetActionType (!!~External it), GetBagOfCraftingOutput (!!~External it), GetConditionProperty (!!~External it), GetConditionType (!!~External it), GetDecoSaveState (Custom Stage A), GetEffect (!!~External it), GetEffectCount (!!~External it), GetExtraPortraitPath (Custom Stage A), GetGridEntitiesSaveState (Custom Stage A), GetI3 (!!~External it), GetNumItemPools (!!~External it), GetPlayerFormCounter (!!~External it), GetTaintedKeeperCoinSpawns (Custom Stage A), GetTargetItem (!!~External it), GiveMinecart (Custom Stage A), IsItemInCollection (!!~External it), MC_POST_NIGHTMARE_SCENE_RENDER (!![REP]Music M), MC_PRE_BOSS_SELECT (Custom Stage A), MC_PRE_GLOWING_HOURGLASS_SAVE (!Unique Items ), SetRail (Custom Stage A), SetTaintedKeeperCoinSpawns (Custom Stage A), SetWaterAmount (Custom Stage A), SetWaterCurrent (Custom Stage A).

### Epiphany (Hard Dependent), full missing list: 136 names

Epiphany's main.lua quits without REPENTOGON, so it is always shown with stand-ins; anything missing is a Lua error or a feature that silently never runs. Grouped by class, with effort (S small, M medium, L large) and reason.

Effort tally for Epiphany: S 22, M 82, L 32.

**Callbacks** (34)

| Name | Effort | Reason |
|---|---|---|
| MC_HUD_POST_UPDATE | S | handler on an existing menu/HUD/console function (hook table row + api_hooks.lua) |
| MC_POST_CHALLENGE_DONE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_COMPLETION_MARKS_RENDER | M | render hook of a HUD/menu draw function; needs the draw call found |
| MC_POST_COMPLETION_MARK_GET | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_FIRE_BONE_CLUB | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_FIRE_KNIFE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_FIRE_SWORD | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_GLOWING_HOURGLASS_LOAD | L | fires on the game-state save/restore; needs engine hooks and the layer save file |
| MC_POST_GLOWING_HOURGLASS_SAVE | L | fires on the game-state save/restore; needs engine hooks and the layer save file |
| MC_POST_GRID_ENTITY_DOOR_UPDATE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_GRID_ENTITY_ROCK_UPDATE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_GRID_ENTITY_TRAPDOOR_UPDATE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_GRID_ROCK_DESTROY | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_PICKUP_SHOP_PURCHASE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_PLAYERHUD_RENDER_HEARTS | M | render hook of a HUD/menu draw function; needs the draw call found |
| MC_POST_PLAYER_DROP_CARD | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_PLAYER_DROP_PILL | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_POST_START_GREED_WAVE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_COMPLETION_MARKS_RENDER | M | render hook of a HUD/menu draw function; needs the draw call found |
| MC_PRE_GRID_ENTITY_TRAPDOOR_RENDER | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_LEVEL_PLACE_ROOM | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_M_MORPH_ACTIVE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_PICKUP_GET_LOOT_LIST | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_PICKUP_UPDATE_GHOST_PICKUPS | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_PICKUP_VOIDED | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_PLAYERHUD_RENDER_HEARTS | M | render hook of a HUD/menu draw function; needs the draw call found |
| MC_PRE_PLAYER_GRID_COLLISION | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_PLAYER_REVIVE | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_SLOT_CREATE_EXPLOSION_DROPS | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_PRE_TRIGGER_PLAYER_DEATH | L | fires from the middle of a game function: new instruction hook (needs engine/lift rebuild, host ABI) |
| MC_MENU_INPUT_ACTION | S | handler on an existing menu/HUD/console function (hook table row + api_hooks.lua) |
| MC_PRE_GLOWING_HOURGLASS_LOAD | L | fires on the game-state save/restore; needs engine hooks and the layer save file |
| MC_MAIN_MENU_RENDER | S | handler on an existing menu/HUD/console function (hook table row + api_hooks.lua) |
| MC_CONSOLE_AUTOCOMPLETE | S | handler on an existing menu/HUD/console function (hook table row + api_hooks.lua) |

**EntityPlayer** (44)

| Name | Effort | Reason |
|---|---|---|
| GetCostumeLayerMap | M | player field or call; player offsets moved vs REPENTOGON |
| GetCustomCacheValue | M | player field or call; player offsets moved vs REPENTOGON |
| GetDamageModifier | M | stat modifier fields + cache evaluation |
| GetFireDelayModifier | M | stat modifier fields + cache evaluation |
| GetImmaculateConceptionState | M | player field or call; player offsets moved vs REPENTOGON |
| GetInnateCollectibleCount | S | api_innate.lua already holds the data |
| GetInnateCollectibleGroup | S | api_innate.lua already holds the data |
| GetKeepersSackBonus | M | player field or call; player offsets moved vs REPENTOGON |
| GetLuckModifier | M | stat modifier fields + cache evaluation |
| GetMarkedTarget | M | player field or call; player offsets moved vs REPENTOGON |
| GetPonyCharge | M | player field or call; player offsets moved vs REPENTOGON |
| GetShotSpeedModifier | M | stat modifier fields + cache evaluation |
| GetSpeedModifier | M | stat modifier fields + cache evaluation |
| GetSuplexState | M | Suplex fields on the player; offsets must be measured |
| GetTearDisplacement | M | player field or call; player offsets moved vs REPENTOGON |
| GetUrnSouls | M | player field or call; player offsets moved vs REPENTOGON |
| IncrementPlayerFormCounter | M | player field or call; player offsets moved vs REPENTOGON |
| IsItemCostumeVisible | M | player field or call; player offsets moved vs REPENTOGON |
| IsNullItemCostumeVisible | M | player field or call; player offsets moved vs REPENTOGON |
| RemovePoopSpell | M | player field or call; player offsets moved vs REPENTOGON |
| SetActiveVarData | M | player field or call; player offsets moved vs REPENTOGON |
| SetDamageModifier | M | stat modifier fields + cache evaluation |
| SetEveSumptoriumCharge | M | player field or call; player offsets moved vs REPENTOGON |
| SetFireDelayModifier | M | stat modifier fields + cache evaluation |
| SetHeadDirection | M | player field or call; player offsets moved vs REPENTOGON |
| SetImmaculateConceptionState | M | player field or call; player offsets moved vs REPENTOGON |
| SetInnateCollectibleCount | S | api_innate.lua already holds the data |
| SetKeepersSackBonus | M | player field or call; player offsets moved vs REPENTOGON |
| SetLuckModifier | M | stat modifier fields + cache evaluation |
| SetShotSpeedModifier | M | stat modifier fields + cache evaluation |
| SetSpeedModifier | M | stat modifier fields + cache evaluation |
| SetSuplexAimCountdown | M | Suplex fields on the player; offsets must be measured |
| SetSuplexLandPosition | M | Suplex fields on the player; offsets must be measured |
| SetSuplexState | M | Suplex fields on the player; offsets must be measured |
| SetSuplexTargetPosition | M | Suplex fields on the player; offsets must be measured |
| SetTearRangeModifier | M | stat modifier fields + cache evaluation |
| ShuffleCostumes | M | player field or call; player offsets moved vs REPENTOGON |
| GetBagOfCraftingContent | M | player field or call; player offsets moved vs REPENTOGON |
| AddCandyHeartBonus | M | player field or call; player offsets moved vs REPENTOGON |
| AddLocust | M | player field or call; player offsets moved vs REPENTOGON |
| AddNullItemEffect | M | player field or call; player offsets moved vs REPENTOGON |
| AddSoulLocketBonus | M | player field or call; player offsets moved vs REPENTOGON |
| CreateAfterimage | M | player field or call; player offsets moved vs REPENTOGON |
| DropCollectibleByHistoryIndex | M | player field or call; player offsets moved vs REPENTOGON |

**EntityPickup** (13)

| Name | Effort | Reason |
|---|---|---|
| GetPickupGhost | M | pickup fields/ghost/option-cycle logic |
| GetRandomPickupVelocity | M | pickup fields/ghost/option-cycle logic |
| HasFlipData | M | pickup fields/ghost/option-cycle logic |
| MakeShopItem | M | pickup fields/ghost/option-cycle logic |
| SetNewOptionsPickupIndex | M | pickup fields/ghost/option-cycle logic |
| SetVarData | M | pickup fields/ghost/option-cycle logic |
| TriggerTheresOptionsPickup | M | pickup fields/ghost/option-cycle logic |
| TryInitOptionCycle | M | pickup fields/ghost/option-cycle logic |
| TryRemoveCollectible | M | pickup fields/ghost/option-cycle logic |
| UpdatePickupGhosts | M | pickup fields/ghost/option-cycle logic |
| AddCollectibleCycle | M | pickup fields/ghost/option-cycle logic |
| GetAlternatePedestal | M | pickup fields/ghost/option-cycle logic |
| InitFlipState | M | pickup fields/ghost/option-cycle logic |

**Level** (6)

| Name | Effort | Reason |
|---|---|---|
| GetNeighboringRooms | L | room placement/level-generator internals |
| TryPlaceRoom | L | room placement/level-generator internals |
| CanPlaceRoom | L | room placement/level-generator internals |
| CanPlaceRoomAtDoor | L | room placement/level-generator internals |
| FindValidRoomPlacementLocations | L | room placement/level-generator internals |
| HasMirrorDimension | S | one dimension query |

**Game** (4)

| Name | Effort | Reason |
|---|---|---|
| GetCurrentColorModifier | M | field or call into the game, address to find |
| IsHardMode | M | field or call into the game, address to find |
| IsPauseMenuOpen | M | field or call into the game, address to find |
| SetColorModifier | M | field or call into the game, address to find |

**Camera** (4)

| Name | Effort | Reason |
|---|---|---|
| IsPosVisible | M | Camera object fields, new class wrapper |
| SetClampEnabled | M | Camera object fields, new class wrapper |
| SetFocusPosition | M | Camera object fields, new class wrapper |
| SnapToPosition | M | Camera object fields, new class wrapper |

**Room** (4)

| Name | Effort | Reason |
|---|---|---|
| SetItemPool | M | room field |
| SetRoomClearDelay | M | room field |
| UpdateColorModifier | M | room field |
| SetBackdropType | M | room fields; Backdrop needs rendering effect |

**EntityConfigPlayer** (3)

| Name | Effort | Reason |
|---|---|---|
| GetCostumeID | S | read of XML data the layer already parses (api_data.lua) |
| GetNameImagePath | S | read of XML data the layer already parses (api_data.lua) |
| GetPortraitPath | S | read of XML data the layer already parses (api_data.lua) |

**EntityFamiliar** (3)

| Name | Effort | Reason |
|---|---|---|
| GetMultiplier | M | field or call into the game, address to find |
| GetRandomWisp | M | field or call into the game, address to find |
| TryAimAtMarkedTarget | M | field or call into the game, address to find |

**(class not in docs)** (3)

| Name | Effort | Reason |
|---|---|---|
| InHitList | S | class method already on other classes (Knife/Tear/Laser InHitList; Delirium BossType; Dump is a render helper) |
| Dump | S | class method already on other classes (Knife/Tear/Laser InHitList; Delirium BossType; Dump is a render helper) |
| BossType | S | class method already on other classes (Knife/Tear/Laser InHitList; Delirium BossType; Dump is a render helper) |

**PlayerHUDHeart** (2)

| Name | Effort | Reason |
|---|---|---|
| GetHeartAnim | S | plain accessor |
| IsEternalHeartOverlayVisible | S | plain accessor |

**EntityKnife** (2)

| Name | Effort | Reason |
|---|---|---|
| GetIsSpinAttack | M | field or call into the game, address to find |
| GetIsSwinging | M | field or call into the game, address to find |

**RNG** (2)

| Name | Effort | Reason |
|---|---|---|
| GetShiftIdx | M | field or call into the game, address to find |
| PhantomFloat | M | field or call into the game, address to find |

**ItemPool** (2)

| Name | Effort | Reason |
|---|---|---|
| PickCollectible | M | field or call into the game, address to find |
| GetCollectiblesFromPool | M | field or call into the game, address to find |

**LevelGeneratorRoom** (2)

| Name | Effort | Reason |
|---|---|---|
| DoorMask | S | plain accessor |
| GenerationIndex | S | plain accessor |

**BossPool** (1)

| Name | Effort | Reason |
|---|---|---|
| GetEntries | S | plain accessor |

**CostumeSpriteDesc** (1)

| Name | Effort | Reason |
|---|---|---|
| GetSkinColor | S | plain accessor |

**Weapon** (1)

| Name | Effort | Reason |
|---|---|---|
| GetWeaponType | M | field or call into the game, address to find |

**Sprite** (1)

| Name | Effort | Reason |
|---|---|---|
| IsOverlayEventTriggered | M | field or call into the game, address to find |

**LootList** (1)

| Name | Effort | Reason |
|---|---|---|
| PushEntry | S | plain accessor |

**RoomDescriptor** (1)

| Name | Effort | Reason |
|---|---|---|
| SetGroup | L | save-state structures of the room (entities/grid) |

**Entity** (1)

| Name | Effort | Reason |
|---|---|---|
| AddMagnetized | M | field or call into the game, address to find |

**LayerState** (1)

| Name | Effort | Reason |
|---|---|---|
| GetBlendMode | S | plain accessor |

### Classification of the whole union

By kind: 37 callbacks; 121 functions on existing classes (EntityPlayer 46+ is the bulk); 3 resolved by name only. New wrapper objects needed: Camera (Room/Game accessors), ProceduralItem/ProceduralEffect (Bag of Crafting, Genesis), LootList entries, RoomDescriptor save-state vectors, LayerState blend modes. Engine-feature (deep Host work): room placement (`Level:TryPlaceRoom`, `MC_PRE_LEVEL_PLACE_ROOM`), glowing hourglass save/restore callbacks, death/revive pre-callbacks, weapon fire callbacks, grid-entity update callbacks. Effort over the union: S 26, M 89, L 46.

## 3. How the Layer is built (for later Layer engineers)

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

## 4. M8 on the Mac Build: what is needed

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

## 5. Suggested order of Layer work

1. Epiphany's 22 S items and the names shared by two mods (MC_MAIN_MENU_RENDER, MC_MENU_INPUT_ACTION, MC_CONSOLE_AUTOCOMPLETE, BossType, GetBlendMode, GetNameImagePath, GetPortraitPath, HasMirrorDimension): they unblock Custom Stage API, miniMAPI, Unique Items API and External Item Descriptions at the same time as Epiphany, and each is a few lines of Lua.
2. Player stat modifiers, Suplex, innate, and costume accessors (M, `api_player.lua`): the bulk of Epiphany's 82 M items.
3. Callbacks that need new hooks (L): batch them in one engine rebuild.
4. Re-run `scan.mjs` with the real `implemented.txt` after the first run of M8; hidden mods that need only a few names (Planetarium Chance and Giant Props use nothing of REPENTOGON's that the scan can name, so they stay hidden by the rule, not by missing names) are cheap wins only if the rule is changed, which is not proposed here.
