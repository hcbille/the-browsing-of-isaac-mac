#!/usr/bin/env python3
"""Stage the Layer for the Mac Build: the files the game serves at mods/repentogon/.

  python scripts/mac/stage_layer.py <out dir>

Same file list as scripts/recomp/rgon/pack.mjs (the Browser Build's packing of the
Layer), minus main.lua and metadata.xml: the Mac Build does not list the Layer as a
mod (ADR 0003), so it needs no mod shell. Output: rgon.lua, api_*.lua,
hook_slots.lua, gate_names.lua, enums_ex.lua (REPENTOGON's, GPL-2.0, unchanged),
LICENSE-REPENTOGON.txt, NOTICE.txt. M11 puts this folder at
Repentance+.app/Contents/Resources/layer (the game's default --layer-dir).
"""
from __future__ import annotations

import re
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RGON = REPO / "scripts" / "recomp" / "rgon"
REPENTOGON = REPO / "third_party" / "REPENTOGON"

NOTICE = """REPENTOGON-compatible Layer (Mac Build)

rgon.lua, its api_*.lua parts and the generated tables are part of The Browsing of Isaac and are
licensed with it (GPL-3.0-or-later). They are not REPENTOGON's code.

enums_ex.lua is REPENTOGON's (https://github.com/TeamREPENTOGON/REPENTOGON),
copied unchanged and licensed under the GNU General Public License version 2;
see LICENSE-REPENTOGON.txt.
"""


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__)
        return 0 if len(sys.argv) == 2 else 2
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    names = ["rgon.lua", "hook_slots.lua", "gate_names.lua"]
    names += sorted(p.name for p in RGON.iterdir() if re.fullmatch(r"api_[a-z_]+\.lua", p.name))
    for n in names:
        shutil.copyfile(RGON / n, out / n)
    enums = REPENTOGON / "repentogon" / "resources" / "scripts" / "enums_ex.lua"
    shutil.copyfile(enums, out / "enums_ex.lua")
    shutil.copyfile(REPENTOGON / "LICENSE", out / "LICENSE-REPENTOGON.txt")
    (out / "NOTICE.txt").write_text(NOTICE)
    total = sum(p.stat().st_size for p in out.iterdir())
    print(f"{out}: {len(names) + 3} files, {total} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
