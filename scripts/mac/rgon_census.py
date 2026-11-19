#!/usr/bin/env python3
"""rgon_census.py -- REPENTOGON's Lua API against what the Layer has (M8 census).

  python3 scripts/mac/rgon_census.py --repentogon <REPENTOGON checkout> \
      --implemented <log with lab_names / lab_census lines> [--implemented <log> ...] \
      [--mods <mods dir>] [--gate <layer-gate.json>] [--json out.json] [--list missing|used|all]

Reads, never copies: REPENTOGON's documentation (docs/docs/**/*.md, the class pages: a
"### Name" heading under "Functions", "Variables" or "Constructors") for the names and the classes
they belong to, its C++ registrations (the `{ "Name", Handler }` rows and `RegisterFunction` calls)
for names the docs lack and for a rough size of each handler, and
repentogon/resources/scripts/enums_ex.lua for the callback ids. Names are facts about an interface.

The Layer side comes from a game log made with ISAAC_RGON_LAB=lab_census.lua (or lab_names.lua):
  "lab_names: I Class.Name"   REPENTOGON.Implemented: the Layer really does it (counted)
  "lab_names: C <id>"         a callback id the Layer really fires (counted)
  "lab_census: E Class.Name"  the name exists on the live class table (no promise it works)

Status of a REPENTOGON name:
  implemented  in REPENTOGON.Implemented (a class alias such as ItemConfig_Item = ItemConfigItem counts)
  inert        exists in the running Layer, is REPENTOGON-only (tools/gate_names.py's list) and is not
               in Implemented: a stand-in that accepts the call and does nothing ("accepted, not supported")
  game         exists, is not REPENTOGON-only: the game had it already (not counted either way)
  missing      not there

Effort of a missing name (an estimate from the C++ handler, to be confirmed when it is picked up):
  S  pure Lua over things the Layer or the vanilla API already has (no game field, no game call that
     is not itself a vanilla API method)
  M  needs a game structure field measured, or a call to an existing game function by address
  L  needs a new engine hook in the middle of a game function (or a patch REPENTOGON applies to
     game code), so a lift rebuild. Every callback the Layer does not fire is L unless a hook for the
     function exists already (then M).
An override table (EFFORT_OVERRIDE below, or --effort-file name=letter lines) holds what was checked by hand.

Prints a table and, with --json, writes everything (per name: status, effort, mods using it).
No home-folder paths: every path is an argument.
"""
import argparse
import io
import json
import os
import re
import sys
from collections import defaultdict, Counter

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
RGON_DIR = os.path.join(REPO, 'scripts', 'recomp', 'rgon')

# docs file stem -> class name as the Layer says it
STEM_CLASS = {'Repentogon': 'REPENTOGON'}
# the Layer's spellings of a class (compared with underscores removed)
def norm_class(c):
    return c.replace('_', '')

# Efforts settled by hand (a name is "Class.Name"; later batches add rows). Anything not listed uses the heuristic.
EFFORT_OVERRIDE = {}
# whole classes whose effort is decided by what they are, not by a handler
CLASS_EFFORT = {
    'ImGui': ('L', 'needs an immediate-mode GUI drawn by the engine (a shim that only accepts the calls would be S but inert)'),
}


def read(p):
    with io.open(p, encoding='utf-8', errors='replace') as f:
        return f.read()


def walk(d, ext):
    for base, _, names in os.walk(d):
        for n in sorted(names):
            if n.lower().endswith(ext):
                yield os.path.join(base, n)


# ---------------------------------------------------------------- REPENTOGON's side

HEAD = re.compile(r"^###\s+([A-Za-z_][A-Za-z0-9_]*)\b[^{\n]*\{:\s*[^}]*aria-label='([^']+)'", re.M)


def parse_docs(rg, include_modified=False):
    """-> list of dicts {cls, name, kind, doc} (new names only; 'Modified ...' sections are counted apart, or,
    with include_modified, listed with kind 'mod': a vanilla name whose behaviour REPENTOGON changes, such as
    EntityNPC:GetPathfinder, which a mod may use as REPENTOGON's)"""
    root = os.path.join(rg, 'docs', 'docs')
    out, modified = [], 0
    for sub in ('', 'menus', 'renderer'):
        d = os.path.join(root, sub)
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith('.md') or fn in ('index.md', 'install.md', 'install_zh.md', 'docs.md'):
                continue
            text = read(os.path.join(d, fn))
            if 'tags:' not in text[:200] or ('Class' not in text[:200] and 'Global' not in text[:200]):
                continue
            stem = fn[:-3]
            cls = STEM_CLASS.get(stem, stem)
            for m in HEAD.finditer(text):
                name, label = m.group(1), m.group(2).strip()
                if label.startswith('Modified'):
                    modified += 1
                    if include_modified and not name.startswith('__'):
                        out.append({'cls': cls, 'name': name, 'kind': 'mod', 'doc': os.path.join(sub, fn)})
                    continue
                kind = {'Functions': 'fn', 'Variables': 'var', 'Constructors': 'ctor'}.get(label)
                if kind is None or name.startswith('__'):
                    continue
                out.append({'cls': cls, 'name': name, 'kind': kind, 'doc': os.path.join(sub, fn)})
    return out, modified


ROW = re.compile(r'\{\s*"([A-Za-z_][A-Za-z0-9_]*)"\s*,\s*([A-Za-z_][A-Za-z0-9_:]*)\s*\}')
REGF = re.compile(r'Register(?:Global)?Class(?:Function|Variable)\s*\(\s*[^,]+,\s*[^,]+,\s*"([A-Za-z_]\w*)"\s*,\s*([A-Za-z_][\w:]*)'
                  r'|RegisterFunction\s*\(\s*[^,]+,\s*[^,]+,\s*"([A-Za-z_]\w*)"\s*,\s*([A-Za-z_][\w:]*)'
                  r'|RegisterVariable(?:Getter|Setter)?\s*\(\s*[^,]+,\s*[^,]+,\s*"([A-Za-z_]\w*)"\s*,\s*([A-Za-z_][\w:]*)')


def body_of(text, handler):
    m = re.search(r'(?:LUA_FUNCTION\(\s*' + re.escape(handler) + r'\s*\)|\b(?:static\s+)?int\s+' + re.escape(handler)
                  + r'\s*\(\s*lua_State\s*\*\s*\w*\s*\))\s*\{', text)
    if not m:
        return None
    i, depth = m.end(), 1
    while i < len(text) and depth:
        c = text[i]
        depth += (c == '{') - (c == '}')
        i += 1
    return text[m.end():i]


def parse_code(rg):
    """-> {bare name: [ {file, handler, body} ]}"""
    out = defaultdict(list)
    base = os.path.join(rg, 'repentogon')
    for f in walk(base, '.cpp'):
        text = read(f)
        rel = os.path.relpath(f, base)
        pairs = [(m.group(1), m.group(2)) for m in ROW.finditer(text)]
        for m in REGF.finditer(text):
            g = [x for x in m.groups() if x]
            pairs.append((g[0], g[1]))
        for name, handler in pairs:
            out[name].append({'file': rel, 'handler': handler, 'body': body_of(text, handler.split('::')[-1])})
    return out


def parse_callbacks(rg):
    """-> ({name: id}, {id: [names]}) for ModCallbacks 1000+ in enums_ex.lua (uncommented lines)"""
    text = read(os.path.join(rg, 'repentogon', 'resources', 'scripts', 'enums_ex.lua'))
    names = {}
    for m in re.finditer(r'^ModCallbacks\.([A-Za-z0-9_]+)\s*=\s*(\d+)', text, re.M):
        names[m.group(1)] = int(m.group(2))
    ids = defaultdict(list)
    for n, i in names.items():
        ids[i].append(n)
    return names, ids


# ---------------------------------------------------------------- the Layer's side

def parse_layer_logs(paths):
    impl, ids, exists = set(), set(), set()
    for p in paths:
        for line in io.open(p, encoding='utf-8', errors='replace'):
            m = re.search(r'lab_names: I (\S+)', line)
            if m:
                impl.add(m.group(1))
                continue
            m = re.search(r'lab_names: C (\d+)', line)
            if m:
                ids.add(int(m.group(1)))
                continue
            m = re.search(r'lab_census: E (\S+)', line)
            if m:
                exists.add(m.group(1))
    return impl, ids, exists


def gate_only_names():
    p = os.path.join(RGON_DIR, 'gate_names.lua')
    text = read(p)
    m = re.search(r'fns\s*=\s*\[\[(.*?)\]\]', text, re.S)
    return set(m.group(1).split()) if m else set()


# ---------------------------------------------------------------- effort

LUAISH = re.compile(r'->\s*([A-Za-z_]\w*)\s*(\()?')
FREECALL = re.compile(r'(?<![>.\w])([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*\(')
PLUMBING_NS = {'lua', 'luabridge', 'std', 'metatables', 'Metatables', 'GlobalClasses'}
PLUMBING_PREFIX = ('lua_', 'luaL_', 'LUA_', 'static_cast', 'reinterpret_cast', 'const_cast')
PLUMBING = {'GetLuabridgeUserdata', 'GetRawUserdata', 'luaL_optboolean', 'luaL_checkboolean', 'max', 'min', 'abs', 'sqrt', 'pow',
            'sin', 'cos', 'atan2', 'floor', 'ceil', 'fabs', 'round', 'strlen', 'strcmp', 'sizeof', 'Vector', 'Color', 'KColor',
            'wide_to_mb', 'mb_to_wide', 'size', 'push_back', 'begin', 'end', 'empty', 'clear', 'c_str', 'find', 'resize',
            'back', 'front', 'count', 'emplace_back', 'insert', 'erase', 'rand'}
KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'else', 'catch', 'sizeof', 'case', 'delete', 'new'}
HOOKISH =re.compile(r'\b(Patch|ASMPatch|HOOK_METHOD|HOOK_STATIC|_Hook\b|ShouldOverride|SigScan|VirtualProtect|MemoryWrite)')


def estimate_effort(cands, vanilla):
    """worst effort over the C++ handlers found for a name"""
    if not cands:
        return 'M', 'no handler found'
    rank = {'S': 0, 'M': 1, 'L': 2}
    worst, why = 'S', 'pure Lua'
    for c in cands:
        b = c['body']
        if b is None:
            e, w = 'M', 'handler not found'
        elif HOOKISH.search(b):
            e, w = 'L', 'patches game code'
        else:
            calls, fields = set(), set()
            for m in LUAISH.finditer(b):
                (calls if m.group(2) else fields).add(m.group(1))
            calls.discard('SetField')
            # free and static calls (Entity_NPC::ThrowMaggot(...), OpenClipboard(...), GetCustomCacheValue(...))
            for m in FREECALL.finditer(b):
                n = m.group(1).split('::')[-1]
                q = m.group(1).split('::')[0] if '::' in m.group(1) else ''
                if q in PLUMBING_NS or n in PLUMBING or n.startswith(PLUMBING_PREFIX) or n in KEYWORDS:
                    continue
                calls.add(n)
            fields ={x for x in fields if x.lower().lstrip('_') not in vanilla}
            if fields:
                e, w = 'M', 'game field: ' + ','.join(sorted(fields)[:3])
            else:
                foreign = sorted(x for x in calls if x.lower().lstrip('_') not in vanilla)
                if foreign:
                    e, w = 'M', 'game call: ' + ','.join(foreign[:3])
                else:
                    e, w = 'S', 'vanilla calls only' if calls else 'pure Lua'
        if rank[e] > rank[worst] or (e == worst and worst == 'S' and w != 'pure Lua'):
            worst, why = e, w
    return worst, why


# ---------------------------------------------------------------- mods

ID = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')


def mods_usage(mods_dir):
    """-> (names_by_mod {mod: set of identifiers}, repentogon_mods set)"""
    out, rg = {}, set()
    for mod in sorted(os.listdir(mods_dir)):
        d = os.path.join(mods_dir, mod)
        if not os.path.isdir(d):
            continue
        ids = set()
        for f in walk(d, '.lua'):
            try:
                text = read(f)
            except OSError:
                continue
            text = re.sub(r'--\[(=*)\[.*?\]\1\]', ' ', text, flags=re.S)
            text = re.sub(r'--[^\n]*', ' ', text)
            ids.update(ID.findall(text))
        out[mod] = ids
        if 'REPENTOGON' in ids:
            rg.add(mod)
    return out, rg


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--repentogon', required=True, help='REPENTOGON checkout (docs/, repentogon/)')
    ap.add_argument('--implemented', action='append', default=[], help='game log(s) with lab_names / lab_census lines')
    ap.add_argument('--mods', help='mods folder (Workshop copy or symlink) to grep for names')
    ap.add_argument('--gate', help='layer-gate.json of a run (decisions of the REPENTOGON-using mods)')
    ap.add_argument('--json', help='write everything here')
    ap.add_argument('--list', choices=['missing', 'used', 'all', 'none'], default='none')
    ap.add_argument('--effort-file', help='lines "Class.Name S|M|L" settled by hand')
    a = ap.parse_args()

    docs, modified = parse_docs(a.repentogon)
    seen_keys, uniq = set(), []
    for d in docs:
        k = (d['cls'], d['name'])
        if k not in seen_keys:
            seen_keys.add(k)
            uniq.append(d)
    docs = uniq
    code = parse_code(a.repentogon)
    cb_names, cb_ids = parse_callbacks(a.repentogon)
    impl, impl_ids, exists = parse_layer_logs(a.implemented)
    gate_only = gate_only_names()
    if a.effort_file:
        for line in read(a.effort_file).splitlines():
            parts = line.split()
            if len(parts) == 2 and parts[1] in 'SML':
                EFFORT_OVERRIDE[parts[0]] = parts[1]

    impl_n = {norm_class(k.split('.')[0]) + '.' + k.split('.', 1)[1] for k in impl if '.' in k}
    exists_n = {norm_class(k.split('.')[0]) + '.' + k.split('.', 1)[1] for k in exists if '.' in k}
    vanilla = {k.split('.', 1)[1].lower().lstrip('_') for k in exists if '.' in k and k.split('.', 1)[1] not in gate_only}

    mods, rg_mods = ({}, set())
    if a.mods:
        mods, rg_mods = mods_usage(a.mods)
    users = defaultdict(set)
    if mods:
        for mod, ids in mods.items():
            for d in docs:
                if d['name'] in ids and d['name'] in gate_only:
                    users[d['name']].add(mod)

    rows = []
    for d in docs:
        key = d['cls'] + '.' + d['name']
        nk = norm_class(d['cls']) + '.' + d['name']
        if nk in impl_n:
            st = 'implemented'
        elif nk in exists_n:
            st = 'inert' if d['name'] in gate_only else 'game'
        else:
            st = 'missing'
        eff, why = (None, None)
        if st in ('missing', 'inert'):
            eff, why = estimate_effort(code.get(d['name'], []), vanilla)
            if d['cls'] in CLASS_EFFORT:
                eff, why = CLASS_EFFORT[d['cls']]
            if key in EFFORT_OVERRIDE:
                eff, why = EFFORT_OVERRIDE[key], 'checked by hand'
        u = sorted(users.get(d['name'], ()))
        rows.append({'name': key, 'kind': d['kind'], 'status': st, 'effort': eff, 'why': why,
                     'mods_any': u, 'mods_rgon': [m for m in u if m in rg_mods]})

    # names in the C++ registrations that no doc page lists (bare names, not classes)
    doc_bare = {d['name'] for d in docs}
    undocumented = sorted(n for n in code if n not in doc_bare and len(n) > 2 and n in gate_only)

    # callbacks: unique ids of 1000 and up
    cb_rows = []
    for i in sorted(cb_ids):
        if i < 1000:
            continue
        names = sorted(cb_ids[i])
        st = 'implemented' if i in impl_ids else 'missing'
        cu = set()
        for mod, ids in mods.items():
            if any(n in ids for n in names):
                cu.add(mod)
        cb_rows.append({'id': i, 'names': names, 'status': st, 'effort': 'L' if st == 'missing' else None,
                        'mods_any': sorted(cu), 'mods_rgon': sorted(m for m in cu if m in rg_mods)})

    gate = None
    if a.gate and os.path.exists(a.gate):
        gate = json.load(open(a.gate)).get('mods')

    def tally(rs):
        c = Counter(r['status'] for r in rs)
        e = Counter(r['effort'] for r in rs if r['status'] == 'missing')
        return {'total': len(rs), 'implemented': c['implemented'], 'inert': c['inert'], 'game': c['game'],
                'missing': c['missing'], 'missing_S': e['S'], 'missing_M': e['M'], 'missing_L': e['L'],
                'missing_used_by_rgon_mods': sum(1 for r in rs if r['status'] in ('missing', 'inert') and r['mods_rgon']),
                'missing_used_by_any_mod': sum(1 for r in rs if r['status'] in ('missing', 'inert') and r['mods_any'])}

    fn_rows = [r for r in rows if r['kind'] == 'fn']
    var_rows = [r for r in rows if r['kind'] == 'var']
    ctor_rows = [r for r in rows if r['kind'] == 'ctor']
    summary = {'functions': tally(fn_rows), 'variables': tally(var_rows), 'constructors': tally(ctor_rows),
               'callbacks': tally(cb_rows), 'modified_vanilla_entries_not_counted': modified,
               'cpp_registered_not_in_docs': len(undocumented),
               'classes_in_docs': len({r['name'].split('.')[0] for r in rows}),
               'classes_with_no_name_found_in_layer': sorted({c for c in {r['name'].split('.')[0] for r in rows}
                                                              if not any(k.startswith(norm_class(c) + '.') for k in impl_n | exists_n)}),
               'mods_naming_repentogon': len(rg_mods)}
    all_rows = rows

    w = sys.stdout.write
    w('%-14s %6s %6s %6s %6s %6s | %4s %4s %4s | %s\n' % ('', 'total', 'impl', 'inert', 'game', 'miss', 'S', 'M', 'L', 'missing, used by REPENTOGON mods / any mod'))
    for label, key in (('functions', 'functions'), ('variables', 'variables'), ('constructors', 'constructors'), ('callbacks', 'callbacks')):
        t = summary[key]
        w('%-14s %6d %6d %6d %6d %6d | %4d %4d %4d | %d / %d\n' % (label, t['total'], t['implemented'], t['inert'], t['game'], t['missing'],
                                                              t['missing_S'], t['missing_M'], t['missing_L'], t['missing_used_by_rgon_mods'], t['missing_used_by_any_mod']))
    w('(modified vanilla entries not counted: %d; C++ registered names with no doc entry and REPENTOGON-only: %d; mods naming REPENTOGON: %d)\n'
      % (modified, len(undocumented), len(rg_mods)))

    if a.list != 'none':
        w('\n')
        for r in sorted(all_rows, key=lambda r: (r['name'])):
            if r['status'] not in ('missing', 'inert') and a.list != 'all':
                continue
            if a.list == 'used' and not r['mods_rgon']:
                continue
            w('%-8s %s %-3s %s\n' % (r['status'], r['effort'] or '-', r['kind'], r['name']) + ('   mods: ' + ', '.join(r['mods_rgon'] or r['mods_any'][:4]) + '\n' if r['mods_any'] else ''))
        for r in cb_rows:
            if r['status'] == 'missing' or a.list == 'all':
                if a.list == 'used' and not r['mods_rgon']:
                    continue
                w('%-8s %s cb  %d %s\n' % (r['status'], r['effort'] or '-', r['id'], '/'.join(r['names'])))

    if a.json:
        with io.open(a.json, 'w', encoding='utf-8') as f:
            json.dump({'summary': summary, 'names': all_rows, 'callbacks': cb_rows, 'undocumented': undocumented,
                       'gate': gate}, f, indent=1, sort_keys=True)
            f.write('\n')


if __name__ == '__main__':
    main()
