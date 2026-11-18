#!/usr/bin/env python3
"""Writes layer-info.json for the Launcher's Options tab (docs/mac/launcher.md "layer-info.json").

    gen_layer_info.py [--rgon DIR] [--enums FILE] [--lab-log FILE] [--game-version V] [--out FILE]

    --rgon          the Layer's Lua sources (default: scripts/recomp/rgon of this checkout)
    --enums         REPENTOGON's enums_ex.lua (its callback ids); or env REPENTOGON_ENUMS_EX.
                    Without it the callback numbers are left out of the file (they are optional).
    --lab-log       a log of a run with ISAAC_RGON_LAB=lab_names.lua ("lab_names: I <name>" / "lab_names: C <id>",
                    and "accepted, not supported in the browser: <name>" lines). Exact numbers; see below.
    --out           default: stdout

Numbers (the denominators are the gate's own, as docs/mac/layer-gaps.md proposes):
  layer_version        `local VERSION = "x.y.z"` in rgon.lua
  game_version         "game vX" in rgon.lua's load log line, else --game-version
  known_names          function names + global tables only REPENTOGON has: gate_names.lua (fns + globals)
  known_callbacks      distinct callback ids >= 1000 in enums_ex.lua
  implemented_names    how many of known_names the Layer has
  implemented_callbacks  how many of known_callbacks it fires
  unsupported_names    names it accepts and cannot act on yet

Without --lab-log these are STATIC estimates from the sources (no game was run): a known name counts as
implemented when its identifier appears in rgon.lua / api_*.lua (the estimate .scratch/layer-analysis used;
it over-counts a little, a name mentioned in a comment counts), a callback when its MC_ name appears there or its
id is in a `define(SLOT.x, { ids })`, and unsupported_names counts the literal `Class:Method` / `Class.Method`
first arguments of `unsupported(...)`. With --lab-log they are what REPENTOGON.Implemented held at run time
(class and method halves, as the gate's own HAVE / HAVECLASS tables). "source" in the output says which.
Wired into build.sh at M11.
"""
import argparse, datetime, json, os, re, sys

here = os.path.dirname(os.path.abspath(__file__))
repo = os.path.abspath(os.path.join(here, "..", "..", ".."))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rgon", default=os.path.join(repo, "scripts", "recomp", "rgon"))
    ap.add_argument("--enums", default=os.environ.get("REPENTOGON_ENUMS_EX"))
    ap.add_argument("--lab-log")
    ap.add_argument("--game-version")
    ap.add_argument("--out")
    a = ap.parse_args()

    def read(p):
        with open(p, encoding="utf-8", errors="replace") as f:
            return f.read()

    rgon = read(os.path.join(a.rgon, "rgon.lua"))
    m = re.search(r'^local VERSION\s*=\s*"([^"]+)"', rgon, re.M)
    if not m:
        sys.exit("gen_layer_info: no VERSION constant in rgon.lua")
    info = {"version": 1, "layer_version": m.group(1)}
    gv = a.game_version or (re.search(r"game v(\d+(?:\.\d+)+)", rgon) or [None, None])[1]
    if gv:
        info["game_version"] = gv

    gate = read(os.path.join(a.rgon, "gate_names.lua"))
    def block(k):
        mm = re.search(k + r"\s*=\s*\[\[(.*?)\]\]", gate, re.S)
        return set(mm.group(1).split()) if mm else set()
    fns, globs = block("fns"), block("globals")
    known = fns | globs
    info["known_names"] = len(known)

    layer_files = ["rgon.lua"] + sorted(f for f in os.listdir(a.rgon) if re.fullmatch(r"api_.*\.lua", f))
    text = "\n".join(read(os.path.join(a.rgon, f)) for f in layer_files)

    cb_ids = {}
    if a.enums and os.path.exists(a.enums):
        for name, num in re.findall(r"\b(MC_[A-Z0-9_]+)\s*=\s*(\d+)", read(a.enums)):
            if int(num) >= 1000:
                cb_ids[name] = int(num)
        info["known_callbacks"] = len(set(cb_ids.values()))
    else:
        print("gen_layer_info: no enums_ex.lua (--enums / REPENTOGON_ENUMS_EX): callback numbers left out", file=sys.stderr)

    if a.lab_log:
        have, ids, unsup = set(), set(), set()
        for line in read(a.lab_log).splitlines():
            mm = re.search(r"lab_names: I (\S+)", line)
            if mm:
                parts = re.split(r"[.:]", mm.group(1))
                have.add(parts[-1]); have.add(parts[0])
            mm = re.search(r"lab_names: C (\d+)", line)
            if mm:
                ids.add(int(mm.group(1)))
            mm = re.search(r"accepted, not supported in the browser: (\S+)", line)
            if mm:
                unsup.add(mm.group(1))
        info["implemented_names"] = len(known & have)
        info["unsupported_names"] = len(unsup)
        if "known_callbacks" in info:
            info["implemented_callbacks"] = len(ids & set(cb_ids.values()))
        info["source"] = "lab_names run"
    else:
        tokens = set(re.findall(r"[A-Za-z_][A-Za-z0-9_]*", text))
        info["implemented_names"] = len(known & tokens)
        unsup = set(re.findall(r'\bunsupported\(\s*"([A-Za-z]+[.:][A-Za-z_]+)"', text))
        info["unsupported_names"] = len(unsup)
        if "known_callbacks" in info:
            ids = {cb_ids[n] for n in set(re.findall(r"\bMC_[A-Z0-9_]+\b", text)) if n in cb_ids}
            for body in re.findall(r"define\(SLOT\.\w+,\s*\{([^}]*)\}", text):
                ids |= {int(x) for x in re.findall(r"\d+", body) if int(x) >= 1000}
            info["implemented_callbacks"] = len(ids & set(cb_ids.values()))
        info["source"] = "static estimate from the Lua sources"
    info["built_at"] = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    out = json.dumps(info, indent=2) + "\n"
    if a.out:
        os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
        with open(a.out, "w") as f:
            f.write(out)
    else:
        sys.stdout.write(out)

if __name__ == "__main__":
    main()
