#!/usr/bin/env python3
"""lint.py -- the Mac Build's own lint: every correction caught in code becomes a rule here.

  python3 scripts/mac/lint.py            # lint the Mac files and our changes to scripts/recomp
  python3 scripts/mac/lint.py --list     # print the rules

Run it before every commit. Exit status 1 when a rule fires.

Adding a rule (the standing rule for this project):
  - a correction found in code becomes a rule in RULES below;
  - its message says what to do INSTEAD, not only what is wrong;
  - the same change removes every existing copy, so the lint passes clean.

Which files: the tracked files under the published Mac folders (native/mac, scripts/mac, docs/mac,
docs/adr, GLOSSARY.md) and the files under scripts/recomp that differ from upstream's base commit
(b986dd8), since those go into the published patch.
"""
import os
import re
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
BASE = 'b986dd8'
MAC_PATHS = ['native/mac', 'scripts/mac', 'docs/mac', 'docs/adr', 'GLOSSARY.md']
RGON = 'scripts/recomp/rgon'
HOME_PREFIX = '/' + 'Users' + '/'   # split so this file does not match its own rule


def git(*args):
    return subprocess.run(['git', *args], cwd=REPO, check=True, capture_output=True, text=True).stdout


def files():
    # tracked and not-yet-added files alike, so a new file is linted before its first commit
    mac = git('ls-files', '--cached', '--others', '--exclude-standard', '--', *MAC_PATHS).split()
    ours = git('diff', '--name-only', '--diff-filter=AM', BASE, '--', 'scripts/recomp').split()
    return sorted(set(f for f in mac + ours if os.path.isfile(os.path.join(REPO, f))))


def text_of(path):
    try:
        with open(os.path.join(REPO, path), encoding='utf-8') as f:
            return f.read()
    except (UnicodeDecodeError, OSError):
        return None


# ---- rules: each yields (path, line number or 0, message) -------------------------------------------

def rule_home_paths(fs):
    """No home-folder paths in published files (2026-11-19: one reached docs/mac/NEXT.md)."""
    for f in fs:
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            if HOME_PREFIX in line:
                yield (f, n, 'home-folder path in a published file: use a path relative to the repo, '
                             'a command-line argument or an environment variable, or describe the place '
                             '("the local project notes")')


def rule_layer_files_loaded(fs):
    """Every api_*.lua is named correctly and is in rgon.lua's load list (2026-11-19: api_eid.lua and
    api_stageapi.lua were committed while the rgon.lua line that loads them was not)."""
    rg = text_of(os.path.join(RGON, 'rgon.lua')) or ''
    m = re.search(r'for _, part in ipairs\(\{([^}]*)\}\)', rg)
    loaded = set(re.findall(r'"([^"]+\.lua)"', m.group(1))) if m else set()
    for f in git('ls-files', '--', RGON).split():
        name = os.path.basename(f)
        if not name.startswith('api_'):
            continue
        if not re.fullmatch(r'api_[a-z_]+\.lua', name):
            yield (f, 0, 'Layer file name must match api_[a-z_]+.lua or stage_layer.py will not stage it: '
                         'rename it (lowercase letters and underscores only)')
        elif name not in loaded:
            yield (f, 0, 'this Layer file is not in rgon.lua\'s load list, so it never runs: add "%s" to the '
                         'ipairs({...}) list in rgon.lua (before "api_hooks.lua") and commit both together' % name)


def rule_frame_line_prefix(fs):
    """The host's "[mac][frame]" timing line keeps its old fields and order (2026-11-19, framestats): new fields go at the
    END, so scripts/mac/framestats.py and older log readers still match."""
    t = text_of('native/mac/host/mac_gl.c') or ''
    pre = '[mac][frame] frames %u-%u: avg %.2f ms (%.1f fps) min %.2f max %.2f | game work avg %.2f max %.2f ms | %u over 20 ms'
    if t and pre not in t:
        yield ('native/mac/host/mac_gl.c', 0, 'the "[mac][frame] frames" line no longer starts with the old fields: restore '
                                              'them exactly and append any new field after "%u over 20 ms"')


def rule_part_loads_part(fs):
    """A Layer part never loads another part (2026-11-19: api_framectx.lua was loaded from api_eid.lua, which hides where
    parts come from). Parts are listed in rgon.lua's ipairs({...}) list only."""
    for f in fs:
        if os.path.basename(f).startswith('api_') and f.endswith('.lua'):
            for n, line in enumerate((text_of(f) or '').splitlines(), 1):
                if re.search(r'B\.load\(\s*"api_', line):
                    yield (f, n, 'a part must not B.load another api_*.lua: add the other file to the ipairs({...}) '
                                  'part list in rgon.lua instead (before "api_hooks.lua" unless it must run after it)')


def rule_lab_prints_done(fs):
    """A lab that counts PASS/FAIL must print a closing DONE line with the counts (2026-11-19: without it a run that
    stopped early or never reached its steps read as clean, since only the FAIL lines were counted)."""
    for f in fs:
        if re.match(r'.*/lab_[a-z0-9_]+\.lua$', f) and f.startswith(RGON + '/'):
            t = text_of(f) or ''
            if re.search(r'local pass, fail\b', t) and not re.search(r'done', t, re.I):
                yield (f, 0, 'a lab with a `local pass, fail` counter must end with a line like '
                             'B.log(string.format("lab_x: DONE %d PASS %d FAIL", pass, fail)) so a run cut short is visible')


def rule_inert_not_implemented(fs):
    """A stand-in that does nothing is not 'implemented' (2026-11-19: Shape and DebugRenderer, which draw nothing, were put
    in REPENTOGON.Implemented). Mark it with a `-- inert: Name Name` line in its Layer file."""
    for f in fs:
        if not f.endswith('.lua') or not f.startswith(RGON + '/'):
            continue
        t = text_of(f)
        if t is None:
            continue
        inert = [n for line in t.splitlines() for m in [re.match(r'\s*--\s*inert:\s*(.*)', line)] if m for n in m.group(1).split()]
        for n, line in enumerate(t.splitlines(), 1):
            if 'Implemented' not in line or re.match(r'\s*--', line):
                continue
            for name in inert:
                if re.search(r'Implemented\[\s*"%s[."]' % re.escape(name), line):
                    yield (f, n, '%s is marked inert (accepted, not supported) but is set in REPENTOGON.Implemented: leave inert '
                                 'stand-ins out of REPENTOGON.Implemented (the gate would show a mod a name that does nothing) and '
                                 'list them as "accepted, not supported" in the file and in layer-gaps.md' % name)


def rule_ctor_implemented_name(fs):
    """A constructor is counted as "Class.Class" by rgon_census.py and rgon_backlog.py (2026-11-19, chunk 8: the quad
    constructors were set as Implemented["SourceQuad"], so the backlog kept them missing while they worked)."""
    ctors = ('DestinationQuad', 'SourceQuad', 'Point', 'Beam', 'Sprite', 'Font', 'RNG', 'Color', 'Vector', 'KColor', 'BitSet128')
    for f in fs:
        if not f.endswith('.lua') or not f.startswith(RGON + '/'):
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            if re.match(r'\s*--', line):
                continue
            m = re.search(r'Implemented\[\s*"(%s)"\s*\]\s*=' % '|'.join(ctors), line)
            if m or re.search(r'Implemented\[\s*name\s*\]\s*=\s*true', line):
                yield (f, n, 'a constructor goes in REPENTOGON.Implemented as "Class.Class" (Implemented["%s.%s"] = true), the key the census '
                             'and the backlog look up; a bare class name or a bare variable counts for nothing' % ((m.group(1),) * 2 if m else ('Name', 'Name')))


def rule_host_math_header(fs):
    """A host C file that calls floor, ceil, sqrt, pow or the like includes <math.h> (2026-11-20, gfx: host_rgon.c's quad blit
    called floor()/ceil() with no declaration, which clang rejects as an implicit function declaration)."""
    fn = re.compile(r'\b(floor|ceil|sqrt|pow|fabs|round|trunc|fmod)\s*\(')
    for f in fs:
        if not (f.endswith('.c') and '/host/src/' in f):
            continue
        t = text_of(f)
        if t is None or '#include <math.h>' in t:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            if not re.match(r'\s*(/\*|\*|//)', line) and fn.search(line):
                yield (f, n, 'add #include <math.h> at the top of this file: it calls a math function without declaring it')
                break


def rule_image_render_not_registered(fs):
    """Image:RenderWithShader needs a shader program inside the engine; there is none, so it stays out of REPENTOGON.Implemented
    (2026-11-20, gfx; Image:Render itself is real since wave 6: screen route by a temporary Sprite). A name that is only half
    real stays unregistered and is listed as deferred."""
    for f in fs:
        if not f.endswith('.lua') or not f.startswith(RGON + '/') or f.endswith('lint.py'):
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            if re.match(r'\s*--', line):
                continue
            if re.search(r'Implemented\[\s*"(Image\.RenderWithShader)"\s*\]\s*=\s*true', line):
                yield (f, n, 'Image:RenderWithShader has no real route (no shader program here), so it is not '
                             'registered in REPENTOGON.Implemented; register it only when it draws what REPENTOGON draws')


def rule_layer_setsize_is_scale(fs):
    """LayerState:GetSize/SetSize is the layer's SCALE (default 1, 1), not the crop size: SetSize(32, 32) on a 64x64 layer drew a
    quad 32 times larger (2026-11-20, gfx2 probe). The crop rectangle is the anm2 frame's Width/Height plus
    LayerState:SetCropOffset; to draw a sub-rectangle, use an anm2 whose frame has that size (api_gfx.lua's blit sprites)."""
    for f in fs:
        if not f.endswith('.lua') or not f.startswith(RGON + '/') or f.endswith('lint.py'):
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            if re.match(r'\s*--', line) or 'function' in line:
                continue
            if re.search(r'[Ll]ayer[^\n]*:SetSize\(|:GetLayer\([^)]*\):SetSize\(', line):
                yield (f, n, 'LayerState:SetSize is the layer scale, not the crop size; set the crop with SetCropOffset and give the anm2 frame '
                             'the size you want (see api_gfx.lua screen_render)')


def rule_hurt_func_source(fs):
    """GridEntity::hurt_func (0x00709fb0) reads its first stack argument, the damage SOURCE entity (its type at +0x28), before
    anything else (2026-11-19, chunk 6: a call with 0 there dereferences null). Pass a real Entity* (the first player), never 0.
    Also: a name from names.json is only a signature match, so check its decompile before using the address as that function
    (FXLayers::AddPoopFx 0x4dd7f0 is really an NPC update)."""
    pat = re.compile(r'call\(\s*0x0*709fb0\s*,[^)]*\)\s*,\s*0\s*,|0x0*709fb0\s*,\s*\{[^}]*\}\s*,\s*0\s*,')
    for f in fs:
        if not f.endswith('.lua') or not f.startswith(RGON + '/'):
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            if pat.search(line):
                yield (f, n, 'hurt_func called with a null source entity: pass a real Entity* (Isaac.GetPlayer(0)) as the first stack argument')


def rule_global_pointer_plus_offset(fs):
    """0x00c71678 (Game), 0x00c7169c (Manager) and 0x00c71690 (LuaEngine) are VARIABLES that hold a pointer to the object
    (2026-11-19, B4: a lab read r32(0xc7169c + 8) and got 0 for the Manager's state; the field is r32(r32(0xc7169c) + 8)).
    Read the pointer first, then add the field offset."""
    pat = re.compile(r'\br(?:8|16|32|i32|f32)\(\s*0x0?0?c71(?:678|690|69c)\s*\+')
    for f in fs:
        if not f.endswith('.lua') or not f.startswith(RGON + '/'):
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            if re.match(r'\s*--', line):
                continue
            if pat.search(line):
                yield (f, n, 'a field read at (global variable address + offset): the game variables 0x00c71678 / 0x00c7169c / '
                             '0x00c71690 hold a pointer; read it first, e.g. r32(r32(0x00c7169c) + 8), never r32(0x00c7169c + 8)')


STATUS_FILES = ('NEXT.md', 'PROGRESS.md')


def rule_status_files_in_notes(fs):
    """Status and resume files live in the git-ignored notes/ folder, not in published docs (2026-11-19: NEXT.md and
    PROGRESS.md moved out of docs/mac/)."""
    for f in fs:
        if os.path.basename(f) in STATUS_FILES:
            yield (f, 0, 'status files are not committed: move it to notes/%s (git-ignored) and put any lasting '
                         'fact in the RUNBOOK or another docs/mac file' % os.path.basename(f))
            continue
        if f == 'scripts/mac/lint.py':
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            for name in STATUS_FILES:
                for m in re.finditer(re.escape(name), line):
                    if not line[:m.start()].endswith('notes/'):
                        yield (f, n, '%s is a git-ignored working note now: point at the published doc that '
                                     'holds the fact (RUNBOOK, launcher.md, window.md, layer-gaps.md), or write '
                                     '`notes/%s` if a local working note is meant' % (name, name))


def rule_flip_default(fs):
    """REPENTOGON's optional booleans default to true when the argument is left out (2026-11-19: Quad:Flip() did
    nothing because `if flip_x then` read a missing argument as false)."""
    for f in fs:
        if not f.endswith('.lua'):
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            if re.search(r'\bif\s+flip_[xy]\s+then', line):
                yield (f, n, 'a missing Flip argument means true in REPENTOGON: test `flip_x == nil or flip_x` (nil means true), '
                             'never the bare name')


def rule_const_table_fallback(fs):
    """A luabridge class's const table (what `const Entity*` arguments, such as MC_PRE_ENTITY_SPAWN's spawner,
    carry) may hang only off the class table (2026-11-19: Entity:ToDelirium was nil on that spawner, so Fiend Folio's
    callback failed)."""
    for f in fs:
        if not f.endswith('.lua'):
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            if re.search(r'return\s+rawget\(mt,\s*"__class"\),\s*rawget\(mt,\s*"__const"\)\s*end', line):
                yield (f, n, 'the const table can hang off the class table only: return `cls, rawget(mt, "__const") or '
                             'rawget(cls, "__const")` so methods added with method() also reach const objects')


def rule_lua_vararg_middle(fs):
    """A `...` that is not the last argument gives one value only, and `x and false or y` never gives false
    (2026-11-19: the PRE_x_COLLISION answers lost their meaning, and MC_USE_PILL's arguments after the first)."""
    for f in fs:
        if not f.endswith('.lua'):
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            code = line.split('--')[0]
            if re.search(r'\w\([^()]*\.\.\.\s*,', code):
                yield (f, n, 'a `...` followed by another argument is cut to its first value: take the values you need '
                             '(`local a, b, c = ...`) or use table.pack/table.unpack, then pass them in order')
            if re.search(r'\band\s+(false|nil)\s+or\b', code):
                yield (f, n, '`cond and false or x` always gives x: write an if/else that assigns each branch')


def rule_lua_mixed_table_index(fs):
    """A Lua table constructor that mixes `[n] = v` with bare values numbers the bare ones from 1, not from n+1
    (2026-11-20: a 41-entry character map written as `[0] = 21, 22, 23, ...` and `[21] = 0, 1, 2, ...` paired Judas, Keeper
    and others with the wrong characters; lab_entity_config caught it)."""
    for f in fs:
        if not f.endswith('.lua'):
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            code = line.split('--')[0]
            if re.search(r'\[-?\d+\]\s*=\s*[-\w.]+\s*,\s*-?\d+\s*[,}]', code):
                yield (f, n, 'a bare value after `[n] = v` in a table constructor is stored at index 1, 2, ... (not n+1): '
                             'give every entry its own `[key] = value`')


def rule_room_transition_stand_in(fs):
    """RoomTransition is the game's own object, made once in api_extra.lua (2026-11-19: a stand-in
    `RT.StartBossIntro` here said "no boss intro is started" while api_extra's real one existed; StageAPI hit a nil
    field when the stand-in was the one in place)."""
    for f in fs:
        if not f.endswith('.lua') or f.endswith('api_extra.lua'):
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            code = line.split('--')[0]
            if re.search(r'function\s+(RT|RTT|RoomTransition)[.:]StartBossIntro\b', code):
                yield (f, n, 'RoomTransition.StartBossIntro is the real call of the game in api_extra.lua: extend that '
                             'table, and do not make a second, inert one')


def rule_roomconfig_both_tables(fs):
    """RoomConfig and RoomConfigHolder are the same functions under two names (2026-11-19: GetRandomRoom was put on
    RoomConfig only, so Fiend Folio's RoomConfigHolder.GetRandomRoom call hit nil)."""
    for f in fs:
        if not f.endswith('.lua'):
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            code = line.split('--')[0]
            if f.endswith('api_roomconfig.lua'):
                if re.search(r'\bT\.static\(', code) and 'T.static(tbl, name, fname, f)' not in code:
                    yield (f, n, 'add a RoomConfig function with `on_both("Name", fn)` so RoomConfig and RoomConfigHolder '
                                 'both carry it; a bare T.static puts it on one table only')
            elif re.search(r'static\([^)]*"RoomConfig(Holder)?"\s*,\s*"', code):
                yield (f, n, 'RoomConfig functions are added in api_roomconfig.lua with `on_both("Name", fn)`, which puts '
                             'them on RoomConfig and RoomConfigHolder together')


def rule_orphan_starts_packed(fs):
    """orphan_starts.py keeps the packed-accessor test (2026-11-19: 0x0060d0e0, a getter right after a 16-byte setter with no int3,
    was dropped as 'inside a function' because the index merges unpadded neighbours; a mod's familiar then hit a TRAP)."""
    f = 'scripts/recomp/lift/orphan_starts.py'
    if f not in fs:
        return
    t = text_of(f) or ''
    for needle in ('def after_ret', 'endVa'):
        if needle not in t:
            yield (f, 0, 'orphan_starts.py must keep "%s": a function packed right after a ret (no int3) is a real entry, and Ghidra\'s '
                         'bodies (endVa), not the PE index\'s merged bounds, decide whether a start falls inside a function' % needle)


def rule_fast_setjmp(fs):
    """Every Host and lifted TU builds with setjmp/longjmp mapped to _setjmp/_longjmp (2026-11-19: Darwin's setjmp
    saves the signal mask and sigaltstack, two syscalls per host->guest call, 26.6% of the main thread with 95 mods)."""
    f = 'scripts/mac/build_native.py'
    if f not in fs:
        return
    t = text_of(f) or ''
    if not re.search(r'FAST_JMP\s*=\s*\[[^\]]*-Dsetjmp=_setjmp[^\]]*-Dlongjmp=_longjmp', t):
        yield (f, 0, 'define FAST_JMP = ["-Dsetjmp=_setjmp", "-Dlongjmp=_longjmp"]: Darwin\'s setjmp costs two syscalls per '
                     'host->guest call; if code must longjmp out of a signal handler, use sigsetjmp/siglongjmp there instead')
    for name in ('HOST_CFLAGS', 'LIFT_CFLAGS'):
        m = re.search(r'^%s\s*=.*$' % name, t, re.M)
        if not m or 'FAST_JMP' not in m.group(0):
            yield (f, t[:m.start()].count('\n') + 1 if m else 0,
                   '%s must end with "+ FAST_JMP" so every TU uses _setjmp/_longjmp (no signal-mask syscalls)' % name)


def rule_gate_class_aware(fs):
    """The gate matches a REPENTOGON name with the class it is used on, never by the bare method name (2026-11-19:
    EntityFamiliar.GetWeapon counted as present because EntityPlayer.GetWeapon existed)."""
    checks = [
        ('scripts/recomp/rgon/rgon.lua', ('G.HAVEP[', 'gate_has(G,', 'gate_receiver(G,'),
         'gate_weigh must look a use up as Class.Name (G.HAVEP via gate_has, the class from gate_receiver); a bare-name '
         'set (HAVE[name]) is only the fallback for names gate_names.lua lists no class for'),
        ('scripts/recomp/web/mods.mjs', ('receiverHint(code, m.index)',),
         'the call names of scanLua must carry the receiver hint (names.add(m[1] + receiverHint(code, m.index))), '
         'which the gate turns into a class'),
        ('native/mac/launcher/Sources/ModScan.swift', ('receiverHint(code, p)',),
         'ModScan.scanLua must add `name + receiverHint(code, p)` like mods.mjs scanLua (Tests/run.sh compares the two '
         'byte for byte), and bump scannerVersion when the scan changes'),
        ('scripts/recomp/rgon/gate_names.lua', ('  pairs = [[', '  rets = [['),
         'gate_names.lua lacks the class tables: run tools/gate_names.py (ISAAC_GAME_DIR=<game folder>) and commit its output'),
    ]
    for f, needles, msg in checks:
        if f not in fs:
            continue
        t = text_of(f) or ''
        for needle in needles:
            if needle not in t:
                yield (f, 0, '%s (missing `%s`)' % (msg, needle))


def rule_backlog_narrowing(fs):
    """The backlog tool must not count a mod's own names as REPENTOGON misses (2026-11-19, entity2: `Mod.Capsule` item tables,
    `x:GetSprite():Name` chains and variables named `*sprite` showed up as missing Shape.Capsule / ReplaceSpritesheet)."""
    f = 'scripts/mac/rgon_backlog.py'
    if f not in fs:
        return
    t = text_of(f) or ''
    for needle, msg in (
        ("(?:sprite|spr)$", 'a variable whose name ends in sprite/spr is a Sprite: narrow it (h = "Sprite") so its method calls are not counted against another class'),
        ("call chain ends in", 'a chain whose last call is GetSprite()/To*() has that return class whatever the head: narrow it to the return class'),
        ("MODACCESS", 'a mod\'s cached accessor chain (`Mod.Room():Name()`) returns the game\'s object of that name: narrow it via MODACCESS so Room:SetBackdropType is not counted against EntityTear'),
        ("'TearFlagsLib'", 'a mod\'s own library table (TearFlagsLib.FireSplitTear) is not a REPENTOGON class: keep it in MODISH so its functions are not counted as misses'),
        ("MODLIB", 'a call reached through a mod library table (REVEL.RoomLists[...]:AddRooms) is that library\'s own function: keep REVEL and StageAPI in MODLIB so it is not counted as a RoomConfig miss'),
        ("MODFIELD", 'a mod\'s cached field (`Mod.itemconfig:GetTaggedItems`) is the game\'s object of that name: narrow it via MODFIELD so ItemConfig.GetTaggedItems is not counted against another class'),
        ("field of the mod's own table", 'a dot access `x.Name` that is not called or constructed, where every REPENTOGON candidate is a function, is a field of the mod\'s own table: suppress it'),
    ):
        if needle not in t:
            yield (f, 0, msg + ' (missing `%s`)' % needle)


def rule_backlog_class_alias(fs):
    """The backlog tool must fold REPENTOGON's C++ metatable name `Item` into ItemConfig_Item (wave 5: AddCustomTag/HasCustomTag were
    implemented as ItemConfigItem.* yet listed as missing "Item.*", and StageAPI's own `:SetMusic` as RoomConfigStage.SetMusic)."""
    f = 'scripts/mac/rgon_backlog.py'
    if f not in fs:
        return
    t = text_of(f) or ''
    for needle, msg in (
        ("CLASS_ALIAS = {'Item'", 'keep CLASS_ALIAS {Item -> ItemConfig_Item} and run every class through norm(), so a name done on ItemConfigItem is not counted missing on the C++ name'),
        ("StageAPI.CustomStage", 'a SetMusic in a file that builds StageAPI.CustomStage is the custom stage\'s own method: suppress it instead of listing RoomConfigStage.SetMusic'),
    ):
        if needle not in t:
            yield (f, 0, msg + ' (missing `%s`)' % needle)


def rule_census_false_positives(fs):
    """The census and backlog tools must not count what is not REPENTOGON's as missing (wave 8 census-fix: 41 "used by mods" names
    were mods' own functions, six names were doc-heading artifacts, ColorModifier's fields and the EntitiesSaveState getters were done)."""
    checks = {
        'scripts/mac/rgon_census.py': (
            ('EXCLUDED = {', 'a name that is not REPENTOGON API (doc-heading artifact, documented-only) goes into the EXCLUDED table with its reason, not into the missing list'),
            ('VALUE_TYPE_FIELDS', 'fields of a value type the Layer has (ColorModifier.R) are not missing class variables: list them in VALUE_TYPE_FIELDS'),
            ('LIVE_CLASS_ALIAS', 'a class the docs and the Layer name differently (EntitiesSaveState / EntitySaveState) goes into LIVE_CLASS_ALIAS'),
            ('defined_names(', 'a name a mod defines itself (function X:Name, X.Name = function, Name = ...) is not a use of REPENTOGON\'s: subtract defined_names() per mod, as mods.mjs scanLua does'),
        ),
        'scripts/mac/rgon_backlog.py': (
            ('rc.EXCLUDED', 'skip the census tool\'s EXCLUDED names in the backlog too'),
            ('mod_defined', 'suppress an unnarrowed use of a name the mod defines itself, as the census tool does'),
        ),
    }
    for f, rows in checks.items():
        if f not in fs:
            continue
        t = text_of(f) or ''
        for needle, msg in rows:
            if needle not in t:
                yield (f, 0, msg + ' (missing `%s`)' % needle)


def rule_new_class_names_flagged(fs):
    """A class made with new_class() in a Layer file has its methods counted only when they are put into REPENTOGON.Implemented
    (wave 5: Camera.IsPosVisible/SetFocusPosition/SnapToPosition and FXParams.WaterColor worked since wave 4 but the census listed
    them missing). After the class table, add `for k in pairs(T) do R.Implemented["Class." .. k] = true end` once the lab reads them."""
    for f in fs:
        if not f.startswith(RGON + '/api_') or not f.endswith('.lua'):
            continue
        t = text_of(f) or ''
        for m in re.finditer(r'new_class\("(\w+)",\s*(\w+)\)', t):
            cls, var = m.group(1), m.group(2)
            if re.search(r'function\s+%s[:.]\w+' % re.escape(var), t) and ('Implemented["%s.' % cls) not in t:
                yield (f, 0, 'class %s (new_class) defines methods on %s but never flags them: add a `for k in pairs(%s) do R.Implemented["%s." .. k] = true end` '
                       'after the class, or mark them `-- inert: Name`' % (cls, var, var, cls))


def rule_leftovers_methods_in_lab(fs):
    """Every method api_leftovers.lua registers is named in lab_leftovers.lua (wave 5, worker misc: a name with no check is a name nobody proved)."""
    src = text_of(RGON + '/api_leftovers.lua')
    lab = text_of(RGON + '/lab_leftovers.lua')
    if src is None or lab is None:
        return
    names = set(re.findall(r'method\("(\w+)", "(\w+)"', src))
    names |= set(('HistoryHUD', n) for n in re.findall(r'\bdef\("(\w+)"', src))
    names |= set(('HistoryHUDItem', n) for n in re.findall(r'function ITEM_MT:(\w+)', src))
    for cls, name in sorted(names):
        if '"%s.%s"' % (cls, name) not in lab:
            yield (RGON + '/lab_leftovers.lua', 0, 'add "%s.%s" to NAMES and a check that calls it (getter by read-back, setter by effect, action by the game\'s reaction), '
                   'or leave the method out of api_leftovers.lua' % (cls, name))


def rule_isaac_misc_names_in_lab(fs):
    """Every name api_isaac_misc.lua registers is in lab_isaac_misc.lua's NAMES (wave 6, worker misc-a: a name with no check is a name nobody proved)."""
    src = text_of(RGON + '/api_isaac_misc.lua')
    lab = text_of(RGON + '/lab_isaac_misc.lua')
    if src is None or lab is None:
        return
    names = set(re.findall(r'method\("(\w+)", "(\w+)"', src))
    names |= set(re.findall(r'static\(\w+, "(\w+)", "(\w+)"', src))
    names |= set(('RoomDescriptor', n) for n in re.findall(r'DM\.(\w+) =', src))
    names |= set(('ProceduralItemManager', n) for n in re.findall(r'rawset\(M, "(\w+)"', src))
    names |= set(('ItemConfigCard', n) for n in re.findall(r'rawset\(pg, "(\w+)"', src))
    for cls, name in sorted(names):
        if '"%s.%s"' % (cls, name) not in lab:
            yield (RGON + '/lab_isaac_misc.lua', 0, 'add "%s.%s" to NAMES and a check that reads it back or shows the game\'s reaction, '
                   'or leave it out of api_isaac_misc.lua' % (cls, name))


def rule_empty_method_stand_in(fs):
    """A method registered with an empty body (`method(cls, name, function() end)`) is a stand-in that does nothing (wave 6: Room:UpdateColorModifier
    sat as one for weeks). Find the game's own function and call it, or leave the name out; never register a no-op."""
    for f in fs:
        if not f.startswith(RGON + '/api_') or not f.endswith('.lua'):
            continue
        text = text_of(f) or ''
        for i, line in enumerate(text.split('\n'), 1):
            mm = re.search(r'\b(method|m)\("(\w+)", "(\w+)", function\(\) end\)', line)
            if mm and not ('"%s"' % mm.group(3) in text and 'Implemented["%s." .. n] = nil' % mm.group(2) in text):
                yield (f, i, 'no-op method: call the game\'s own function (find it in the decompile) or leave the name unregistered')


def rule_entity_methods_in_lab(fs):
    """Every method api_npc.lua registers is called by lab_entity_classes.lua (2026-11-19: the entity-class chunk was given
    a lab that names each one; a name with no check is a name nobody proved)."""
    src = text_of(RGON + '/api_npc.lua')
    lab = text_of(RGON + '/lab_entity_classes.lua')
    if src is None or lab is None:
        return
    names = set(re.findall(r'method\("(\w+)", "(\w+)"', src)) | set(re.findall(r'static\([^,]+, "(\w+)", "(\w+)"', src))
    for cls, name in sorted(names):
        if '"%s.%s"' % (cls, name) not in lab:
            yield (RGON + '/lab_entity_classes.lua', 0, 'add "%s.%s" to NAMES and a check that calls it (getter by read-back, setter by '
                   'effect, action by the game\'s reaction), or leave the method out of api_npc.lua' % (cls, name))


def rule_tears_methods_in_lab(fs):
    """Every method api_tears_more.lua registers is named in lab_tears_more.lua (2026-11-19, tears: a name with no check is a name
    nobody proved; the lab's "Implemented" list and a check that calls it)."""
    src = text_of(RGON + '/api_tears_more.lua')
    lab = text_of(RGON + '/lab_tears_more.lua')
    if src is None or lab is None:
        return
    names = set(re.findall(r'method\("(\w+)", "(\w+)"', src)) | set(re.findall(r'static\([^,]+, "(\w+)", "(\w+)"', src))
    for cls, name in sorted(names):
        if '"%s.%s"' % (cls, name) not in lab:
            yield (RGON + '/lab_tears_more.lua', 0, 'add "%s.%s" to the Implemented list and a check that calls it, or leave the method out of api_tears_more.lua' % (cls, name))


def rule_read_of_call_result(fs):
    """A memory reader wrapped around call(): r32/ri32/rf32/r8/r16 READ GUEST MEMORY at the number they get (2026-11-19, entity3:
    `ri32((call(GetRandomWisp, ..)))` read memory at the collectible id and raised a garbled error). call's first result is eax
    itself: for an id or a count take it directly (`local v = call(..); if v >= 0x80000000 then v = v - 0x100000000 end`); only when
    eax really is a pointer to read through, say so with `-- eax is a pointer` on the line."""
    pat = re.compile(r'\b(?:ri32|r32|rf32|r8|r16)\(\(*\s*(?:B\.)?call\(')
    for f in fs:
        if not (f.startswith(RGON + '/') and f.endswith('.lua')):
            continue
        t = text_of(f)
        for n, line in enumerate((t or '').splitlines(), 1):
            if pat.search(line) and 'eax is a pointer' not in line:
                yield (f, n, 'a memory reader on call()\'s result reads memory at eax: take the result directly, or mark a real pointer with `-- eax is a pointer`')


def rule_type_of_callback_result(fs):
    """type(run(...)) raises 'bad argument #1 to type (value expected)' when no callback answers, because run() then returns NO value, not
    nil (2026-11-19, cb-c: every grid update hook errored on its first call and the hurt hook lost its `after` state). Take the first
    result into a local: `local res = run(...)`, then `if type(res) == "boolean"`; `(run(...))` in parentheses also gives exactly one."""
    pat = re.compile(r'\btype\(\s*(?:T\.)?(?:run|run_by_rule|RunWithParam)\(')
    for f in fs:
        if not (f.startswith(RGON + '/') and f.endswith('.lua')):
            continue
        t = text_of(f)
        for n, line in enumerate((t or '').splitlines(), 1):
            if pat.search(line):
                yield (f, n, 'type(run(...)) errors when the callback answers nothing (no value, not nil): `local res = run(...)` first, then type(res)')


def rule_item_name_is_a_key(fs):
    """ItemConfigItem.Name is the localization KEY ('#THE_INNER_EYE_NAME'), not a name Isaac.GetItemIdByName knows (2026-11-19, pickup:
    a lab fed `config.Name` to GetItemIdByName and got -1; the English name 'The Inner Eye' answered 2)."""
    pat = re.compile(r'GetItemIdByName\(\s*[^()]*\.Name\b')
    for f in fs:
        if not (f.startswith(RGON + '/') and f.endswith('.lua')):
            continue
        t = text_of(f)
        for n, line in enumerate((t or '').splitlines(), 1):
            if pat.search(line):
                yield (f, n, 'GetItemIdByName(x.Name) looks up a localization key and answers -1: pass the English name as a string literal, '
                             'or find the id by scanning Isaac.GetItemConfig():GetCollectibles()')


def rule_conception_pairing(fs):
    """Cambion and Immaculate Conception hook rows pair as measured (2026-11-19, cba: the two were swapped once by guessing from
    function order; the collectible ids settle it: 412 = Cambion, 413 = Immaculate)."""
    want = {'GiveBirthCambion': '0x00775e1b', 'GiveBirthImmaculate': '0x007c81a6'}
    for f in fs:
        if not f.endswith(('hook_table.py', 'lift_patches.py', 'hook_slots.lua')):
            continue
        t = text_of(f) or ''
        for i, line in enumerate(t.splitlines(), 1):
            for name, va in want.items():
                other = [v for k, v in want.items() if k != name][0]
                if name + '@' in line and other.replace('0x00', '0x') in line.lower().replace('0x00', '0x'):
                    yield (f, i, '%s belongs at %s: find the function whose collectible check is 0x19c (Cambion, 412) or 0x19d (Immaculate, 413) '
                           'instead of assuming from function order' % (name, va))


def rule_health_field_offsets(fs):
    """The Layer's player health fields sit at measured offsets, not REPENTOGON's (2026-11-19, player-health: v1.9.7.12 has them
    0x14 to 0x24 lower; Candy Heart 0x1d9c -> 0x1db0, Soul Locket 0x1da8 -> 0x1dbc, Immaculate 0x191c -> 0x1930, Cambion 0x1920 -> 0x1934,
    familiar flags 0x1924 -> 0x1938, Red Stew 0x1e58 -> 0x1e74, Maggy drain 0x1ed0 -> 0x1ef4, Keeper's Sack 0x1f98 -> 0x1fbc)."""
    bad = re.compile(r'(?<![0-9A-Za-z_])0x(191c|1920|1924|1d9c|1da8|1ed0|1e58|1f98)(?![0-9A-Za-z_])', re.I)
    for f in fs:
        if not re.search(r'rgon/api_[a-z_]+\.lua$', f):
            continue
        for i, line in enumerate((text_of(f) or '').splitlines(), 1):
            code = line.split('--', 1)[0]
            m = bad.search(code)
            if m:
                yield (f, i, '%s is REPENTOGON\'s offset for v1.9.7.12: use the one measured in this build (see api_player_health.lua\'s table, '
                       'or measure it with a diff probe like lab_player_health_probe.lua); never write to a guessed offset' % m.group(0))


def rule_cacheflag_literals(fs):
    """CacheFlag values come from the game's CacheFlag table, never as numbers (2026-11-19, player-health: CACHE_LUCK was written as 32,
    which is CACHE_TEARFLAG; CACHE_LUCK is 1 << 10 and CACHE_ALL is 0xffff, so Luck bonuses never moved Luck)."""
    bad = re.compile(r'\{\s*1\s*,\s*2\s*,\s*(?:4|8)\s*,\s*(?:8|4)\s*,\s*16\s*,\s*32\s*\}')
    for f in fs:
        if not re.search(r'rgon/(api|lab)_[a-z_0-9]+\.lua$', f):
            continue
        for i, line in enumerate((text_of(f) or '').splitlines(), 1):
            if bad.search(line.split('--', 1)[0]):
                yield (f, i, 'a table of cache flag numbers: read CacheFlag.CACHE_DAMAGE, CACHE_FIREDELAY, CACHE_RANGE, CACHE_SHOTSPEED, '
                       'CACHE_SPEED, CACHE_LUCK from the game\'s CacheFlag table (LUCK is 1 << 10, 32 is TEARFLAG)')


def rule_lua_format_options(fs):
    """luaL_error/lua_pushfstring take only %d %s %f %p %c %% (2026-11-19: host_rgon.c's "0x%08x" messages came out as
    "invalid option '%0' to 'lua_pushfstring'" and hid the real error behind EntityFamiliar.GetRandomWisp)."""
    call = re.compile(r'\b(luaL_error|lua_pushfstring)\s*\(\s*L\s*,\s*"((?:[^"\\]|\\.)*)"')
    bad = re.compile(r'%(?![dsfpcI%])')
    for f in fs:
        if not f.endswith(('.c', '.h')):
            continue
        t = text_of(f) or ''
        for m in call.finditer(t):
            if bad.search(m.group(2).replace('%%', '')):
                yield (f, t[:m.start()].count('\n') + 1,
                       '%s formats with Lua\'s own formatter (only %%d %%s %%f %%p %%c %%%%): format hex or widths with snprintf '
                       'first (e.g. hex32() in host_rgon.c) and pass the text as %%s' % m.group(1))


def rule_host_string_to_guest(fs):
    """A Lua shim must never return a Host C literal with ret_ptr (2026-11-19: lua_typename's "boolean" is in the Host
    image, so EntityPlayer:GetBombVariant died with "Host pointer ... outside the Guest region"). Strings that Lua
    itself owns live in the Guest arena and are fine; a literal or static string goes through guest_str()."""
    bad = re.compile(r'ret_ptr\s*\(\s*cpu\s*,\s*(lua_typename|luaL_typename|lua_getupvalue|lua_getlocal|lua_setupvalue)\b')
    for f in fs:
        if not f.endswith('host_lua.c'):
            continue
        t = text_of(f) or ''
        for m in bad.finditer(t):
            if 'host-string-ok' in t[max(0, m.start() - 200):m.start()]:
                continue    # the web build's #else branch: literals are in linear memory there
            yield (f, t[:m.start()].count('\n') + 1,
                   '%s returns a static Host string: store guest_str(%s(...)) in cpu->EAX instead of ret_ptr' % (m.group(1), m.group(1)))


def rule_pgrep_game_by_name(fs):
    """Detect a running game with `pgrep -x isaac-mac`, never `pgrep -f` with a path (2026-11-19: heavy.sh waited 40 min
    for a game that was not running, because -f matched the waiting worker's own parent shell, whose command line
    mentioned output/mac/isaac-mac)."""
    for f in fs:
        if not (f.endswith('.sh') or f == 'docs/mac/RUNBOOK.md'):
            continue
        for n, line in enumerate((text_of(f) or '').splitlines(), 1):
            if re.search(r'pgrep\s+-[a-z]*f[a-z]*\s+.*isaac-mac', line):
                yield (f, n, 'use `pgrep -x isaac-mac` (a process NAMED isaac-mac): `pgrep -f` with a path also matches '
                             'the shell command line of any caller that mentions the path, so a waiting job blocks on its own parent')


def rule_web_host_mem_base(fs):
    """Host TUs also build for the browser (build_boot.py --web, 32-bit wasm), where there is no recomp_mem_base
    (2026-11-19: the heap-debug code broke the web build of host_lua.c)."""
    for f in fs:
        if not (f.startswith('scripts/recomp/host/src/') and f.endswith('.c')):
            continue
        t = text_of(f) or ''
        stack = []
        for n, line in enumerate(t.splitlines(), 1):
            st = line.strip()
            m = re.match(r'#\s*(if|ifdef|ifndef|endif)\b(.*)', st)
            if m:
                if m.group(1) == 'endif':
                    if stack:
                        stack.pop()
                elif m.group(1) == 'else' :
                    pass
                else:
                    stack.append(m.group(1) == 'if' and 'ISAAC_NATIVE' in m.group(2) and '!' not in m.group(2))
                continue
            if st.startswith(('/*', '*', '//')):
                continue
            if 'recomp_mem_base' in line and not any(stack):
                yield (f, n, 'recomp_mem_base does not exist in the web build: use isaac_g(0) for the Guest base (it is 0 '
                             'there), or put the code under #if defined(ISAAC_NATIVE) && ISAAC_NATIVE')


def rule_insn_hook_giant_entry(fs):
    """An instruction hook inside the split item-use giant names its lift entry, 0x005b39d0 (2026-11-19, cb-b: rows for sites in
    the Void and Compost cases named the pequery function 0x005ba56f and the apply failed with "in 0 TU(s)"; the lift TU is the
    whole function). Also the item cases are found from the collectible id, not by looking at a loop: Void is 477 (0x1dd), not 0x206."""
    row = re.compile(r'^\s*\((\d+),\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*"')
    for f in fs:
        if not f.endswith('lift_patches.py'):
            continue
        for i, line in enumerate((text_of(f) or '').splitlines(), 1):
            m = row.match(line)
            if m and 0x005b39d0 < int(m.group(3), 16) < 0x005d0000 and int(m.group(2), 16) != 0x005b39d0:
                yield (f, i, 'the site %s is inside the item-use giant: use 0x005b39d0 as the function (the lift entry, a split '
                       'TU), and find the case through the byte table 0x005c7760 and jump table 0x005c7598 from the collectible id'
                       % m.group(3))


def rule_stdstring_read(fs):
    """A std::string field of a game object is read with B.stdstr(addr), not B.str(addr) (2026-11-19, game-misc: BossPool's name read with
    B.str returned the string's SSO bytes or a pointer, not the text; the 16 bytes at the field are a std::string)."""
    for f in fs:
        if not (f.startswith(RGON + '/api_') and f.endswith('.lua')):
            continue
        for i, ln in enumerate((text_of(f) or '').split('\n'), 1):
            if re.search(r'B\.str\([^)]*\)', ln) and re.search(r'name|Name', ln) and '--' not in ln.split('B.str')[0]:
                yield (f, i, 'a std::string name field is read with B.stdstr(addr); B.str(addr) is for a plain C string: use B.stdstr here')


def rule_add_cache_flags_one_arg(fs):
    """The Layer's own Lua never calls player:AddCacheFlags with a second argument (2026-11-19, ffkeys: the game's AddCacheFlags
    takes only the flags and never evaluates, so mods that passed REPENTOGON's `true` got no key ghosts; api_cacheflags.lua now
    wraps it for mods). In Layer code call player:AddCacheFlags(flags) and then player:EvaluateItems() yourself."""
    call = re.compile(r':AddCacheFlags\([^()]*,')
    for f in fs:
        if not (f.startswith('scripts/recomp/rgon/') and f.endswith('.lua')) or f.endswith('api_cacheflags.lua'):
            continue
        for i, line in enumerate((text_of(f) or '').splitlines(), 1):
            if call.search(line) and not line.lstrip().startswith('--'):
                yield (f, i, 'AddCacheFlags takes only the flags in the game: call player:AddCacheFlags(flags) then '
                       'player:EvaluateItems() (the second argument is for mods, handled by api_cacheflags.lua)')


def rule_misc_b_methods_in_lab(fs):
    """Every method api_misc_b.lua registers (and GetGroup/SetGroup) is named in lab_misc_b.lua, and no Layer file calls GridEntity:HurtDamage "unproven" (wave 6, worker misc-b: GridEntity::hurt_func hurts the ENTITY argument with the grid's damage, it never changes a poop or rock; the first probe watched the wrong thing and left the name out)."""
    src = text_of(RGON + '/api_misc_b.lua')
    lab = text_of(RGON + '/lab_misc_b.lua')
    if src is not None and lab is not None:
        names = set(re.findall(r'method\("(\w+)", "(\w+)"', src))
        names |= set(('RoomDescriptor', n) for n in re.findall(r'DM\.(\w+) =', src))
        for cls, name in sorted(names):
            if '"%s.%s"' % (cls, name) not in lab:
                yield (RGON + '/lab_misc_b.lua', 0, 'add "%s.%s" to the Implemented list of first_part and a check that calls it (getter by read-back, '
                       'setter by effect, action by the game\'s reaction), or leave the method out of api_misc_b.lua' % (cls, name))
    for f in fs:
        if not (f.startswith('scripts/recomp/rgon/') and f.endswith('.lua')):
            continue
        for i, line in enumerate((text_of(f) or '').splitlines(), 1):
            if 'HurtDamage' in line and 'effect unproven' in line:
                yield (f, i, 'GridEntity:HurtDamage works (api_misc_b.lua): hurt_func damages the Entity passed in, so test it by that '
                       'entity\'s hit points (a spikes entity hurting a Gaper), not by a poop or rock state')


def rule_data_values_names_in_lab(fs):
    """Every name api_data_values.lua registers (T.static on Isaac, R.Implemented, the `install` getters, the EntityDesc methods) is
    named in lab_data_values.lua's Implemented list (wave 8, worker data-values: the ItemConfigCard properties were first installed
    through T.property, which found no class and registered nothing; only the lab's Implemented check showed it)."""
    src = text_of(RGON + '/api_data_values.lua')
    lab = text_of(RGON + '/lab_data_values.lua')
    if src is None or lab is None:
        return
    names = set('Isaac.' + n for n in re.findall(r'T\.static\(Isaac, "Isaac", "(\w+)"', src))
    names |= set('ItemConfigCard.' + n for n in re.findall(r'install\("(\w+)"', src))
    names |= set('EntityDesc.' + n for n in re.findall(r'function D:(\w+)\(', src))
    names |= set(re.findall(r'R\.Implemented\["([\w.]+)"\] = true', src))
    for n in sorted(names):
        if '"%s"' % n not in lab:
            yield (RGON + '/lab_data_values.lua', 0, 'add "%s" to the Implemented list of names() and a check that calls it, or leave it out of api_data_values.lua' % n)


def rule_host_misc_names_in_lab(fs):
    """Every name api_host_misc.lua registers (T.static on Isaac or Input) is named in lab_host_misc.lua's Implemented list and
    called there (wave 9, worker host-bridge: a Host-backed name that is registered but never run is the kind a mod finds first)."""
    src = text_of(RGON + '/api_host_misc.lua')
    lab = text_of(RGON + '/lab_host_misc.lua')
    if src is None or lab is None:
        return
    names = set(c + '.' + n for c, n in re.findall(r'static\((Isaac|Input), "\1", "(\w+)"', src))
    for n in sorted(names):
        if '"%s"' % n not in lab:
            yield (RGON + '/lab_host_misc.lua', 0, 'add "%s" to the Implemented list of the lab and a check that calls it, or leave it out of api_host_misc.lua' % n)
        elif n.split('.')[1] not in lab.replace('"%s"' % n, ''):
            yield (RGON + '/lab_host_misc.lua', 0, 'the lab names "%s" but never calls it: add a check that calls %s' % (n, n))


def rule_scan_global_assignment(fs):
    """A mod's own global (`Dump = include(...)` in Epiphany) defines that name: the mod scan must not count `Dump(` as
    REPENTOGON's Vertex:Dump (2026-11-20). Both scan implementations carry the rule, and the scanner version moves."""
    checks = [
        ('scripts/recomp/web/mods.mjs', ('SCAN_ASSIGNED', 'assignedAt('),
         'scanLua must add a global `Name = ...` at statement start to `defined` (SCAN_ASSIGNED, assignedAt), so the mod\'s '
         'own global is not listed as a REPENTOGON name'),
        ('native/mac/launcher/Sources/ModScan.swift', ('SCAN_ASSIGNED', 'assignedAt('),
         'ModScan.scanLua must add a global `Name = ...` at statement start to `defined` exactly like mods.mjs scanLua '
         '(Tests/run.sh compares the two), and bump scannerVersion when the scan changes'),
    ]
    for f, needles, msg in checks:
        if f not in fs:
            continue
        t = text_of(f) or ''
        for needle in needles:
            if needle not in t:
                yield (f, 0, '%s (missing `%s`)' % (msg, needle))


def rule_locate_names_in_lab(fs):
    """Every name api_locate.lua registers is named in lab_locate.lua, and PlayerManager.RemoveCoPlayer clears the other player's twin link (wave 7, worker locate: the game's RemoveCoPlayer frees the player but leaves the twin pointer at +0x1e68 of the other player dangling, which the lab's first run caught as GetOtherTwin answering freed memory)."""
    src = text_of(RGON + '/api_locate.lua')
    lab = text_of(RGON + '/lab_locate.lua')
    if src is None or lab is None:
        return
    names = set(re.findall(r'method\("(\w+)", "(\w+)"', src))
    names |= set(re.findall(r'T\.static\(\w+, "(\w+)", "(\w+)"', src))
    for cls, name in sorted(names):
        if '"%s.%s"' % (cls, name) not in lab:
            yield (RGON + '/lab_locate.lua', 0, 'add "%s.%s" to the Implemented list of run() and a check that calls it (getter by read-back, '
                   'setter by effect, action by the game\'s reaction), or leave the name out of api_locate.lua' % (cls, name))
    if 'RemoveCoPlayer' in src and '0x1e68' not in src.split('RemoveCoPlayer", function')[-1]:
        yield (RGON + '/api_locate.lua', 0, 'after the game\'s RemoveCoPlayer, write 0 over every remaining player\'s twin link (+0x1e68) that still holds '
               'the removed player: the game leaves it dangling')


def rule_familiar_pickup_names_in_lab(fs):
    """Every name api_familiar_pickup.lua registers is named in lab_familiar_pickup.lua, and GridEntityRock.SpawnDrops passes the game's fifth stack word (wave 8, worker familiar-pickup: REPENTOGON's signature has four stack words after ecx/edx, but all three callers of 0x0071b410 in v1.9.7.17 push five, the last an Entity* source; a call with four reads one word past the caller's pushes)."""
    src = text_of(RGON + '/api_familiar_pickup.lua')
    lab = text_of(RGON + '/lab_familiar_pickup.lua')
    if src is None or lab is None:
        return
    names = set(re.findall(r'method\("(\w+)", "(\w+)"', src))
    names |= set(re.findall(r'static\(\w+, "(\w+)", "(\w+)"', src))
    for cls, name in sorted(names):
        if '"%s.%s"' % (cls, name) not in lab:
            yield (RGON + '/lab_familiar_pickup.lua', 0, 'add "%s.%s" to the Implemented list of names() and a check that calls it (getter by read-back, '
                   'setter by effect, action by the game\'s reaction), or leave the name out of api_familiar_pickup.lua' % (cls, name))
    if 'call(0x0071b410' in src and 'backdrop & 0xffffffff, 0)' not in src:
        yield (RGON + '/api_familiar_pickup.lua', 0, 'call GridEntityRock SpawnDrops (0x0071b410) with five stack words (variant, seed, unk, backdrop, source Entity* or 0): '
               'this build\'s function reads a fifth that REPENTOGON\'s signature does not list')
def rule_npc_entity_misc_names_in_lab(fs):
    """Every name api_npc_entity_misc.lua registers is named in lab_npc_entity_misc.lua, and no Lua-only name is registered for a behaviour that needs a game hook (wave 10, worker npc-entity-misc: EntityNPC Set/Get/ClearFlyingOverride only mean something when Entity::IsFlying obeys the override, so a table of overrides without a hook is an inert stand-in)."""
    src = text_of(RGON + '/api_npc_entity_misc.lua')
    lab = text_of(RGON + '/lab_npc_entity_misc.lua')
    if src is None or lab is None:
        return
    names = set(re.findall(r'method\("(\w+)", "(\w+)"', src))
    names |= set(re.findall(r'static\(\w+, "(\w+)", "(\w+)"', src))
    names |= set(('EntityNPC', n) for n in re.findall(r'npc_loot\("(\w+)"', src))
    for cls, name in sorted(names):
        if '"%s.%s"' % (cls, name) not in lab:
            yield (RGON + '/lab_npc_entity_misc.lua', 0, 'add "%s.%s" to the Implemented list of names() and a check that calls it (getter by read-back, '
                   'setter by effect, action by the game\'s reaction), or leave the name out of api_npc_entity_misc.lua' % (cls, name))
    if 'FlyingOverride' in src.replace('FlyingOverride left', ''):
        yield (RGON + '/api_npc_entity_misc.lua', 0, 'leave Set/Get/ClearFlyingOverride unregistered until a hook on Entity::IsFlying makes the override take effect')


def rule_player_calls_names_in_lab(fs):
    """Every name api_player_calls.lua registers is named in lab_player_calls.lua, and a std::string argument is built with make_stdstr (wave 8, worker player-calls: the game's costume-animation functions take a std::string reference, so a Lua string or a bare char pointer reads garbage)."""
    src = text_of(RGON + '/api_player_calls.lua')
    lab = text_of(RGON + '/lab_player_calls.lua')
    if src is None or lab is None:
        return
    for cls, name in sorted(set(re.findall(r'method\("(\w+)", "(\w+)"', src))):
        if '"%s.%s"' % (cls, name) not in lab:
            yield (RGON + '/lab_player_calls.lua', 0, 'add "%s.%s" to NAMES and a check that calls it (getter by read-back, setter by effect, '
                   'action by the game\'s reaction), or leave the name out of api_player_calls.lua' % (cls, name))
    for m in re.finditer(r'call\(0x007(?:59f50|ad680),[^\n]*', src):
        if 'make_stdstr' not in src[:m.start()].rsplit('method(', 1)[-1]:
            yield (RGON + '/api_player_calls.lua', 0, 'PlayCollectibleAnim and IsCollectibleAnimFinished take a std::string reference: build it with make_stdstr(name) '
                   'and pass that address, never the Lua string')


def rule_console_box_drawn_in_console_hook(fs):
    """The console's suggestion box is drawn from the hook after Console::Render (wave 7, worker console): a draw from a callback inside Game::Render is covered by the console, and a draw from a hook after Game::Render or Manager::Render returns is never shown (measured: Sprite and Font calls ran without error and nothing reached the frame)."""
    src = text_of(RGON + '/api_hooks.lua')
    if src is None:
        return
    for m in re.finditer(r'on\("(Game|Manager)::Render",[^\n]*\n[^\n]*console_draw', src):
        yield (RGON + '/api_hooks.lua', src[:m.start()].count('\n') + 1,
               'draw the console box from on("Console::Render", ...): a draw after Game::Render or Manager::Render returns is not shown on the screen')


def rule_free_only_own_string_buffers(fs):
    """A std::string buffer is freed with B.free only when the Layer allocated it (2026-11-20, stage-config-modhot: the old path of an unloaded RoomSet already pointed at a freed block, and freeing it logged "double free"). Keep a table of the buffers the writer made (api_stage_config.lua's `mine`) and free only those; leave the game's own buffers alone."""
    for f in fs:
        if not (f.startswith(RGON + '/api_') and f.endswith('.lua')):
            continue
        src = text_of(f) or ''
        if 'set_stdstr' not in src and 'stdstr' not in src:
            continue
        for i, ln in enumerate(src.split('\n'), 1):
            if re.search(r'B\.free\(\s*old', ln) and 'mine[' not in src:
                yield (f, i, 'free a string buffer only if this writer allocated it: keep a `mine` table of its own allocations and check mine[old] before B.free')
def rule_console_game_lines_whitelisted(fs):
    """The game's own console suggestion strings (console + 0x74, six std::strings) keep what the last list left in them: for a command the game has no list for ('restart ', 'kill ') they still hold the command list of the empty line (wave 8, worker console2: Tab on 'restart ' put 'restart spawn' in the line). Read them only for a command in the GAME_ARGS table."""
    src = text_of(RGON + '/api_console.lua')
    if src is None or 'GAME_LINES' not in src:
        return
    if 'GAME_ARGS[' not in src:
        yield (RGON + '/api_console.lua', 0, 'before reading the six suggestion strings of the game\'s console, check the typed command against GAME_ARGS '
               '(the commands the game lists arguments for): for any other command the strings are stale; use the XML tables (extra_entries) for those')


def rule_lua_keyword_table_key(fs):
    """A Lua keyword used as a bare table key (`{ goto = true }`) is a syntax error that stops the whole Layer file from loading, with only 'did not load' in the game log (wave 8, worker console2: api_console.lua lost every feature). Write the key as ["goto"]."""
    for f in fs:
        if not (f.startswith(RGON + '/') and f.endswith('.lua')):
            continue
        t = text_of(f)
        if t is None:
            continue
        for n, line in enumerate(t.splitlines(), 1):
            code = line.split('--')[0]
            m = re.search(r'[{,]\s*(goto|end|and|or|not|if|then|else|elseif|for|while|do|in|local|function|repeat|until|return|break|nil|true|false)\s*=[^=]', code)
            if m:
                yield (f, n, 'the Lua keyword "%s" cannot be a bare table key: write ["%s"] = ...' % (m.group(1), m.group(1)))




def rule_misnamed_collectible_enums(fs):
    """Lab code uses the game's enum names (2026-11-20, player-map: COLLECTIBLE_BLOOD_LUST and COLLECTIBLE_CHARM_OF_THE_VAMPIRE
    are nil, so AddCollectible(nil) silently did nothing and a probe saw no change). The real names are COLLECTIBLE_BLOODY_LUST
    and COLLECTIBLE_CHARM_VAMPIRE; print CollectibleType[name] for a new item name before relying on it."""
    bad = re.compile(r'COLLECTIBLE_(BLOOD_LUST|CHARM_OF_THE_VAMPIRE)\b')
    for f in fs:
        if not (f.startswith('scripts/recomp/rgon/') and f.endswith('.lua')):
            continue
        for i, line in enumerate((text_of(f) or '').splitlines(), 1):
            if bad.search(line):
                yield (f, i, 'misnamed enum: use COLLECTIBLE_BLOODY_LUST / COLLECTIBLE_CHARM_VAMPIRE (the other spellings are nil); '
                       'check a new item name with print(CollectibleType[name]) first')




def rule_player_rest_names_in_lab(fs):
    """Every name api_player_rest.lua registers (method(P, "X") or S.X = function) is called by name in lab_player_rest.lua, and the lab uses
    the real PlayerType enum names (wave 9, worker player-rest: PLAYER_THE_FORGOTTEN is nil, the game's name is PLAYER_THEFORGOTTEN, so
    ChangePlayerType(nil) raised and a probe scenario was lost)."""
    src = text_of(RGON + '/api_player_rest.lua')
    lab = text_of(RGON + '/lab_player_rest.lua')
    if src is None or lab is None:
        return
    names = set(re.findall(r'method\(P, "(\w+)"', src)) | set(re.findall(r'\bS\.(\w+) = function', src))
    for name in sorted(names):
        if name not in lab:
            yield (RGON + '/lab_player_rest.lua', 0, 'add a check that calls "%s" (getter by read-back, setter by effect, action by the game\'s '
                   'reaction), or leave the name out of api_player_rest.lua' % name)
    for f in fs:
        if f.startswith(RGON + '/') and f.endswith('.lua'):
            for i, line in enumerate((text_of(f) or '').splitlines(), 1):
                if 'PLAYER_THE_FORGOTTEN' in line and not line.lstrip().startswith('--') and 'rule' not in f:
                    yield (f, i, 'PlayerType.PLAYER_THE_FORGOTTEN is nil: the game calls it PLAYER_THEFORGOTTEN')


def rule_lab_check_never_fails(fs):
    """A lab check that cannot fail (2026-11-20, room-calls: a check(...) whose condition ended in `or true` passed whatever the game did).
    Write the condition so it fails when the name is wrong, or drop the check and say in the hand-back that the name is read-back only."""
    bad = re.compile(r'\bcheck\(.*\bor true\b')
    for f in fs:
        if not (f.startswith('scripts/recomp/rgon/lab_') and f.endswith('.lua')):
            continue
        for i, line in enumerate((text_of(f) or '').splitlines(), 1):
            if bad.search(line):
                yield (f, i, 'a lab check with `or true` cannot fail: state the condition the game must meet, or remove the check')



def rule_grid_npc_names_in_lab(fs):
    """Every name api_grid_npc.lua registers is named in lab_grid_npc.lua, and a door's sprite function is not registered on a byte-signature match alone (wave 9, worker grid-npc: REPENTOGON's GridEntityDoor::play_animation signature matched a four-argument function in .17, and the real candidate left the door sprite without an animation, so PlayAnimation stayed unregistered)."""
    src = text_of(RGON + '/api_grid_npc.lua')
    lab = text_of(RGON + '/lab_grid_npc.lua')
    if src is None or lab is None:
        return
    names = set(re.findall(r'method\("(\w+)", "(\w+)"', src)) | set(re.findall(r'static\(\w+, "(\w+)", "(\w+)"', src))
    for cls, name in sorted(names):
        if name == 'TryUnlock':
            cls = 'GridEntityLock'
        if cls == 'EntityNPC' and name.startswith(('IsRed', 'SetRed')) or cls == 'EntityNPC' and name.endswith('TeleportationTimer'):
            cls = 'EntityDelirium'
        if '"%s.%s"' % (cls, name) not in lab and not (cls == 'GridEntity' and ('"GridEntity.%s"' % name) in lab):
            yield (RGON + '/lab_grid_npc.lua', 0, 'add "%s.%s" to NAMES and a check that calls it (getter by read-back, action by the game\'s reaction), '
                   'or leave the name out of api_grid_npc.lua' % (cls, name))
    if 'PlayAnimation' in src:
        yield (RGON + '/api_grid_npc.lua', 0, 'GridEntityDoor.PlayAnimation: do not register it until a lab shows the door sprite playing an animation after the call '
               '(0x0070d3f0 only reloads the sprite and the animation names)')




def rule_room_sets_names_in_lab(fs):
    """Every name api_room_sets.lua registers is named in lab_room_sets.lua, and memory the game later frees is made with the game's own operator new (wave 10, worker room-sets: the room array, spawn arrays and entry arrays of a room set are freed by the game's unload with its tracked delete, so a B.alloc block would be a foreign pointer there)."""
    src = text_of(RGON + '/api_room_sets.lua')
    lab = text_of(RGON + '/lab_room_sets.lua')
    if src is None or lab is None:
        return
    for name in ('CcpContainer_RoomConfigSet.Get', 'CcpContainer_RoomConfigSet.AddRooms', 'CcpContainer_RoomConfigSet.LoadStb',
                 'CcpContainer_RoomConfigSet.Size', 'RoomConfigStage.GetRoomSet', 'RoomConfig.AddRooms', 'RoomConfig.LoadStb'):
        if '"%s"' % name not in lab:
            yield (RGON + '/lab_room_sets.lua', 0, 'add "%s" to the Implemented list the lab checks, and a check that calls it' % name)
    for n, line in enumerate(src.splitlines(), 1):
        if 'B.alloc(' in line and not line.lstrip().startswith('--'):
            yield (RGON + '/api_room_sets.lua', n, 'allocate room, spawn and entry arrays with call(NEW, bytes) (the game\'s operator new at 0x00a0f4e0), '
                   'not B.alloc: the game frees them itself when the Game object is destroyed')


def rule_menus_rest_names_in_lab(fs):
    """Every name api_menus_rest.lua registers (put("Class", "Name"...) or def("Name") of GenericPrompt) is named in lab_menus_rest.lua, and the special seeds menu's selection is the word at +0xc (wave 9, worker menus-b: REPENTOGON's +4 holds a float position in this build, which a round trip alone passed; only the Down key's reaction showed it)."""
    src = text_of(RGON + '/api_menus_rest.lua')
    lab = text_of(RGON + '/lab_menus_rest.lua')
    if src is None or lab is None:
        return
    names = set(re.findall(r'put\("(\w+)", "(\w+)"', src))
    names |= set(('GenericPrompt', n) for n in re.findall(r'def\("(\w+)"', src))
    for cls, name in sorted(names):
        if '"%s.%s"' % (cls, name) not in lab:
            yield (RGON + '/lab_menus_rest.lua', 0, 'add "%s.%s" to NAMES and a check that calls it (getter by read-back, setter by effect, '
                   'action by the game\'s reaction), or leave the name out of api_menus_rest.lua' % (cls, name))
    if re.search(r'"SpecialSeedsMenu", 0x251c, 0x1c, 4,', src):
        yield (RGON + '/api_menus_rest.lua', 0, 'the special seeds menu\'s selected element is at base+0xc in this build (+4 is a float position): use 0xc')
def rule_image_render_names_in_lab(fs):
    """Every name api_image_render.lua registers (R.Implemented["Class.Name"] = true) is named in lab_image_render.lua, and api_shapes.lua keeps no Lua stand-in for Shape / DebugRenderer (wave 10, worker image-render: the stand-in kept a timeout of 0 in a Lua table while the game's own fresh shape starts at 5, and Shape:Circle / Capsule drew nothing; the game's DebugRenderer::Get 0x0042eb10 and Shape functions 0x0042ede0 / 0x0042eef0 are called now)."""
    src = text_of(RGON + '/api_image_render.lua')
    lab = text_of(RGON + '/lab_image_render.lua')
    if src is not None and lab is not None:
        names = set(re.findall(r'Implemented\["(\w+\.\w+)"\]\s*=\s*true', src))
        for name in sorted(names):
            if '"%s"' % name not in lab:
                yield (RGON + '/lab_image_render.lua', 0, 'add "%s" to the Implemented list and a check that calls it (getter by read-back, '
                       'setter by effect, action by the game\'s reaction), or leave the name out of api_image_render.lua' % name)
    shapes = text_of(RGON + '/api_shapes.lua')
    if shapes is not None and re.search(r'rawset\(_G, "DebugRenderer"|Sh\.__type = "Shape"', shapes):
        yield (RGON + '/api_shapes.lua', 0, 'Shape and DebugRenderer.Get are the game\'s own objects (api_image_render.lua): do not make a Lua stand-in here')

def rule_player_cooldowns_names_in_lab(fs):
    """Every name api_player_cooldowns.lua registers (method(P, "X") or the Get/Set loop's names) is called in lab_player_cooldowns.lua, and the lab sets the player type the mechanic needs (wave 10, worker player-cooldowns: wave 9 called the Maggy swing, Forgotten swap and item state cooldown words 'no reaction' because the probes ran as Isaac; the game only touches them for Tainted Magdalene 0x16, The Forgotten 0x10 / Soul 0x11, and while an item state runs)."""
    src = text_of(RGON + '/api_player_cooldowns.lua')
    lab = text_of(RGON + '/lab_player_cooldowns.lua')
    if src is None or lab is None:
        return
    names = set(re.findall(r'method\(P, "(\w+)"', src))
    for key in re.findall(r'\b(\w+) = 0x1[0-9a-f]{3}', src):
        names |= {'Get%sCooldown' % key, 'Set%sCooldown' % key}
    for name in sorted(names):
        if name not in lab:
            yield (RGON + '/lab_player_cooldowns.lua', 0, 'add a check that calls "%s" (getter by read-back, setter by the game\'s countdown, '
                   'action by the game\'s reaction), or leave the name out of api_player_cooldowns.lua' % name)
    for needle in ('PLAYER_MAGDALENE_B', 'PLAYER_THEFORGOTTEN'):
        if needle not in lab:
            yield (RGON + '/lab_player_cooldowns.lua', 0, 'the lab must run as %s: the cooldown words only move for that character' % needle)


def rule_stride_divisibility_only(fs):
    """A record vector's stride was taken as 0x50 because the 720-byte vector divided evenly (wave 9 cb-level; the real stride is 0x48 and 720 divides by both, so GetAllRooms returned garbage records and a wrong count). A lab never decides a stride, or a count, from `bytes % stride == 0`: read the first records at each candidate stride (fields must be sane and the index field run 0..n-1) and compare the record count with a count the game gives independently (Level:GetRooms():Size(), a room found at each record's grid cell). Mark a deliberate divisibility test `-- count-checked`."""
    for f in fs:
        if not (f.startswith(RGON + '/') and f.endswith('.lua')):
            continue
        for n, line in enumerate((text_of(f) or '').splitlines(), 1):
            code = line.split('--')[0]
            if re.search(r'(?:\w\s*-\s*\w[^%]*|bytes[^%]*|size[^%]*)%\s*(0x[0-9a-fA-F]{2,}|\d{2,})\s*==\s*0', code) and 'count-checked' not in line:
                yield (f, n, 'a record count must not be decided by `bytes % stride == 0`: read the first records at each candidate stride and compare the count with an independent one (Level:GetRooms():Size(), a room at each record\'s cell), then mark the line -- count-checked')





def rule_menus_more_names_in_lab(fs):
    """Every name api_menus_more.lua registers is named in lab_menus_more.lua, and the lab checks it against the game's own key reaction (wave 10, worker menus-c: DailyChallengeMenu, KeyConfigMenu and ControllerSelectMenu stayed unregistered because no key moved them; a round trip alone proves nothing)."""
    src = text_of(RGON + '/api_menus_more.lua')
    lab = text_of(RGON + '/lab_menus_more.lua')
    if src is None or lab is None:
        return
    for cls, name in sorted(set(re.findall(r'put\("(\w+)", "(\w+)"', src))):
        if '"%s.%s"' % (cls, name) not in lab:
            yield (RGON + '/lab_menus_more.lua', 0, 'add "%s.%s" to NAMES and a check (getter by read-back, setter by the game moving on from the written value under a key), '
                   'or leave the name out of api_menus_more.lua' % (cls, name))




RULES = [rule_home_paths, rule_layer_files_loaded, rule_frame_line_prefix, rule_part_loads_part, rule_lab_prints_done, rule_inert_not_implemented, rule_ctor_implemented_name, rule_host_math_header, rule_image_render_not_registered, rule_layer_setsize_is_scale, rule_hurt_func_source, rule_global_pointer_plus_offset, rule_status_files_in_notes, rule_flip_default, rule_const_table_fallback, rule_lua_vararg_middle, rule_lua_mixed_table_index, rule_room_transition_stand_in, rule_roomconfig_both_tables, rule_orphan_starts_packed, rule_fast_setjmp, rule_gate_class_aware, rule_backlog_narrowing, rule_backlog_class_alias, rule_census_false_positives, rule_new_class_names_flagged, rule_leftovers_methods_in_lab, rule_isaac_misc_names_in_lab, rule_empty_method_stand_in, rule_entity_methods_in_lab, rule_tears_methods_in_lab, rule_read_of_call_result, rule_type_of_callback_result, rule_item_name_is_a_key, rule_conception_pairing, rule_health_field_offsets, rule_cacheflag_literals, rule_lua_format_options, rule_host_string_to_guest, rule_pgrep_game_by_name, rule_web_host_mem_base, rule_insn_hook_giant_entry, rule_stdstring_read, rule_add_cache_flags_one_arg, rule_misc_b_methods_in_lab, rule_data_values_names_in_lab, rule_host_misc_names_in_lab, rule_scan_global_assignment, rule_locate_names_in_lab, rule_familiar_pickup_names_in_lab, rule_npc_entity_misc_names_in_lab, rule_player_calls_names_in_lab, rule_console_box_drawn_in_console_hook, rule_free_only_own_string_buffers, rule_console_game_lines_whitelisted, rule_lua_keyword_table_key, rule_misnamed_collectible_enums, rule_player_rest_names_in_lab, rule_lab_check_never_fails, rule_grid_npc_names_in_lab, rule_room_sets_names_in_lab, rule_menus_rest_names_in_lab, rule_image_render_names_in_lab, rule_player_cooldowns_names_in_lab, rule_stride_divisibility_only, rule_menus_more_names_in_lab]

def main():
    if '--list' in sys.argv:
        for r in RULES:
            print('%s: %s' % (r.__name__, ' '.join(r.__doc__.split())))
        return 0
    fs = files()
    hits = [h for r in RULES for h in r(fs)]
    for f, n, msg in hits:
        print('%s%s: %s' % (f, ':%d' % n if n else '', msg))
    print('lint: %d file(s), %d rule(s), %d problem(s)' % (len(fs), len(RULES), len(hits)))
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
