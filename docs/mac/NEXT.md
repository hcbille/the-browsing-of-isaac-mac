# Where the Mac Build stands

## Done

- Browser Build pipeline verified end to end on an M2 (Ghidra export,
  tables, lift, web module): boots, starts a run in Basement I, walks, fires,
  plays audio, exits cleanly.
- Graphics route proven: SDL3 + ANGLE on Metal (`docs/mac/graphics-spike.md`).
- Native Host: every Host TU compiles for arm64 with `-DISAAC_NATIVE=1
  -DRECOMP_MEM_IDENTITY=0`, and the changed TUs still compile for the web.
  - `isaac_g` / `isaac_va` through `recomp_mem_base`; `isaac_va` aborts on a
    pointer outside the Guest region.
  - Guest region, overflow segments and the fatal-pointer report:
    `native/mac/host/mac_guest_mem.c`.
  - Host→Guest leaks fixed: `GlobalAlloc`/`GlobalFree`, `sscanf` targets,
    image placement and guard, CPUID scratch, heap overflow segments.
  - `lua_getinfo` / `lua_getstack` marshal the 32-bit `lua_Debug`.
  - Emscripten clock/core-count stand-ins in `isaac_host.h`; the JS input poll
    is a weak hook natively.
  - File callbacks as real I/O: `native/mac/host/mac_fs.c`.
  - Headless driver: `native/mac/host/mac_main.c`.
- Native build script: `scripts/mac/build_native.py`.

## Next

M1 (headless main), M2 (window, ANGLE Metal, title and room render) and M3
(keyboard, mouse, gamepads with rumble) are done, see PROGRESS.md.

4. (done) Audio: see M4 in PROGRESS.md.
5. Steam (achievements), Workshop mod folder, the Launcher, the Layer.
