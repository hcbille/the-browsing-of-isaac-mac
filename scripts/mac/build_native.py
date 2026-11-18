#!/usr/bin/env python3
"""Build the Mac Build's native arm64 executable from the lifted tree.

Same sources as scripts/recomp/lift/build_boot.py (the Host, the generated shim
tables, boot_integration.c, recomp_rt.c, the lift's stubs.c / dispatch_tbl.c
and lifted_*.c), compiled with the system clang for arm64 instead of emcc, plus
native/mac/host/*.c (the Guest region, real file I/O, the driver).

Guest memory goes through a base pointer (docs/adr/0001):
-DRECOMP_MEM_IDENTITY=0 for the lifted code, -DISAAC_NATIVE=1 for the Host.

Run build_boot.py once first: it applies the lift patches in place and writes
stubs.c and dispatch_tbl.c, which this script compiles as they are.

  python scripts/mac/build_native.py [--jobs N] [--lift-dir DIR] [--out DIR]
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import os
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RECOMP = REPO / "scripts" / "recomp"
HOST = RECOMP / "host"
LIFT = RECOMP / "lift"
MAC_HOST = REPO / "native" / "mac" / "host"
LUA_SRC = REPO / "tools" / "lua-5.3.3" / "src"

ARCH = ["-arch", "arm64"]
COMMON = ARCH + ["-DISAAC_NATIVE=1", "-DRECOMP_MEM_IDENTITY=0", "-fno-strict-aliasing"]
HOST_CFLAGS = COMMON + ["-O2", "-std=gnu11", "-w"]
LIFT_CFLAGS = COMMON + ["-O2", "-w", "-DRECOMP_MEM_CHECK=0"]

# Lua: the flags scripts/recomp/host/lua_build.py requires for the Guest's ABI
# (no LUA_COMPAT_5_2, no LUA_32BITS, no LUA_C89_NUMBERS; C, not C++).
LUA_CORE = """lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes
lparser lstate lstring ltable ltm lundump lvm lzio lauxlib lbaselib lbitlib
lcorolib ldblib liolib lmathlib loslib lstrlib ltablib lutf8lib loadlib
linit""".split()
LUA_CFLAGS = ARCH + ["-O2", "-std=gnu99", "-fno-strict-aliasing", "-w",
                     "-DLUA_USE_POSIX", "-DLUA_USE_LONGJMP"]

# The main thread runs the Guest's call chains on the native stack.
LDFLAGS = ARCH + ["-Wl,-stack_size,0x10000000", "-lm"]


def compile_one(src: Path, obj: Path, flags: list[str]) -> tuple[Path, int, str]:
    if obj.exists() and obj.stat().st_mtime >= src.stat().st_mtime:
        return obj, 0, ""
    obj.parent.mkdir(parents=True, exist_ok=True)
    r = subprocess.run(["clang", "-c", *flags, str(src), "-o", str(obj)],
                       capture_output=True, text=True)
    return obj, r.returncode, (r.stderr or "")[-3000:]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--lift-dir", type=Path, default=REPO / "output" / "recomp" / "lift" / "gu")
    ap.add_argument("--out", type=Path, default=REPO / "output" / "mac")
    args = ap.parse_args()

    lift_dir = args.lift_dir.resolve()
    for need in (lift_dir / "stubs.c", lift_dir / "dispatch_tbl.c", LUA_SRC / "lua.h"):
        if not need.exists():
            print("missing %s (run build_boot.py and lua_build.py first)" % need)
            return 2

    inc = ["-I", str(HOST / "include"), "-I", str(HOST / "generated"),
           "-I", str(LIFT), "-I", str(lift_dir), "-I", str(LUA_SRC)]
    obj_dir = args.out / "obj"

    jobs: list[tuple[Path, Path, list[str]]] = []
    for name in LUA_CORE:
        jobs.append((LUA_SRC / (name + ".c"), obj_dir / "lua" / (name + ".o"), LUA_CFLAGS))
    host_srcs = (sorted((HOST / "src").glob("*.c")) + sorted((HOST / "generated").glob("*.c"))
                 + [LIFT / "boot_integration.c"] + sorted(MAC_HOST.glob("*.c")))
    for s in host_srcs:
        jobs.append((s, obj_dir / "host" / (s.stem + ".o"), HOST_CFLAGS + inc))
    for s in [LIFT / "recomp_rt.c", lift_dir / "stubs.c", lift_dir / "dispatch_tbl.c"]:
        jobs.append((s, obj_dir / "rt" / (s.stem + ".o"), LIFT_CFLAGS + inc))
    lifted = sorted(lift_dir.glob("lifted_*.c"))
    # largest first, so the long poles start early
    lifted.sort(key=lambda p: -p.stat().st_size)
    for s in lifted:
        jobs.append((s, obj_dir / "lifted" / (s.stem + ".o"), LIFT_CFLAGS + inc))

    t0 = time.time()
    objs, failed = [], []
    with cf.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futs = [ex.submit(compile_one, *j) for j in jobs]
        for f in cf.as_completed(futs):
            obj, rc, err = f.result()
            if rc:
                failed.append(obj)
                print("FAIL %s\n%s" % (obj.name, err))
            else:
                objs.append(obj)
    print("compile: %d objects, %d failed, %.1fs" % (len(objs), len(failed), time.time() - t0))
    if failed:
        return 1

    exe = args.out / "isaac-mac"
    t1 = time.time()
    r = subprocess.run(["clang", *sorted(map(str, objs)), *LDFLAGS, "-o", str(exe)],
                       capture_output=True, text=True)
    err = (r.stderr or "").strip()
    if r.returncode:
        undef = [l for l in err.splitlines() if l.strip().startswith('"_')]
        print("link FAILED (%d undefined symbol line(s))" % len(undef))
        print("\n".join(err.splitlines()[:60]))
        return 1
    print("link: %s, %.1f MB, %.1fs" % (exe, exe.stat().st_size / 1e6, time.time() - t1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
