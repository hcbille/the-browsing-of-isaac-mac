# Steam bridge (M9) design

Status: M9 implemented in code (`native/mac/host/mac_steam.c`), syntax-checked only; NOT built or run yet, nothing verified (section 8). Design written from the Host sources, the unpacked exe's import table, and a look at this Mac's Steam install. Hard limits that apply: never type credentials, never write to Steam Cloud, never buy anything, never write into the Workshop folder.

## 1. What the Guest imports from Steam, and what the Host does today

The Guest imports exactly 10 symbols from `steam_api.dll` (128 call sites, 56 address-taken). It does NOT import any `SteamAPI_ISteam...` function. Every interface is reached through the SDK's inline accessors:

    push &initData ; call [SteamInternal_ContextInit] ; mov ecx,[eax] ; mov eax,[ecx] ; call [eax+off]

`initData` is a 3-word guest struct `{ pFn, counter, ptr }`. `pFn` is a tiny guest function that does `*arg = SteamInternal_FindOrCreateUserInterface(SteamAPI_GetHSteamUser(), "<version string>")`.

| Import (IAT slot) | Host today (`host_shims_steam.c`, `host_shims_misc.c`) |
|---|---|
| `SteamInternal_ContextInit` (0xb18a1c) | Returns the slot address. For the two allow-listed init structs (0xbf93c8, 0xc5c510) it plants a fake 16-slot "CSteamAPIContext" vtable whose slots return 0 or 1; every other struct reads NULL, so the game takes its "Steam not running" arm. Logged once per slot. |
| `SteamAPI_RegisterCallback` (0xb18a0c) | Records (obj, id), up to 64. A global constructor calls it before main. |
| `SteamAPI_UnregisterCallback` (0xb18a14) | Removes the record. |
| `SteamAPI_RunCallbacks` (0xb18a04) | No-op. Nothing is ever dispatched. |
| `SteamAPI_RegisterCallResult` (0xb18a10), `SteamAPI_UnregisterCallResult` (0xb18a18) | Default/no-op shim (not in the Steam files). |
| `SteamInternal_SteamAPI_Init` (0xb18a08), `SteamAPI_Shutdown` (0xb18a00) | Default/no-op shim. |
| `SteamInternal_FindOrCreateUserInterface` (0xb189fc), `SteamAPI_GetHSteamUser` (0xb18a20) | Only reached from the `pFn` functions, which the current shim never runs. |

Net effect today: Steam is "absent". No achievement, stat, overlay, UGC, cloud or friends call ever reaches a real object. (`native/steam_stub/` is a separate Windows-side stub DLL for the Browser Build; not used by the Mac Build.)

### Interfaces the game asks for (version strings found in the exe, with their `pFn`)

| Interface | pFn | initData | Notes |
|---|---|---|---|
| ISteamUserStats `STEAMUSERSTATS_INTERFACE_VERSION013` | 0x41e440 | 0xbf93c8 | Achievements and stats. Vtable offsets observed after the accessor: 0x0c 0x14 0x18 0x24 0x30 0x50 0x5c 0x60 0x64 0xac. Exact method mapping still to confirm (see below). |
| ISteamRemoteStorage `..._VERSION016` | 0x685ca0 | 0xc5c3a4 | Cloud. 45 accessor sites; offsets 0x0 0x18 0x3c 0x4. MUST stay write-no-op. |
| ISteamUGC `..._VERSION020` | 0x8fd3a0 | 0xc5c48c | Workshop. 6 sites. |
| ISteamApps `..._VERSION008` | 0x9ef980 | 0xc5c4e0 | DLC checks (BIsDlcInstalled at +0x1c per the shim notes). |
| ISteamUtils `SteamUtils010` | 0xa238c0 | 0xc5c4ec | 13 sites. |
| ISteamFriends `SteamFriends017` | 0xa23900 | 0xc5c4f8 | Names, overlay. |
| ISteamMatchmaking `SteamMatchMaking009` | 0xa53860 | 0xc5c504 | 40 sites. Out of scope (online co-op). |
| ISteamUser `SteamUser023` | 0xa5f1a0 | 0xc5c510 | The only ones at +0x38 are the context-init dance. |
| ISteamNetworking `SteamNetworking006` | 0xa76090 | 0xc5c51c | Out of scope. |

The exe also carries the full interface-version table (Controller, Input, Screenshots, HTTP, Parties, etc., at 0xb9f200..) used only by the SDK's own `CSteamAPIContext::Init`; no accessor for them was found.

Mapping caveat: the game was built with MSVC against SDK ~1.58-1.60, so vtable slot index = Windows layout. MSVC lays overloaded virtuals out in reverse declaration order within a group (GetStat int/float, SetStat int/float swap). The bridge must map `(interface, vtable offset)` to a flat function through a table generated from the SDK headers (`steam_api_flat.h` / `steam_api.json` come with the SDK, which needs a Steam login to download: not allowed). Fallback without headers: first run with a trace-only bridge that logs every `(interface, offset)` the game actually calls (small set, see offsets above), then hand-write the table from the public interface declarations. Rough expectation for UserStats013 (unverified): slot 6 GetAchievement, 7 SetAchievement (+0x1c), 10 StoreStats (+0x28), 0 RequestCurrentStats, 3/4 SetStat. The observed offsets do not obviously contain +0x1c/+0x28, so the trace step is not optional.

## 2. The real libsteam_api.dylib on this Mac

Not in a game folder: Mac Afterbirth+ (250900) is NOT installed on the Mac Steam (`steamapps/` holds only `libraryfolders.vdf` and `sourcemods`; `libraryfolders.vdf` lists 228980 and 250900 with size 0). `~/Library/Application Support/Steam/steamapps/common` does not exist.

Found inside the Steam client's own bundle:

    ~/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/Frameworks/Steam Helper.app/Contents/MacOS/libsteam_api.dylib

- Universal: `x86_64 arm64` (lipo -info). The arm64 slice loads into the arm64 Mac Build.
- 394,304 bytes, signed by Valve (TeamIdentifier MXGJJ98X76), dated 13 Nov 2026, min macOS 12.0, install name `@loader_path/libsteam_api.dylib`, depends only on libc++ and libSystem.
- 1059 exported symbols. SDK era is newer than the game's: flat accessors `SteamAPI_SteamUserStats_v013`, `SteamRemoteStorage_v016`, `SteamApps_v009`, `SteamFriends_v018`, `SteamUGC_v021`, `SteamUser_v024`, `SteamUtils_v011`, `SteamNetworkingSockets_SteamAPI_v013`. The game wants UserStats013 (match), RemoteStorage016 (match) and older Apps008, Friends017, UGC020, User023, Utils010. Therefore resolve interfaces by the game's own version strings through `SteamInternal_FindOrCreateUserInterface(hUser, "<string>")` (exported), not the `_vNNN` accessors; the Steam client serves older versions.
- Needed exports present: `SteamAPI_InitFlat`, `SteamAPI_InitSafe`, `SteamAPI_Shutdown`, `SteamAPI_RunCallbacks`, `SteamAPI_IsSteamRunning`, `SteamAPI_RestartAppIfNecessary`, `SteamAPI_GetHSteamUser`, `SteamAPI_ManualDispatch_{Init,RunFrame,GetNextCallback,FreeLastCallback,GetAPICallResult}`, `SteamInternal_{ContextInit,SteamAPI_Init,FindOrCreateUserInterface,CreateInterface}`, `SteamAPI_ISteamUserStats_{SetAchievement,GetAchievement,ClearAchievement,StoreStats,GetStatInt32,GetStatFloat,SetStatInt32,SetStatFloat,IndicateAchievementProgress,GetAchievementAndUnlockTime,...}` (`RequestCurrentStats` is NOT exported: newer SDKs dropped it, stats load automatically at init; the bridge answers the game's RequestCurrentStats call itself by returning true and injecting a UserStatsReceived_t callback after the first successful ManualDispatch frame, or via `RequestUserStats(ownSteamID)`), `SteamAPI_ISteamFriends_ActivateGameOverlay*`, `SteamAPI_ISteamUtils_{IsOverlayEnabled,BOverlayNeedsPresent,SetOverlayNotificationPosition}`, 8 `SteamAPI_ISteamRemoteStorage_File{Write,Delete,Share,WriteAsync}` entries (never to be called).
- Overlay: `.../Steam.AppBundle/Steam/Contents/MacOS/gameoverlayrenderer.dylib` (universal x86_64+arm64, 461,552 bytes) and `gameoverlayrenderer32.dylib` (old, 2021).
- The Windows `steam_api.dll` (266,336 bytes) in the game folder is irrelevant on the Mac.
- No Steamworks SDK download is needed or allowed. Redistribution: the dylib belongs to Valve; do not commit it to the public repo. The build script should copy it from the user's Steam bundle (or an installed Steam game) into `Repentance+.app/Contents/Frameworks/` locally, and the user can drop one in by hand. Option if it is missing: the bridge runs in "no Steam" mode (section 3 fallback).
- Note the Steam bundle location is where Steam keeps its own copy; a Mac game installed through Steam may ship its own copy. Prefer, in order: app Frameworks, next to executable, `STEAM_API_DYLIB` env path, Steam bundle path above (read-only use).

## 3. Bridge design

Components: a new Host file `native/mac/host/mac_steam.c` (guarded by `ISAAC_NATIVE`) that replaces the bodies of the 10 `steam_api` imports on the Mac Build only; the Browser Build is unchanged.

### 3.1 Loading and graceful failure
1. At startup (before static constructors run, since `SteamAPI_RegisterCallback` is called pre-main; record those calls exactly as today) do nothing Steam-side yet.
2. At the Guest's `SteamInternal_SteamAPI_Init` (or lazily at the first ContextInit): `dlopen` in order `@executable_path/../Frameworks/libsteam_api.dylib`, `@executable_path/libsteam_api.dylib`, `$ISAAC_STEAM_DYLIB`. Check the file has an arm64 slice implicitly (dlopen fails otherwise; log the dlerror text).
3. Resolve flat symbols with `dlsym`; every symbol optional except `SteamAPI_InitFlat` and `SteamInternal_FindOrCreateUserInterface`.
4. If `dlopen` fails, `IsSteamRunning`/`InitFlat` fails, or the Steam client is not running: log one `[mac][steam] disabled: <reason>` line and keep today's behaviour (all accessors NULL, the game takes its no-Steam arm). Never exit, never prompt, never block. The Launcher shows "Steam: off (<reason>)".
5. `SteamAPI_RestartAppIfNecessary` is never called (it could relaunch through Steam and loop).

### 3.2 App ID
Launched from Steam (Launch Options `"<path>/Repentance+.app/Contents/MacOS/<launcher>" %command%`): Steam sets `SteamAppId`/`SteamGameId`; use as is. When started by hand: if `SteamAppId` is unset, set `SteamAppId=250900` and `SteamGameId=250900` with `setenv` before `InitFlat`; also, if the executable directory is writable, do NOT write `steam_appid.txt` (avoid writing into the bundle); the env var is enough. Note: a hand launch with a running Steam client makes the client believe the game is running only if the app ID is owned by the logged-in user; it is.
`SteamAPI_InitFlat(char errMsg[1024])` returns `ESteamAPIInitResult` (0 = OK).

### 3.3 Mapping the Guest's 32-bit interface calls
The Guest cannot see native pointers, so each interface is a guest-side fake object (the DI8 pattern already used):
- `SteamInternal_ContextInit(initData)`: implement per SDK: if `initData.counter` differs from the bridge's context counter, call guest `pFn(&initData.ptr)` (via the dispatcher, which reaches `FindOrCreateUserInterface`), store the counter, return `&initData.ptr`. Verify the `{pFn, counter, ptr}` layout against the call sites first (assumed from SDK; the existing shim returns the slot address and works with a one-cell view).
- `SteamInternal_FindOrCreateUserInterface(hUser, versionStr)`: read the guest string, call the native one (if live) and remember the native pointer `N`. Return a guest object `G` (guest scratch, as at `ISAAC_TEB_VA + 0xd500...`) whose first word points to a guest vtable of PROVIDED shim tokens. Token n for interface X maps to a table entry `{flat function, signature, argument bytes}`.
- Each token implementation reads thiscall args from the guest stack (`isaac_arg`), converts guest strings to host `char*`, calls the flat function with the native `N` (kept in a side table keyed by `G`), writes outputs (e.g. `bool *pbAchieved`) back to guest memory, and sets EAX (and EDX for 64-bit returns). The dispatcher cleans the stack with the declared arg bytes (the lesson from boot round 11: wrong arg_bytes drift ESP, so every token gets its own exact purge).
- Unmapped vtable slots: log `[mac][steam] unmapped <iface>+0x<off>` once and return 0 with the correct purge if known, else default 0 purge (the SDK interfaces are all thiscall callee-cleans-args). To prevent ESP drift, only interfaces whose full used-offset set has been mapped and purge-verified get a live vtable; the rest keep the NULL arm (the existing allow-list approach, generalised).
- Live set for M9: `ISteamUserStats` (achievements, stats), `ISteamUtils` (overlay-related, `IsOverlayEnabled`, `BOverlayNeedsPresent`), `ISteamFriends` (`ActivateGameOverlay`, persona name), `ISteamUser` (`GetSteamID`, `BLoggedOn`), `ISteamApps` (DLC/owned checks). Cloud and the rest stay NULL or no-op (3.5).
- 64-bit values (SteamID, SteamAPICall_t) occupy two stack slots in, EDX:EAX out.
- Callbacks: `SteamAPI_RegisterCallback(obj, id)` records the guest callback object. Native `SteamAPI_RunCallbacks()` would try to dispatch to native `CCallbackBase` objects the Guest never registered, so use manual dispatch: `SteamAPI_ManualDispatch_Init()`, then per frame in the Guest's `SteamAPI_RunCallbacks`: `RunFrame(hPipe)`, loop `GetNextCallback`, for each `CallbackMsg_t` with id matching a recorded guest registration, copy `m_pubParam` into guest memory and call the guest callback object's virtual `Run(void*)` (vtable slot 0 of the guest object; MSVC `CCallbackBase`: Run(void*), Run(void*,bool,uint64) overloads), then `FreeLastCallback`. For `SteamAPI_RegisterCallResult` (async results such as stats received), match `k_iSteamAPICallCompleted` (703) to the recorded handle, fetch with `GetAPICallResult`, then call the guest's `Run(void*, bool, uint64)`. Callback structs differ by packing (Windows 32-bit uses pack 8 for Steam structs; macOS pack 4): convert per struct for the few used (UserStatsReceived_t 1101, UserStatsStored_t 1102, UserAchievementStored_t 1103, GameOverlayActivated_t 331, SteamAPICallCompleted_t 703). Everything else is dropped with a one-time log.
- `HSteamPipe` for ManualDispatch: `SteamAPI_GetHSteamPipe()` from the dylib.

### 3.4 Achievements
Flow expected: game calls RequestCurrentStats at start, receives UserStatsReceived_t, then SetAchievement + StoreStats; the bridge forwards the flat calls. Safeguard: log every `SetAchievement name` with the native return value, so the Verifier sees them. Nothing is written without the Steam client; the unlock happens on the user's real profile only when launched while the client is running and logged in. A "dry-run" env `ISAAC_STEAM_DRYRUN=1` logs and returns true without forwarding, for testing against the real client without touching the profile.

### 3.5 Cloud and Workshop (hard limit)
`ISteamRemoteStorage` is never connected to the native object. Guest object is always a fake: `FileWrite`, `FileWriteAsync`, `FileDelete`, `FileForget`, `FileShare`, `SetCloudEnabledForApp`, stream writes return false/0 (no-op); `FileExists`/`FileRead`/`GetFileSize`/`GetFileCount` report none; `IsCloudEnabledForAccount/App` returns false; `GetQuota` writes total=0 available=0 and returns true (or false if the game stalls on it; measure). The save path is the Host's local files (ADR 0002). The dylib's `SteamAPI_ISteamRemoteStorage_*` symbols are not even resolved. `ISteamUGC`: stays NULL (mods come from the filesystem Workshop folder, M7); never call `SubscribeItem`/`DownloadItem`/etc.; a later read-only `GetNumSubscribedItems` is the most that could be added. No writes into the Workshop folder.

### 3.6 Overlay (uncertain)
The Mac overlay is `gameoverlayrenderer.dylib` (arm64+x86_64), injected via `DYLD_INSERT_LIBRARIES` into games that Steam itself starts (Launch Options flow), and hooks the process' OpenGL/Metal present paths. Our window is SDL3 plus ANGLE on Metal: SDL3 owns a `CAMetalLayer`, and ANGLE presents with `MTLCommandBuffer presentDrawable`, not through `NSOpenGLContext flushBuffer`. Unknown whether the overlay hooks that: it may hook `CAMetalLayer nextDrawable`/`presentDrawable` (works) or only `NSOpenGLContext` (would not show). Also hardened-runtime or ad-hoc-signed apps must allow `DYLD_INSERT_LIBRARIES` (needs the `com.apple.security.cs.allow-dyld-environment-variables` entitlement or no hardened runtime); our app is ad-hoc signed so no hardened runtime is expected, which should let injection work. Plan: (a) the bridge exposes `IsOverlayEnabled`, `BOverlayNeedsPresent` and handles `GameOverlayActivated_t` by pausing the game (like the PC game); (b) test only after the user logs in: launch through Steam, press Shift+Tab; if nothing, the fallbacks are none for the native path (Metal hook is Valve's code). Record the outcome in this file as "overlay: works / does not hook ANGLE-Metal". Do not claim it works untested.

## 4. This Mac's Steam state (read-only checks)

- `/Applications/Steam.app` exists (bundle version 6.1; the real client lives in `~/Library/Application Support/Steam/Steam.AppBundle/Steam`).
- Not running now (`pgrep -l steam_osx` empty). The user must start it and log in themselves.
- `~/Library/Application Support/Steam/userdata/<id>/` exists with `7/`, `config/`, `cloudstorage/` (a `config` folder holds `localconfig.vdf`, plus `librarycache`, `licensecache`). There is no `250900/` folder under userdata, so no Repentance+ cloud data exists on the Mac client.
- Mac Afterbirth+ (250900) is not installed. The Windows depot lives outside Steam in the TBOI game folder (with `appmanifest_250900.acf` and `workshop/content/250900`).
- Launch Options live in `~/Library/Application Support/Steam/userdata/<steamid3>/config/localconfig.vdf`, path `UserLocalConfigStore > Software > Valve > Steam > apps > 250900 > LaunchOptions`. Today the file has a `250900` entry but no `LaunchOptions` key. Do not edit it while Steam runs (Steam rewrites it); document the string for the user to paste via the Steam UI (Properties > General > Launch Options) after they install Afterbirth+. Per the plan the value is `"<path>/Repentance+.app/Contents/MacOS/<launcher>" %command%`.
- A user-only prerequisite: install Mac Afterbirth+ (it is the 250900 license that makes `SteamAppId=250900` valid for a Steam launch), log in, start Steam.

## 5. Stub test plan (no Steam client needed)

Stub source and a built arm64 stub live in `.scratch/steam-research/` (`stub_steam_api.c`, `libsteam_api_stub.dylib`, install name `@rpath/libsteam_api.dylib`, 11 exported flat symbols, each call logs `[stub-steam] ...` to stderr). It exports `SteamAPI_InitFlat` returning false-as-OK (0), `IsSteamRunning`, `RestartAppIfNecessary`, `Shutdown`, `RunCallbacks`, `GetHSteamUser`, `SteamInternal_FindOrCreateUserInterface`, and `ISteamUserStats_{RequestCurrentStats,SetAchievement,StoreStats,GetAchievement}`. To be extended with `ManualDispatch_*` (return no callbacks) when the bridge is written.

Verifier steps: see section 8.4 (exact commands and the log lines each must show). They replace the list that stood here; the stub is now the 28-export version described at the end of 8.4.

## 6. Open items

- Confirm the vtable-offset to flat-function table for ISteamUserStats013 / ISteamUtils010 / ISteamFriends017 / ISteamUser023 / ISteamApps008 by the trace pass (section 1).
- Confirm `initData` is `{pFn, counter, ptr}` at all 9 accessor sites.
- The existing shim's "fake context" for 0xbf93c8 and 0xc5c510 is the init dance for those two structs; verify what the sites at 0xa7db31 (engine init) expect before replacing it, as boot round 11 shows ESP drift is the failure mode.
- Overlay hook with ANGLE-Metal is untested (needs a logged-in client).


## 7. Trace pass (static)

Method: every `SteamInternal_ContextInit(&initData)` accessor in the Ghidra export was paired with the vtable offset of the call that follows it (all 9 accessors, about 190 sites). Slot numbers were then checked against the vtable order of the macOS `libsteam_api.dylib` (each flat wrapper loads one vtable slot; read with `otool`/`nm` only, nothing copied). The two orders agree wherever the interface did not change, and the argument shapes at the game's sites confirm each mapping. Nothing was run.

### 7.1 Findings that change the design above

1. The game was built against an SDK where `ISteamUserStats` has NO `RequestCurrentStats` (slot 0 is `GetStat`). Its achievement calls start at +0x14, not +0x1c/+0x28. So there is no RequestCurrentStats call to answer, no `UserStatsReceived_t` to inject, and the 3.4 flow is simply `SetAchievement` then `StoreStats`. The macOS dylib has the same layout (it exports no `RequestCurrentStats`), so no remapping is needed beyond MSVC overload order (7.2).
2. The slot `0xbf93c8` is `ISteamUserStats`, not an "init dance" for CSteamAPIContext. `0xc5c510` is `ISteamUser`. The shim's allow-list serves both with the 16-slot fake CSteamAPIContext vtable. Consequences today: `GetAchievement` (+0x14) lands on `ConnectToGlobalUser` (returns 0), `SetAchievement` (+0x18) and `StoreStats` (+0x24) land on `Zero` tokens, and the leaderboard calls at +0x54 and above read past the 16-slot table (zero memory, a call through NULL if that code path is ever reached). Whether each token's purge matches the real argument bytes is unverified, so ESP drift is possible. This is why real mapping tokens with exact purges are required for UserStats before it can go live.
3. `SteamInternal_ContextInit` returns a pointer to the third word of `initData` `{pFn, counter, ptr}` (ctx at +8 on 32-bit); callers do `cmp [eax],0` then read `[eax]` as the interface pointer. The current shim returns the base and overwrites `pFn`, which works while Steam is absent. A live bridge must store the counter at +4, the interface at +8 and return +8.
4. The game never calls `IsOverlayEnabled`, `BOverlayNeedsPresent`, `SetOverlayNotificationPosition`, `RequestCurrentStats`, or any overlay-activated callback. The only overlay-related call is `ISteamFriends::ActivateGameOverlayToUser("steamid", id)`. The overlay therefore depends only on a successful `SteamAPI_Init` with the process launched by Steam (or Valve's overlay dylib injected); no bridge method is needed to "enable" it, and there is nothing to pause on `GameOverlayActivated_t`.
5. The game DOES write to Steam in two ways besides achievements: leaderboard score upload (`UploadLeaderboardScore`, +0x78) and a cloud-file share chain (`FileWrite` + `FileShare` + `AttachLeaderboardUGC`, used to attach a replay to a leaderboard entry). The second is Steam Cloud and must be refused.
6. `SteamInternal_SteamAPI_Init("SteamUtils010", 0)` is the game's init (one call, boot path 0xa5f0e0); `SteamAPI_RunCallbacks` has one direct call site (mod loading) besides the IAT uses. Callbacks the game registers with `SteamAPI_RegisterCallback`: id 168 (`GetTicketForWebApiResponse_t`), 304 (`PersonaStateChange_t`), 506 (`LobbyChatUpdate_t`). Call results (`SteamAPI_RegisterCallResult`) are used by leaderboards, the FileShare/UGC chain, and lobby code. No achievement call needs a callback.

### 7.2 Offset tables

Convention: guest offsets are 32-bit vtable byte offsets (slot x 4). "Purge" is the callee-cleaned argument byte count of the thiscall (excluding `this`). `u64` args and returns use two stack slots in, EDX:EAX out. MSVC lays out overloaded virtuals in reverse order inside the overload group, so the int and float overloads swap: GetStat float is +0x00 and GetStat int is +0x04, SetStat float is +0x08 and SetStat int is +0x0c, and the same for `GetUserStat`, `GetGlobalStat`, `GetGlobalStatHistory` and `GetAchievementProgressLimits` (float +0xa8, int32 +0xac). Observed argument shapes at the sites confirm the int overloads are the ones used.

**ISteamUserStats, `STEAMUSERSTATS_INTERFACE_VERSION013` (initData 0xbf93c8, pFn 0x41e440)**

| Off | Slot | Method | Purge | Used by (function VAs) | Flat entry on macOS | Plan |
|---|---|---|---|---|---|---|
| +0x04 | 1 | GetStat(const char*, int32*) | 8 | 0xa61ba0, 0xa61d20 | `SteamAPI_ISteamUserStats_GetStatInt32` | live |
| +0x0c | 3 | SetStat(const char*, int32) | 8 | 0xa61ba0 | `..._SetStatInt32` | live |
| +0x14 | 5 | GetAchievement(const char*, bool*) | 8 | 0x9548d0, 0xa61e50 | `..._GetAchievement` | live |
| +0x18 | 6 | SetAchievement(const char*) | 4 | 0xa61af0, 0xa61b30 | `..._SetAchievement` | live |
| +0x24 | 9 | StoreStats() | 0 | 0xa61af0, 0xa61b30, 0xa61ba0 | `..._StoreStats` | live |
| +0x30 | 12 | IndicateAchievementProgress(const char*, u32, u32) | 12 | 0xa61ba0 | `..._IndicateAchievementProgress` | live |
| +0xac | 43 | GetAchievementProgressLimits(const char*, int32*, int32*) | 12 | 0xa61ba0, 0xa61d20 | `..._GetAchievementProgressLimitsInt32` | live |
| +0x54 | 21 | FindOrCreateLeaderboard(name, sortMethod, displayType) -> SteamAPICall_t | 12 | 0xa7db80 | `..._FindOrCreateLeaderboard` | later (call results) |
| +0x58 | 22 | FindLeaderboard(name) -> SteamAPICall_t | 4 | 0xa7db80 | `..._FindLeaderboard` | later |
| +0x5c | 23 | GetLeaderboardName(u64) | 8 | 0xa7e120 | `..._GetLeaderboardName` | later |
| +0x60 | 24 | GetLeaderboardEntryCount(u64) | 8 | 0xa7e120 | `..._GetLeaderboardEntryCount` | later |
| +0x64 | 25 | GetLeaderboardSortMethod(u64) | 8 | 0xa7e120 | `..._GetLeaderboardSortMethod` | later |
| +0x6c | 27 | DownloadLeaderboardEntries(u64, request, start, end) -> SteamAPICall_t | 20 | 0xa7db80 | `..._DownloadLeaderboardEntries` | later |
| +0x74 | 29 | GetDownloadedLeaderboardEntry(u64, idx, LeaderboardEntry_t*, int32*, n) | 24 | 0xa7df10 | `..._GetDownloadedLeaderboardEntry` | later; struct is 32 bytes on the guest (pack 8), 28 natively (pack 4): convert |
| +0x78 | 30 | UploadLeaderboardScore(u64, method, score, details*, n) -> SteamAPICall_t | 24 | 0xa7db80 | `..._UploadLeaderboardScore` | REFUSE by default (profile write); opt-in env only |
| +0x7c | 31 | AttachLeaderboardUGC(u64 leaderboard, u64 ugc) -> SteamAPICall_t | 16 | 0xa7db80, 0xaabc80 | `..._AttachLeaderboardUGC` | REFUSE (needs a cloud FileShare handle) |

Offsets +0x00, +0x08, +0x10 (GetStat float, SetStat float, UpdateAvgRateStat), +0x1c (ClearAchievement), +0x20 (GetAchievementAndUnlockTime) were not seen in the exe; unmapped slots must log once and answer 0 (purge unknown, so only the observed set gets a live vtable).

**ISteamRemoteStorage, `STEAMREMOTESTORAGE_INTERFACE_VERSION016` (initData 0xc5c3a4, pFn 0x685ca0) - CLOUD, refuse all**

| Off | Slot | Method | Purge | Used by |
|---|---|---|---|---|
| +0x00 | 0 | FileWrite(name, data, cb) | 12 | 0x683710, 0x6839b0, 0xaabb10 (WRITE) |
| +0x04 | 1 | FileRead(name, buf, cb) | 12 | 0x683820 |
| +0x18 | 6 | FileDelete(name) | 4 | 0x9c8350, 0x9c8410 (DELETE, "GameState::Delete") |
| +0x1c | 7 | FileShare(name) -> SteamAPICall_t | 4 | 0xaabb10 (WRITE, shares to UGC) |
| +0x34 | 13 | FileExists(name) | 4 | 0x9c8350, 0x9c8410, 0x683820 |
| +0x3c | 15 | GetFileSize(name) | 4 | 0x683820 |
| +0x60 | 24 | UGCDownload(u64, priority) -> SteamAPICall_t | 12 | 0xaabbd0 |
| +0x6c | 27 | UGCRead(u64, buf, cb, offset, action) | 24 | 0xaabd90 |

Cloud-write calls found: **FileWrite (+0x00), FileDelete (+0x18), FileShare (+0x1c)**, plus `AttachLeaderboardUGC` in UserStats that depends on a FileShare handle. Bridge rule: never resolve any `SteamAPI_ISteamRemoteStorage_*` symbol, and keep the `0xc5c3a4` accessor slot NULL. Every function that touches this interface tests `*slot != 0` first (0x9c8350, 0xaabb10, 0x683820 checked) and takes its local-file arm otherwise, so NULL is the verified-safe answer and no fake vtable is needed. If a fake is ever installed, all eight offsets above return 0/false with the purges shown.

**ISteamUGC, `STEAMUGC_INTERFACE_VERSION020` (0xc5c48c, pFn 0x8fd3a0)** - read-only use only, in the mod loader 0x8fb120

| Off | Slot | Method | Purge |
|---|---|---|---|
| +0x128 | 74 | GetNumSubscribedItems() | 0 |
| +0x12c | 75 | GetSubscribedItems(PublishedFileId_t*, max) | 8 |
| +0x130 | 76 | GetItemState(u64) | 8 |
| +0x134 | 77 | GetItemInstallInfo(u64, u64* size, char* folder, cb, u32* timestamp) | 24 |

Flat entries exist (`SteamAPI_ISteamUGC_GetNumSubscribedItems`, `GetSubscribedItems`, `GetItemState`, `GetItemInstallInfo`). Per M7 mods come from the filesystem, so keep the slot NULL; these four are the only candidates if native Workshop discovery is ever wanted. Never forward `SubscribeItem`, `DownloadItem`, `CreateItem`, `SubmitItemUpdate`, `DeleteItem` or any `Set*`/`Add*`.

**ISteamApps `STEAMAPPS_INTERFACE_VERSION008` (0xc5c4e0)**: only +0x1c = slot 7 `BIsDlcInstalled(AppId_t)`, purge 4, 3 sites in 0x9ef5c0 (DLC app id 0x62200 at one of them). Flat: `SteamAPI_ISteamApps_BIsDlcInstalled`. Candidate for a live read-only pass-through (see 7.4).

**ISteamUtils `SteamUtils010` (0xc5c4ec)**: +0x0c slot 3 `GetServerRealTime()` (purge 0; 4 sites: 0x917030, 0x958bc0, 0xa61490, 0xa75c10); +0x14 slot 5 `GetImageSize(int, u32*, u32*)` (12) and +0x18 slot 6 `GetImageRGBA(int, u8*, int)` (12), both in 0xa23610 (avatars). Overlay-related methods are never called.

**ISteamFriends `SteamFriends017` (0xc5c4f8)**: the macOS dylib is v018 and lacks `SetPersonaName`, so game slot = dylib slot + 1 from slot 1 on. +0x00 `GetPersonaName`; +0x08 slot 2 `GetPersonaState`; +0x0c slot 3 `GetFriendCount(flags)`; +0x10 slot 4 `GetFriendByIndex(idx, flags)` (CSteamID returned through a hidden pointer, purge 12); +0x14 `GetFriendRelationship(u64)`; +0x18 `GetFriendPersonaState(u64)`; +0x1c `GetFriendPersonaName(u64)`; +0x74 slot 29 `ActivateGameOverlayToUser("steamid", u64)` (purge 12; flat `SteamAPI_ISteamFriends_ActivateGameOverlayToUser`). Sites in 0xa9e2d0, 0xa23410, 0xa23500, 0xa23580.

**ISteamUser `SteamUser023` (0xc5c510)**: +0x08 slot 2 `GetSteamID()` (hidden-pointer return, purge 4, 0xa9e2d0); +0x38 slot 14 `GetAuthTicketForWebApi("epiconlineservices")` (purge 4, 0xa60ec0, 0xa710a0; guarded by `*slot != 0`; result arrives as callback 168). The game sends this ticket to Epic Online Services. Do NOT forward: keep NULL (the game's own no-Steam arm) or keep faking a handle. In the dylib the same method is slot 12 (v024 dropped the two deprecated game-connection slots), so slots differ from the game's by +2 after slot 2.

**ISteamMatchmaking `SteamMatchMaking009` (0xc5c504) and ISteamNetworking `SteamNetworking006` (0xc5c51c)**: online co-op, out of scope, keep NULL. Observed: Matchmaking +0x10 RequestLobbyList, +0x14 +0x18 +0x1c +0x20 +0x24 +0x28 the lobby list filters, +0x30 GetLobbyByIndex, +0x4c GetLobbyData, +0x50 SetLobbyData, +0x5c DeleteLobbyData, +0x80 GetLobbyMemberLimit, +0x88 SetLobbyJoinable; Networking +0x00 SendP2PPacket, +0x08 ReadP2PPacket, +0x10 AcceptP2PSessionWithUser.

### 7.3 Entry points needed on the macOS dylib side

Always: `SteamAPI_InitFlat`, `SteamAPI_IsSteamRunning`, `SteamAPI_Shutdown`. For callbacks: `SteamAPI_GetHSteamPipe`, `SteamAPI_ManualDispatch_{Init,RunFrame,GetNextCallback,FreeLastCallback,GetAPICallResult}` (RunFrame only in M9; the rest when call results are wired). Interface pointer: `SteamAPI_SteamUserStats_v013` (the same version string the game asks for). Achievements and stats: `SteamAPI_ISteamUserStats_{GetAchievement,SetAchievement,StoreStats,GetStatInt32,SetStatInt32,IndicateAchievementProgress,GetAchievementProgressLimitsInt32}`. Leaderboard reads later: `..._{FindOrCreateLeaderboard,FindLeaderboard,GetLeaderboardName,GetLeaderboardEntryCount,GetLeaderboardSortMethod,DownloadLeaderboardEntries,GetDownloadedLeaderboardEntry}`. Never called or resolved: `SteamAPI_RestartAppIfNecessary`, every `SteamAPI_ISteamRemoteStorage_*`, `SteamAPI_ISteamUser_GetAuthTicketForWebApi`. All of the above exist as exports in the local dylib (checked with `nm`); `RequestCurrentStats` does not and is not needed.

### 7.4 How the shim answers today, and the M9 target

| Interface | Slot (initData) | Today (host_shims_steam.c) | M9 target |
|---|---|---|---|
| UserStats013 | 0xbf93c8 | Fake CSteamAPIContext vtable (purges unverified, see 7.1 item 2) | Live tokens for the 7 achievement/stat offsets; leaderboard offsets answer 0 until call results exist; +0x78/+0x7c refused |
| RemoteStorage016 | 0xc5c3a4 | NULL | NULL (unchanged) |
| UGC020 | 0xc5c48c | NULL | NULL |
| Apps008 | 0xc5c4e0 | NULL | NULL, optionally live `BIsDlcInstalled` (read-only) |
| Utils010 | 0xc5c4ec | NULL | NULL (fallback to local time/avatars); no overlay methods needed |
| Friends017 | 0xc5c4f8 | NULL | NULL; later optional `GetPersonaName` and `ActivateGameOverlayToUser` |
| Matchmaking009 / Networking006 | 0xc5c504 / 0xc5c51c | NULL | NULL |
| User023 | 0xc5c510 | Fake CSteamAPIContext vtable (+0x38 returns 1) | NULL is equivalent (the only two uses test `*slot != 0`); never forward the Epic ticket |

`SteamAPI_RegisterCallback` ids 168, 304, 506 and the call-result registrations are recorded and never fired today; with only achievements live nothing needs dispatching.

### 7.5 What is still unverified

- The purge of every token in the current shim, and the leaderboard state machine's behaviour when the UserStats slot is NULL versus live (0xa7db80 was not read to the end; the first four lines show it is gated by a game-side "Steam enabled" flag at `this+4`, set by 0xa60ec0).
- That all UserStats sites were found: the scan matches the accessor to the call within a function, so a call through a saved interface pointer would be missed. Re-run with a trace-only bridge (log every `(interface, offset)` reached) on the first live boot to close this.
- `LeaderboardEntry_t` and callback struct packing conversions (7.2) are from the SDK pack rules, not from game data.
- The overlay hooking an ANGLE/Metal present path remains untested (section 3.6).

Draft code for the bridge (loader, graceful failure, dry-run, cloud/leaderboard refusals) is in `.scratch/steam-research/mac_steam_draft.c` (not committed; compiles against the stub).

## 8. Implementation (M9, verified against the stub 2026-11-19)

### 8.1 What exists

- `native/mac/host/mac_steam.c` and `mac_steam.h` (Mac Build only, globbed by `build_native.py`, whole file behind `ISAAC_NATIVE`).
- One hook: `scripts/recomp/host/src/host_shims_steam.c`, `imp_steam_api__SteamInternal_ContextInit`, calls `mac_steam_context_init()` under `#if defined(ISAAC_NATIVE) && ISAAC_NATIVE`. It returns 0 when Steam is off, and the old code below it then runs unchanged. The Browser Build compiles the same file without the hook. `host_shims_misc.c` (RegisterCallback and friends) is untouched.
- Bring-up is lazy, at the first ContextInit. Order: the bridge is OFF unless `ISAAC_STEAM=1` (opt-in since 2026-11-19: a live bridge registers with the user's Steam client as app 250900), then `dlopen` of `<exe dir>/../Frameworks/libsteam_api.dylib`, `<exe dir>/libsteam_api.dylib`, `$ISAAC_STEAM_DYLIB` (the exe dir is resolved with `_NSGetExecutablePath` + `realpath`, no `@executable_path`). The first candidate that exists decides: if it will not load, the `dlerror` text is the reason (for example "incompatible architecture"). Then `SteamAPI_IsSteamRunning`, `setenv SteamAppId/SteamGameId=250900` when unset, `SteamAPI_InitFlat`, `SteamInternal_FindOrCreateUserInterface(SteamAPI_GetHSteamUser(), "STEAMUSERSTATS_INTERFACE_VERSION013")` (fallback `SteamAPI_SteamUserStats_v013`). Any failure logs one `[mac][steam] disabled: <reason>` and Steam stays off; `SteamAPI_Shutdown` runs from `atexit` when live.
- `SteamAPI_RestartAppIfNecessary` and every `SteamAPI_ISteamRemoteStorage_*` symbol are never resolved.

### 8.2 Behaviour when live

- Slot `0xbf93c8` (UserStats013): `initData+4 = 1`, `initData+8 = guest object`, ContextInit returns `slot+8` (7.1 item 3). The guest vtable has 45 slots; every slot first points at an "unmapped" token (logs the caller, answers 0, purge 0).
- Live methods, exact purges from 7.2: +0x04 GetStat(8), +0x0c SetStat(8), +0x14 GetAchievement(8), +0x18 SetAchievement(4), +0x24 StoreStats(0), +0x30 IndicateAchievementProgress(12), +0xac GetAchievementProgressLimits(12). Writes (SetStat, SetAchievement, StoreStats, Indicate) log every call with the native result; `ISAAC_STEAM_DRYRUN=1` logs `DRYRUN ...` and answers true without forwarding.
- Leaderboard offsets +0x54 +0x58 +0x5c +0x60 +0x64 +0x6c +0x74 answer 0 (EDX:EAX = 0) with their exact purges, logged once. +0x78 UploadLeaderboardScore and +0x7c AttachLeaderboardUGC are logged as REFUSED and never forwarded (their flat functions are not resolved either).
- ISteamUser (`0xc5c510`) is a second guest object when live (fix found by the first stub run): the game's own "Steam is up" flag (`0xc5ab08` bit 0, set in `0xa617e0`) needs BOTH `ContextInit(0xbf93c8)` and `ContextInit(0xc5c510)` non-NULL, and every achievement call is gated on it, so 7.4's "NULL is equivalent" is wrong for a live bridge. One token (purge 4) serves +0x08 GetSteamID (writes a zero CSteamID) and +0x38 GetAuthTicketForWebApi (REFUSED, answers 0, never forwarded); the offset is decoded from the caller's `call [eax+d8]`. All other ISteamUser slots point at the unmapped token.
- UserStats +0x50 is `ResetAllStats(bool)` (purge 4, caller `0x009aa878`: the game calls it with true when the stat `version` is not 1000). It was missing from 7.2. It is REFUSED (never forwarded; on a real account it would wipe stats and achievements).
- Every other accessor slot, including RemoteStorage (`0xc5c3a4`), is the NULL arm (`[slot] = 0`, return `slot`), as 7.4 targets. That is a change from the fake context ONLY when live; when Steam is off the old behaviour (fake context for `0xbf93c8` and `0xc5c510`) is byte-for-byte what ran before. ISteamUser basics (GetSteamID) were left out: the only verified use is the Epic ticket path, which must not be forwarded.
- Callbacks: registrations keep being recorded in `host_shims_misc.c`. `ManualDispatch_Init` is NOT called and nothing is dispatched (7.1 item 6: no achievement call needs a callback). TODO for leaderboards: ManualDispatch_Init, per-frame RunFrame + GetNextCallback/FreeLastCallback, pack-4 to pack-8 conversion, then the guest object's `Run`.
- Extras: `ISAAC_STEAM=0` forces Steam off. `ISAAC_STEAM_SELFTEST=1` drives GetAchievement, SetAchievement and StoreStats through the live tokens on a fabricated stack (name `ISAAC_SELFTEST_NOOP`) and logs result and purge; use with `ISAAC_STEAM_DRYRUN=1` against a real client.

### 8.3 Known compromise: token pool

A vtable slot must be a shim token whose identity is the method, and the generated table has no spare rows. `gen_shims.py` is outside the M9 file list, so when the bridge is live `mac_steam.c` re-binds `fn` and `arg_bytes` of 19 rows of `isaac_imports[]` (17 UserStats methods + the unmapped token + the ISteamUser token): first the 8 `CSteamAPIContext_*` tokens (dead when live, because the fake context is not built), then rows that are `NEVER_CALLED` with 0 call sites (7 `luaopen_*`, 3 `vcruntime140` rows, `DefWindowProcA`; the first run failed with `no re-bindable shim tokens` before the vcruntime and DefWindowProcA rows were admitted). Exactly 19 exist, so one more method needs the `gen_shims.py` follow-up. Nothing is touched when Steam is off. Clean follow-up: add 19 PROVIDED rows to `gen_shims.py`, regenerate, delete `pool_bind()`. If the pool is too small the bridge reports `disabled: no re-bindable shim tokens`.

### 8.4 Verifier steps and results

The bridge is off unless `ISAAC_STEAM=1`, so every live step sets it. Run with `scripts/mac/run.sh <scratch support dir> <log>` (see RUNBOOK section 5); `ISAAC_STEAM*` variables pass through. Before each live run, `output/Frameworks/libsteam_api.dylib` and `output/mac/libsteam_api.dylib` must not exist, and the log line `[mac][steam] dlopen ok (...)` must name the stub. Build: `python3 scripts/mac/build_native.py --jobs 3`. Common checks on every step: frame count reached with `guard intact`, no `[isaac][TRAP]`, no `UNMAPPED`, `grep -c CLOUD-WRITE` = 0.

Results, 2026-11-19, dev binary only, stub `.scratch/steam-research/libsteam_api_stub.dylib`, scratch folders under `.scratch/m9/`:

1. **Default off** (`HEADLESS=1 FRAMES=600`, nothing set): PASS. `[mac][steam] disabled: off by default; ISAAC_STEAM=1 turns it on`, then the old `fake context` lines for `0xbf93c8` and `0xc5c510`, `reached 773 frames (wanted 600); guard intact`. With `ISAAC_STEAM=1` and no dylib: PASS, `disabled: dlopen failed (libsteam_api.dylib not found; tried: <exe>/../Frameworks/..., <exe>/libsteam_api.dylib)`.
2. **Stub, live** (`ISAAC_STEAM=1 ISAAC_STEAM_DYLIB=<stub> ISAAC_STEAM_SELFTEST=1`): PASS after two fixes. First run: `disabled: no re-bindable shim tokens` (pool short by 2 rows), then a live run showed `UserStats UNMAPPED slot called from 0x009aa878` (ResetAllStats, see 8.2). Final run: `dlopen ok (...libsteam_api_stub.dylib)`, `[stub-steam] IsSteamRunning`, `InitFlat appid=250900`, `FindOrCreateUserInterface STEAMUSERSTATS_INTERFACE_VERSION013`, `live (app 250900, dryrun=0)`, selftest `purge=8 (want 8)`, `purge=4 (want 4)`, `purge=0 (want 0)`, `ContextInit(slot=0x00bf93c8) -> live ISteamUserStats013`, `ContextInit(slot=0x00c5c510) -> live ISteamUser`, `ISteamUser +0x38 GetAuthTicketForWebApi REFUSED`, the game's own `GetStat version -> value=7`, `ResetAllStats(+0x50) REFUSED`, `SetStat version=1000 -> 1`, `StoreStats -> 1`; `reached 618 frames; guard intact`.
3. **Dry run** (+ `ISAAC_STEAM_DRYRUN=1`): PASS. `DRYRUN SetAchievement ISAAC_SELFTEST_NOOP`, `DRYRUN StoreStats`, `DRYRUN SetStat version=1000`; 0 `[stub-steam] SetAchievement/StoreStats/SetStat` lines after the dry-run marker; 773 frames, guard intact.
4. **Game's own SetAchievement**: PASS via the game's own `PersistentGameData::TryUnlock`, NOT via the console. `lab_unlock_all.lua` (`LAB=lab_unlock_all.lua FRESH=1 SCRIPT='until=Room 1.2(Start Room):enter'`; the Layer only runs with at least one mod in the support folder, a dummy mod and a minimal `modscan.lua` were enough) logged `game achievements 1..637: 31 already unlocked, 606 newly unlocked, 0 failed` and the log has 637 `[mac][steam] SetAchievement <id> -> 1` (ids 1, 2 ... 637; the API name is the decimal id) and 638 `StoreStats -> 1`; 1507 frames, guard intact, 0 TRAP, 0 UNMAPPED, 0 CLOUD-WRITE. The console `achievement 1` / `achievement 2` (grave, typed, Enter, with `DBG=true`; windowed screenshots show the command accepted) produced no Steam call at all: the game never reached `0x929790` (no `unlock steam achievement` log, no ContextInit). Achievement 1 is unlocked from the start, but 2 is not, so the console command is not a trigger for this path (untested why; open).
5. **Wrong arch** (x86_64 build of the stub in `.scratch/m9/x86.dylib`): PASS. `disabled: dlopen failed (dlopen(...x86.dylib...): ... incompatible architecture (have 'x86_64', need 'arm64e, arm64e.v1, arm64, arm64'))`, old fake context lines, 773 frames, guard intact.
6. **Real dylib, Steam not running**: NOT RUN (user's: needs a copy of Steam's dylib; do not copy anything out of Steam's folders without the user).
7. **Cloud guard**: PASS. `grep -c CLOUD-WRITE` = 0 in the logs of steps 2, 3, 4, 5; nothing resolves a RemoteStorage symbol; the stub never ran an upload canary.
8. **Real client**: NOT RUN (user only). `ISAAC_STEAM=1 ISAAC_STEAM_DRYRUN=1` first. Note `ResetAllStats`, `UploadLeaderboardScore`, `AttachLeaderboardUGC` and the Epic auth ticket are refused by design.

Stub (`.scratch/steam-research/libsteam_api_stub.dylib`, arm64, install name `@rpath/libsteam_api.dylib`, 28 exports): everything the bridge resolves, the ManualDispatch set, ClearAchievement, and canaries (`UploadLeaderboardScore`, `AttachLeaderboardUGC`, `ISteamRemoteStorage_File{Write,Delete,Share,WriteAsync}`) that log `CLOUD-WRITE` if ever called.
