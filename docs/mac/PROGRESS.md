# Mac Build progress

## M1 Headless main runs: done (pending Verifier)

Status: `isaac-mac ... main` loads every archive and Lua script, runs the game
past 20,000 presented frames (the intro cutscene, then the menu path) and exits
cleanly through a frame watchdog; the guard canary is intact.

Causes and fixes:
1. Large archives failed to open: `mac_main.c` chdirs into the instance while
   `g_mac_instance_dir` stayed relative. Both the instance and save dirs are now
   resolved with `realpath` first (`native/mac/host/mac_main.c`).
2. "attempt to load a text chunk (mode is '')": natively Guest address 0 is a
   readable page (like WebAssembly), so `S_ARG`/`P_ARG` in `host_lua.c` turned a
   Guest NULL into a pointer to a zero byte (""). Both now map Guest 0 to NULL
   (harmless for the Browser Build, where 0 already is NULL).
3. "Modding data path ... is not a writable directory" MessageBoxes: the native
   `getcwd` leaked the real instance path to the Guest (GetCurrentDirectoryA,
   GetModuleFileName*), so its `mods/` and `data/` keys missed the RAM
   filesystem. The Guest now sees cwd "/" as under Emscripten
   (`guest_getcwd` in `host_shims_misc.c`); the real chdir stays for the Host Lua loader.

Decisions: `ISAAC_MAC_RUN_FRAMES=N` starts a watchdog that, after N presented
frames, checks the guard, prints `[mac] reached N frames ... guard intact` and
exits 0 (unset: run until the game exits). stdout is line buffered.

Evidence: `ISAAC_MAC_RUN_FRAMES=20000 output/mac/isaac-mac output/recomp/host/isaac.segs.bin .scratch/game-bundle main .scratch/save-m1`
exits 0. Error lines match the Browser Build's boot log
(`output/recomp/local-smoke/page.log`) exactly: XInputGetCapabilitiesEx,
EOS handle, 7 optional `repentance_*.a` language archives, options config.
