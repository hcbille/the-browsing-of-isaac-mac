# Base-pointer survey (ADR 0001)

Measured 2026-11-18 on the upstream tree at the `mac-native` branch point.

## Lifted code: already supported

`scripts/recomp/lift/recomp_rt.h` has `RECOMP_MEM_IDENTITY`. With
`-DRECOMP_MEM_IDENTITY=0`, `RECOMP_PTR(a)` becomes `recomp_mem_base + (uint32_t)a`.
Every lifted load and store goes through `MEMR*`/`MEMW*`/`RECOMP_PTR`
(`lift.py`, the `OpCode.LOAD`/`STORE` arms), so the lift itself needs no change.
The oracle already builds with this flag (`oracle/wideops.py`), so the path is
exercised.

## Host code: two choke points plus a short list

`host/include/isaac_host.h`:

- `isaac_g(va)` Guest→Host, 280 call sites: becomes `recomp_mem_base + va`.
  The `isaac_r*/w*` helpers sit on it.
- `isaac_va(p)` Host→Guest, 19 call sites, plus 3 raw `(uint32_t)(uintptr_t)`
  casts. These are the risk: in wasm, Host-allocated memory is inside the
  4 GB Guest space, so a Host pointer is a valid Guest address. Natively it is
  not. Each site must either point into the Guest region or allocate there
  (`isaac_guest_alloc`).

| file | sites |
| --- | --- |
| host_shims_forward.c | 7 |
| host_lua.c | 6 |
| host_rgon.c | 3 |
| host_fastpath.c | 2 |
| host_shims_misc.c | 1 |
| host_shims_heap.c | 1 |
| host_boot.c | 1 |

Plus 17 explicit `(T *)(uintptr_t)` casts (host_lua.c 9, host_rgon.c 3,
host_gl_clientarrays.c 3, host_shims_misc.c 2) to audit the same way.

## First native compile (clang arm64, `-DRECOMP_MEM_IDENTITY=0`)

41 Host TUs syntax-checked with `-Wshorten-64-to-32 -Wpointer-to-int-cast`:
7 errors, 50 warnings. Errors: Emscripten headers or `emscripten_get_now` in
host_boot.c, host_shims_win.c, host_shims_misc.c, host_trap.c (fatal includes
may hide more behind them), and host_lua.c's `LUA_EXTRASPACE == 4` assert.
Warnings: 36 `(void *)uint32_t` casts in host_shims_crt.c, 14 `size_t`→`uint32_t`
narrowings in conv/module/gaps.

## Lua across a 64-bit Host

The 32-bit Guest and the native Lua no longer share struct layouts. Of the 67
Lua imports, only these pass Guest memory that native Lua would lay out
differently:

| import | Guest call sites | state |
| --- | --- | --- |
| `lua_tolstring`, `luaL_checklstring`, `luaL_tolstring` (`size_t *len`) | 17 / 36 / 1 | already safe: local `size_t`, 32-bit write-back |
| `lua_getinfo` (`lua_Debug *`) | 1 | **needs marshalling** |
| `lua_getstack` (`lua_Debug *`) | 0 | **needs marshalling** |

Guest `lua_Debug` is 100 bytes: `event`, five `const char *` (4 bytes each),
three ints, four chars, `short_src[60]`, `i_ci` (4). Native strings returned in
it may be C literals outside the Guest region (`"=[C]"`, `"main"`), so they
must be copied into Guest memory. `i_ci` points at a Lua `CallInfo`, which is
in the arena, so it round-trips through `isaac_va`.

No `luaL_Reg`, `luaL_Buffer`, `lua_Reader` or hook import is used. Lua already
allocates in the Guest arena (`isaac_lua_alloc`), so `lua_State *`, userdata
and strings are Guest-addressable. `LUA_EXTRASPACE` is 8 natively; the Guest's
`L - 4` access stays inside it, so the assert becomes `>= 4`.

## Heap overflow

`host_shims_heap.c` grows the Guest heap with 64 MiB segments from the Host
heap when the 384 MiB arena fills. Natively those segments must be carved from
the reserved Guest region, not `malloc`.

## Guest region

Reserve 4 GiB once (`mmap` PROT_NONE, anywhere), commit the ranges the
Browser Build's map uses (image at 0x00400000, TEB/PEB at 0x1a000000, shim
slots at 0x1b000000, Guest heap arena), and set `recomp_mem_base`.
