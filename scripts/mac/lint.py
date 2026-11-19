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


RULES = [rule_room_transition_stand_in, rule_lua_vararg_middle, rule_const_table_fallback, rule_global_pointer_plus_offset, rule_flip_default, rule_inert_not_implemented, rule_home_paths, rule_layer_files_loaded, rule_status_files_in_notes]


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
