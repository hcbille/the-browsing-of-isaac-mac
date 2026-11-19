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
        ("field of the mod's own table", 'a dot access `x.Name` that is not called or constructed, where every REPENTOGON candidate is a function, is a field of the mod\'s own table: suppress it'),
    ):
        if needle not in t:
            yield (f, 0, msg + ' (missing `%s`)' % needle)


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


RULES = [rule_item_name_is_a_key, rule_insn_hook_giant_entry, rule_pgrep_game_by_name, rule_tears_methods_in_lab, rule_cacheflag_literals, rule_health_field_offsets, rule_web_host_mem_base, rule_read_of_call_result, rule_lua_format_options, rule_conception_pairing, rule_backlog_narrowing, rule_entity_methods_in_lab, rule_gate_class_aware, rule_fast_setjmp, rule_roomconfig_both_tables, rule_room_transition_stand_in, rule_lua_vararg_middle, rule_const_table_fallback, rule_global_pointer_plus_offset, rule_flip_default, rule_inert_not_implemented, rule_home_paths, rule_layer_files_loaded, rule_status_files_in_notes, rule_frame_line_prefix, rule_part_loads_part]


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
