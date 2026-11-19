#!/usr/bin/env python3
"""rgon_backlog.py -- every REPENTOGON name that given mods use and the Layer lacks, class by class.

  python3 scripts/mac/rgon_backlog.py --repentogon <REPENTOGON checkout> --implemented <census.log> \
      --mod FF=<mod dir> --mod Epiphany=<mod dir> --out-md notes/backlog.md --out-json notes/backlog.json

rgon_census.py and the gate match a mod's use by the bare method name, so a name that two classes share
(EntityPlayer.GetWeapon and EntityFamiliar.GetWeapon) is counted as present when only one of them is. This
tool keeps the class:

  * candidate classes of a name = every class that registers it, from REPENTOGON's documentation (class
    pages, including "Modified" sections) and from its C++ registrations (luaL_Reg arrays handed to
    RegisterFunctions / RegisterNewClass, and the one-name Register* calls). Names are facts about an
    interface; no C++ is copied.
  * a use is `x:Name(`, `x.Name`, `Class.Name`, a constructor call, an enum entry REPENTOGON adds, or a
    ModCallbacks id REPENTOGON adds. Lua comments and strings are stripped first; `function a:Name(`
    definitions are not uses.
  * the candidates narrow only where the code shows the class: the receiver is a call chain
    (Game():GetRoom():Name, Isaac.GetPlayer(0):Name, x:ToNPC():Name), a variable every assignment in the
    file gives the same class, a parameter of a function registered for a callback whose entity is known
    (MC_FAMILIAR_UPDATE -> EntityFamiliar), or a variable called player/npc/familiar/... whose other
    assignments do not contradict the name. Otherwise every candidate stays (a superset beats a miss).
  * a use is MISSING when the Layer lacks the name on ANY candidate class.

The Layer side is a census log (ISAAC_RGON_LAB=lab_census.lua): "lab_census: E Class.Name" (exists on
the live class table), "lab_names: I Class.Name" (REPENTOGON.Implemented), "lab_names: C <id>" (callback
fires). Status of a (class, name) pair:
  implemented   in Implemented (the class or its Entity/GridEntity parent)
  inert         exists, REPENTOGON-only by name (gate_names.lua), not Implemented: a stand-in
  unverified    exists, not Implemented, and the bare name is not REPENTOGON-only (the game may have it
                on this class, or a stand-in hides behind a name the game has elsewhere)
  missing       absent
Names read: --repentogon docs/docs/**/*.md and repentogon/**/*.cpp, repentogon/resources/scripts/enums_ex.lua.
No home-folder paths: every path is an argument.
"""
import argparse
import collections
import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rgon_census as rc  # noqa: E402  (reuse its REPENTOGON-reading code)

norm = rc.norm_class

ENTITY_SUB = ['EntityPlayer', 'EntityNPC', 'EntityFamiliar', 'EntityTear', 'EntityLaser', 'EntityBomb', 'EntityPickup',
              'EntityProjectile', 'EntityEffect', 'EntityKnife', 'EntitySlot']
PARENT = {norm(c): 'Entity' for c in ENTITY_SUB}
for _g in ('Rock', 'TNT', 'Poop', 'Spikes', 'Door', 'Pit', 'Fire', 'Teleporter', 'Gravity', 'Pressureplate', 'Statue',
           'Trapdoor', 'Stairs', 'Crawlspace', 'Decoration', 'Lock', 'Web', 'Wall'):
    PARENT[norm('GridEntity' + _g)] = 'GridEntity'

# ---------------------------------------------------------------------------- C++ registrations, class resolved


def strip_cpp(t):
    t = re.sub(r'/\*.*?\*/', ' ', t, flags=re.S)
    return re.sub(r'//[^\n]*', ' ', t)


def split_args(s, i):
    """s[i] is just after '(' -> (list of top-level args, index after ')')"""
    args, depth, cur = [], 1, []
    while i < len(s) and depth:
        c = s[i]
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                break
        if c == ',' and depth == 1:
            args.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    args.append(''.join(cur).strip())
    return args, i + 1


def cpp_pairs(rg):
    """-> list of {cls, name, file} from the Register* calls whose class can be resolved"""
    base = os.path.join(rg, 'repentogon')
    core = rc.read(os.path.join(rg, 'libzhl', 'LuaCore.cpp'))
    enum2cls = {}
    for m in re.finditer(r'_metatable_idx_from_name\["([^"]+)"\]\s*=\s*Metatables::(\w+)', core):
        if not m.group(1).startswith('const '):
            enum2cls[m.group(2)] = m.group(1)
    mt2cls = {m.group(1): m.group(2) for m in re.finditer(r'const char\*\s+(\w+MT)\s*=\s*"([^"]+)"', core)}

    def resolve(tok):
        tok = tok.strip()
        if tok.startswith('"') and tok.endswith('"'):
            return tok[1:-1]
        t = tok.split('::')[-1]
        return enum2cls.get(t) or mt2cls.get(t)

    out = []
    for f in rc.walk(base, '.cpp'):
        text = strip_cpp(rc.read(f))
        rel = os.path.relpath(f, base)
        arrays = []   # (pos, array name, [row names])
        for m in re.finditer(r'luaL_Reg\s+(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\n?\s*\}\s*;', text, re.S):
            arrays.append((m.start(), m.group(1), [r.group(1) for r in rc.ROW.finditer(m.group(2))]))
        for m in re.finditer(r'\bRegister\w*\s*\(', text):
            args, _ = split_args(text, m.end())
            if len(args) < 3:
                continue
            cls = resolve(args[1])
            if not cls:
                continue
            for a in args[2:]:
                a = a.strip()
                if a.startswith('"') and a.endswith('"') and re.fullmatch(r'"[A-Za-z_]\w*"', a):
                    out.append({'cls': cls, 'name': a[1:-1], 'file': rel})
                    break
                cand = [x for x in arrays if x[1] == a and x[0] < m.start()]
                if cand:
                    for n in cand[-1][2]:
                        out.append({'cls': cls, 'name': n, 'file': rel})
                    break
    return out


# ---------------------------------------------------------------------------- Lua source, stripped


def strip_lua(t):
    """Lua source with comments and string contents blanked, newlines and offsets kept."""
    out, i, n = [], 0, len(t)
    while i < n:
        c = t[i]
        if t.startswith('--', i):
            m = re.match(r'--\[(=*)\[', t[i:i + 40])
            if m:
                close = ']' + m.group(1) + ']'
                j = t.find(close, i)
                j = n if j < 0 else j + len(close)
            else:
                j = t.find('\n', i)
                j = n if j < 0 else j
            out.append(re.sub(r'[^\n]', ' ', t[i:j]))
            i = j
        elif c in '"\'':
            j = i + 1
            while j < n and t[j] != c and t[j] != '\n':
                j += 2 if t[j] == '\\' else 1
            out.append(c + ' ' * max(0, j - i - 1) + (c if j < n and t[j] == c else ''))
            i = j + 1
        elif c == '[' and re.match(r'\[=*\[', t[i:i + 40]):
            m = re.match(r'\[(=*)\[', t[i:i + 40])
            close = ']' + m.group(1) + ']'
            j = t.find(close, i)
            j = n if j < 0 else j + len(close)
            out.append('""' + re.sub(r'[^\n]', ' ', t[i + 2:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return ''.join(out)


RET = {'GetRoom': 'Room', 'GetLevel': 'Level', 'GetPlayer': 'EntityPlayer', 'ToPlayer': 'EntityPlayer', 'ToNPC': 'EntityNPC',
       'ToFamiliar': 'EntityFamiliar', 'ToTear': 'EntityTear', 'ToLaser': 'EntityLaser', 'ToBomb': 'EntityBomb',
       'ToPickup': 'EntityPickup', 'ToProjectile': 'EntityProjectile', 'ToEffect': 'EntityEffect', 'ToKnife': 'EntityKnife',
       'ToSlot': 'EntitySlot', 'GetSprite': 'Sprite', 'GetItemPool': 'ItemPool', 'GetHUD': 'HUD', 'GetSeeds': 'Seeds',
       'GetEffects': 'TemporaryEffects', 'GetItemConfig': 'ItemConfig', 'GetMusicManager': 'MusicManager',
       'GetSFXManager': 'SFXManager', 'GetCurrentRoomDesc': 'RoomDescriptor', 'GetRoomByIdx': 'RoomDescriptor',
       'GetEntityConfig': None, 'GetPlayerByIndex': 'EntityPlayer', 'GetTemporaryEffects': 'TemporaryEffects',
       'GetCollectible': 'ItemConfigItem', 'GetTrinket': 'ItemConfigItem', 'GetCard': 'ItemConfigCard',
       'GetPillEffect': 'ItemConfigPillEffect', 'GetPlayerHUD': 'PlayerHUD', 'GetGridEntity': 'GridEntity',
       'GetGridEntityFromPos': 'GridEntity', 'GetRoomShape': None}
CTOR = {'Game': 'Game', 'Sprite': 'Sprite', 'RNG': 'RNG', 'Vector': 'Vector', 'Color': 'Color', 'BitSet128': 'BitSet128',
        'EntityPtr': 'EntityPtr', 'KColor': 'KColor'}
PROP = {'Player': 'EntityPlayer'}
# variable names that mean one class (used only when no assignment in the file contradicts them)
VARNAME = {'player': 'EntityPlayer', 'npc': 'EntityNPC', 'familiar': 'EntityFamiliar', 'fam': 'EntityFamiliar',
           'tear': 'EntityTear', 'laser': 'EntityLaser', 'room': 'Room', 'level': 'Level', 'game': 'Game',
           'sprite': 'Sprite', 'pickup': 'EntityPickup', 'bomb': 'EntityBomb', 'knife': 'EntityKnife',
           'projectile': 'EntityProjectile', 'proj': 'EntityProjectile', 'effect': 'EntityEffect', 'itempool': 'ItemPool',
           'hud': 'HUD', 'seeds': 'Seeds', 'rng': 'RNG', 'p': None}
MODISH = {'mod', 'Mod', 'self', 'FiendFolio', 'Epiphany', 'FF', 'epiphany', '_G', 'table', 'string', 'math', 'os', 'io', 'debug',
          'json', 'utf8', 'coroutine', 'package',
          'TearFlagsLib'}   # a mod's own library table (Fiend Folio's TearFlagsLib.FireSplitTear is its own function)
# a mod's cached accessor (Mod.Room(), Mod.Game(), Mod.Level()) returns the game's object of that name
MODACCESS = {'Room': 'Room', 'Game': 'Game', 'Level': 'Level'}


def cb_arg_class(mc, idx):
    """class of event argument idx (0-based, after the mod) of a callback, or None"""
    n = mc
    if idx == 0:
        for key, cls in (('FAMILIAR', 'EntityFamiliar'), ('_NPC_', 'EntityNPC'), ('_TEAR_', 'EntityTear'),
                         ('_LASER_', 'EntityLaser'), ('_BOMB_', 'EntityBomb'), ('_PICKUP_', 'EntityPickup'),
                         ('_PROJECTILE_', 'EntityProjectile'), ('_EFFECT_', 'EntityEffect'), ('_KNIFE_', 'EntityKnife'),
                         ('_SLOT_', 'EntitySlot'), ('_PLAYER_', 'EntityPlayer'), ('PEFFECT', 'EntityPlayer'),
                         ('EVALUATE_CACHE', 'EntityPlayer')):
            if key in n:
                return cls
        return None
    if idx == 2 and n in ('MC_USE_ITEM', 'MC_PRE_USE_ITEM'):
        return 'EntityPlayer'
    if idx == 1 and n in ('MC_USE_CARD', 'MC_USE_PILL', 'MC_PRE_USE_CARD', 'MC_PRE_USE_PILL'):
        return 'EntityPlayer'
    return None


# a Lua value's class from an expression chain: pieces = [('id', name) | ('call',) | ('dot', name) | ('colon', name)]

def eval_chain(pieces, binding_of, classes_global):
    """-> class name, 'TABLE', or None"""
    if not pieces or pieces[0][0] != 'id':
        return None
    first = pieces[0][1]
    i = 1
    if first in CTOR and len(pieces) > 1 and pieces[1][0] == 'call':
        cur, i = CTOR[first], 2
    elif first in classes_global:
        cur = ('static', first)
    elif first in MODISH and len(pieces) > 2 and pieces[1][0] == 'dot' and pieces[1][1] in MODACCESS and pieces[2][0] == 'call':
        cur, i = MODACCESS[pieces[1][1]], 3
    else:
        cur = binding_of(first)
        if cur is None:
            return None
    while i < len(pieces):
        p = pieces[i]
        if p[0] in ('dot', 'colon'):
            name = p[1]
            has_call = i + 1 < len(pieces) and pieces[i + 1][0] == 'call'
            if has_call:
                cur = RET.get(name)
                i += 2
            else:
                cur = PROP.get(name) if p[0] == 'dot' else None
                i += 1
            if cur is None:
                return None
        elif p[0] == 'call' and i == 1 and first not in CTOR:
            # a plain global call: Game() handled above; others unknown
            return None
        else:
            return None
    return cur if isinstance(cur, str) else None


def chain_before(text, pos):
    """pieces of the receiver expression that ends just before text[pos] (a '.' or ':'); [] if none"""
    pieces = []
    i = pos - 1
    while True:
        while i >= 0 and text[i] in ' \t':
            i -= 1
        if i < 0:
            break
        c = text[i]
        if c == ')' or c == ']':
            close, op = c, '(' if c == ')' else '['
            depth, j = 0, i
            while j >= 0:
                if text[j] == close:
                    depth += 1
                elif text[j] == op:
                    depth -= 1
                    if depth == 0:
                        break
                j -= 1
            if j < 0:
                return []
            pieces.append(('call',) if c == ')' else ('index',))
            i = j - 1
            continue
        if re.match(r'[A-Za-z0-9_]', c):
            j = i
            while j >= 0 and re.match(r'[A-Za-z0-9_]', text[j]):
                j -= 1
            word = text[j + 1:i + 1]
            k = j
            while k >= 0 and text[k] in ' \t':
                k -= 1
            sep = text[k] if k >= 0 else ''
            if sep in '.:' and not (sep == '.' and k > 0 and text[k - 1] == '.') and not (sep == ':' and k > 0 and text[k - 1] == ':'):
                pieces.append(('dot' if sep == '.' else 'colon', word))
                i = k - 1
                continue
            # keywords before an identifier (return x:Foo) end the chain
            pieces.append(('id', word))
            break
        break
    pieces.reverse()
    return pieces


# ---------------------------------------------------------------------------- per-file analysis

ASSIGN = re.compile(r'^[ \t]*(local[ \t]+)?([A-Za-z_]\w*(?:[ \t]*,[ \t]*[A-Za-z_]\w*)*)[ \t]*=(?!=)[ \t]*(.*)$', re.M)
FUNC = re.compile(r'\bfunction\b[ \t]*([\w.:]*)[ \t]*\(([^)]*)\)')
CBREG = re.compile(r'\bAdd(?:Priority)?Callback\s*\(')


def analyse_file(text, classes_global):
    """-> (binding_of(name) -> class | 'TABLE' | None, bindings {name: (set of classes, has_unknown)})"""
    cbs = collections.defaultdict(set)    # handler tail name -> {MC names}
    anon = {}                              # function-start offset -> MC name
    for m in CBREG.finditer(text):
        args, end = split_args(text, m.end())
        mc = next((re.search(r'\bMC_[A-Z0-9_]+', a).group(0) for a in args if re.search(r'\bMC_[A-Z0-9_]+', a)), None)
        if mc is None:
            continue
        for a in args:
            if a.startswith('function'):
                anon[m.end() + text[m.end():end].find(a)] = mc
            elif re.fullmatch(r'[\w.]+', a) and not a.startswith('MC_') and 'MC_' not in a and not a.isdigit() \
                    and a not in ('ModCallbacks', 'nil', 'true', 'false'):
                cbs[a.split('.')[-1]].add(mc)

    binds = collections.defaultdict(lambda: [set(), False])

    def add(name, cls):
        if cls is None:
            binds[name][1] = True
        else:
            binds[name][0].add(cls)

    for m in FUNC.finditer(text):
        fname, params = m.group(1), [p.strip() for p in m.group(2).split(',') if p.strip()]
        colon = ':' in fname
        mcs = set()
        if m.start() in anon:
            mcs.add(anon[m.start()])
        elif fname:
            mcs |= cbs.get(re.split(r'[.:]', fname)[-1], set())
        for k, p in enumerate(params):
            if p == '...':
                continue
            idx = k - (0 if colon else 1)
            cls = None
            if mcs and idx >= 0:
                got = {cb_arg_class(mc, idx) for mc in mcs}
                cls = got.pop() if len(got) == 1 else None
            add(p, cls)
        if colon:
            add('self', 'TABLE')

    # handlers registered as `mod.Name = function(...)`/ local function handled by FUNC for named ones; others unknown
    for m in re.finditer(r'\bfor\b([^\n]*?)\bin\b', text):
        for v in re.findall(r'[A-Za-z_]\w*', m.group(1)):
            add(v, None)

    def infer_rhs(rhs):
        rhs = rhs.strip()
        if rhs.startswith('{') or re.match(r'RegisterMod\s*\(', rhs) or re.match(r'require\s*\(', rhs) or rhs.startswith('function'):
            return 'TABLE'
        # leading chain: identifier (.|:) name ( ... ) ...
        pieces, i = [], 0
        m = re.match(r'[A-Za-z_]\w*', rhs)
        if not m:
            return None
        pieces.append(('id', m.group(0)))
        i = m.end()
        while i < len(rhs):
            if rhs[i] == '(':
                _, j = split_args(rhs, i + 1)
                pieces.append(('call',))
                i = j
            elif rhs[i] in '.:' and not rhs.startswith('..', i):
                m = re.match(r'[.:][ \t]*([A-Za-z_]\w*)', rhs[i:])
                if not m:
                    break
                pieces.append(('dot' if rhs[i] == '.' else 'colon', m.group(1)))
                i += m.end()
            else:
                break
        rest = rhs[i:].lstrip()
        if rest and not rest.startswith((',', 'or ', 'and ', ')')) and rest != '':
            # trailing operators make the type unclear
            if re.match(r'(\+|-|\*|/|%|\.\.|==|~=|<|>|\[)', rest):
                return None
        return eval_chain(pieces, lambda n: current(n), classes_global)

    def current(name):
        s, unk = binds[name] if name in binds else (set(), True)
        if len(s) == 1 and not unk and 'TABLE' not in s:
            return next(iter(s))
        return None

    for m in ASSIGN.finditer(text):
        names = [x.strip() for x in m.group(2).split(',')]
        rhs_parts = split_top(m.group(3))
        for k, nm in enumerate(names):
            if len(names) == 1 or (len(rhs_parts) == len(names)):
                add(nm, infer_rhs(rhs_parts[k] if rhs_parts else ''))
            else:
                add(nm, None)
    return binds


def split_top(s):
    parts, depth, cur = [], 0, []
    for c in s:
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
        if c == ',' and depth == 0:
            parts.append(''.join(cur))
            cur = []
        else:
            cur.append(c)
    parts.append(''.join(cur))
    return parts


def lua_files(d):
    for base, _, names in os.walk(d):
        for n in sorted(names):
            if n.lower().endswith('.lua'):
                yield os.path.join(base, n)


# ---------------------------------------------------------------------------- main


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--repentogon', required=True)
    ap.add_argument('--implemented', action='append', required=True, help='census log(s)')
    ap.add_argument('--mod', action='append', required=True, metavar='LABEL=DIR')
    ap.add_argument('--out-md')
    ap.add_argument('--out-json')
    ap.add_argument('--runtime-log', help='game log with "missing method Class.Name" lines (lab_missing.lua) to compare')
    ap.add_argument('--command', help='the command line to print at the top of the .md')
    a = ap.parse_args()

    mods = []
    for spec in a.mod:
        label, _, d = spec.partition('=')
        mods.append((label, d))

    # ---- REPENTOGON's names, class-qualified
    docs, _mod = rc.parse_docs(a.repentogon, include_modified=True), None
    docs = docs[0]
    display = {}
    pairs = {}   # (normclass, name) -> {cls, name, kind, src:set}
    for d in docs:
        k = (norm(d['cls']), d['name'])
        display.setdefault(norm(d['cls']), d['cls'])
        e = pairs.setdefault(k, {'cls': d['cls'], 'name': d['name'], 'kind': d['kind'], 'src': set()})
        e['src'].add('docs')
        if e['kind'] == 'mod' and d['kind'] != 'mod':
            e['kind'] = d['kind']
    for c in cpp_pairs(a.repentogon):
        k = (norm(c['cls']), c['name'])
        display.setdefault(norm(c['cls']), c['cls'])
        e = pairs.setdefault(k, {'cls': display[norm(c['cls'])], 'name': c['name'], 'kind': 'fn', 'src': set()})
        e['src'].add('cpp')
    by_name = collections.defaultdict(list)
    for (nc, n), e in pairs.items():
        if e['kind'] == 'mod' and norm(n) == nc:
            continue   # a "Modified Constructors" entry (RNG(), Font()): the class name, not a method
        by_name[n].append(e)

    impl, impl_ids, exists = rc.parse_layer_logs(a.implemented)
    impl_n = {norm(k.split('.')[0]) + '.' + k.split('.', 1)[1] for k in impl if '.' in k}
    exists_n = {norm(k.split('.')[0]) + '.' + k.split('.', 1)[1] for k in exists if '.' in k}
    gate_only = rc.gate_only_names()
    inert_comments = set()
    for f in rc.walk(rc.RGON_DIR, '.lua'):
        for m in re.finditer(r'^\s*--\s*inert:\s*(.+)$', rc.read(f), re.M):
            inert_comments.update(m.group(1).replace(',', ' ').split())

    def pair_status(nc, name):
        chain, c = [nc], nc
        while c in PARENT:
            c = norm(PARENT[c])
            chain.append(c)
        for c in chain:
            if c + '.' + name in impl_n:
                return 'implemented'
        for c in chain:
            if c + '.' + name in exists_n:
                if name in gate_only or name in inert_comments:
                    return 'inert'
                return 'unverified'
        return 'missing'

    # classes where the Layer's live table has a name REPENTOGON does not register there: the game's own
    # method of that name. A use whose receiver stays unknown may be that one (entity:GetSprite()).
    vanilla_classes = collections.defaultdict(set)
    for k in exists:
        if '.' in k:
            c, n = k.split('.', 1)
            if n in by_name and (norm(c), n) not in pairs:
                vanilla_classes[n].add(c)

    cb_names, cb_ids = rc.parse_callbacks(a.repentogon)
    cb_new = {n: i for n, i in cb_names.items() if i >= 1000}
    enum_add = collections.defaultdict(set)
    ex = rc.read(os.path.join(a.repentogon, 'repentogon', 'resources', 'scripts', 'enums_ex.lua'))
    for m in re.finditer(r'^([A-Za-z]+)\.([A-Za-z0-9_]+)\s*=', ex, re.M):
        if m.group(1) != 'ModCallbacks':
            enum_add[m.group(1)].add(m.group(2))
    classes_global = {d['cls'] for d in docs} | set(display.values())
    classes_global.discard('REPENTOGON')
    classes_global |= {'Isaac', 'Game'}

    # ---- scan mods
    uses = []   # dicts
    cb_uses = collections.defaultdict(lambda: collections.defaultdict(list))
    enum_uses = collections.defaultdict(lambda: collections.defaultdict(list))
    suppressed = collections.Counter()
    name_re = re.compile(r'([:.])[ \t]*([A-Za-z_]\w*)')
    for label, d in mods:
        for f in lua_files(d):
            rel = os.path.relpath(f, d)
            raw = rc.read(f)
            text = strip_lua(raw)
            binds = analyse_file(text, classes_global)

            def binding_of(nm, binds=binds):
                if nm not in binds:
                    return None
                s, unk = binds[nm]
                if s == {'TABLE'} and not unk:
                    return 'TABLE'
                known = s - {'TABLE'}
                if len(known) == 1 and not unk and 'TABLE' not in s:
                    return next(iter(known))
                return None

            line_starts = [0]
            for m in re.finditer('\n', text):
                line_starts.append(m.end())
            import bisect

            def line_of(pos):
                return bisect.bisect_right(line_starts, pos)

            for m in name_re.finditer(text):
                sep, name = m.group(1), m.group(2)
                if name not in by_name:
                    continue
                p = m.start()
                if text.startswith('..', p) or (p > 0 and text[p - 1] == '.' and sep == '.') or (p > 0 and text[p - 1].isdigit() and sep == '.'):
                    continue
                if sep == ':' and p > 0 and text[p - 1] == ':':
                    continue
                prefix = text[line_starts[line_of(p) - 1]:p]
                if re.search(r'\bfunction[ \t]+[\w.:]*$', prefix):
                    suppressed['definition (function a:Name)'] += 1
                    continue
                pieces = chain_before(text, p)
                if (name == 'FromAngle' and pieces == [('id', 'Vector')]):
                    suppressed['Vector.FromAngle( ) is the game\'s static; REPENTOGON documents an instance method of that name'] += 1
                    continue
                cands = by_name[name]
                if sep == '.' and all(e['kind'] == 'fn' for e in cands) and not re.match(r'[ \t]*[\(\{"\']', text[m.end():]) \
                        and not (len(pieces) == 1 and pieces[0][0] == 'id' and pieces[0][1] in classes_global):
                    # x.Name that is never called, where every REPENTOGON Name is a function: a field of the mod's own table
                    # (Mod.Capsule, a table of capsule items), not the function
                    suppressed['field access x.%s without a call, and REPENTOGON has only functions of that name' % name] += 1
                    continue
                cand_classes = [norm(e['cls']) for e in cands]
                narrowed, reason = None, None
                recv = None
                if pieces:
                    if pieces[-1][0] == 'id' and len(pieces) == 1:
                        recv = pieces[0][1]
                    cls = eval_chain(pieces, binding_of, classes_global) if len(pieces) > 1 or pieces[0][0] == 'id' else None
                    if len(pieces) == 1 and pieces[0][0] == 'id':
                        nm = pieces[0][1]
                        if nm in classes_global and (norm(nm) in cand_classes or sep == '.'):
                            cls, reason = nm, 'class table %s' % nm
                        elif nm in MODISH:
                            suppressed['receiver is a mod/table name (%s)' % nm] += 1
                            continue
                        else:
                            b = binding_of(nm)
                            if b == 'TABLE':
                                suppressed['receiver is a mod table (a local assigned {} or RegisterMod)'] += 1
                                continue
                            if b:
                                cls, reason = b, 'every assignment of "%s" in the file is %s' % (nm, b)
                            else:
                                h = VARNAME.get(nm) or VARNAME.get(nm.lower())
                                if not h and re.search(r'(?:sprite|spr)$', nm, re.I):
                                    h = 'Sprite'   # pSprite, rendersprite, spr: a Sprite by naming habit
                                s_known = (binds[nm][0] - {'TABLE'}) if nm in binds else set()
                                if h and s_known <= {h}:
                                    cls, reason = h, 'variable name "%s"' % nm
                                else:
                                    cls = None
                    elif cls:
                        reason = 'call chain ends in %s' % cls
                    if not cls and len(pieces) >= 3 and pieces[-1][0] == 'call' and pieces[-2][0] in ('colon', 'dot') \
                            and RET.get(pieces[-2][1]) and pieces[-2][1] in ('GetSprite', 'ToNPC', 'ToPlayer', 'ToFamiliar', 'ToTear', 'ToLaser', 'ToBomb', 'ToPickup', 'ToProjectile', 'ToEffect', 'ToKnife', 'ToSlot'):
                        # the receiver's own class is unknown but the last call always returns one class (x:GetSprite():Name)
                        cls, reason = RET[pieces[-2][1]], 'call chain ends in %s()' % pieces[-2][1]
                    if cls and cls != 'TABLE':
                        narrowed = norm(cls)
                if narrowed:
                    chain, c = [narrowed], narrowed
                    while c in PARENT:
                        c = norm(PARENT[c])
                        chain.append(c)
                    sel = [e for e in cands if norm(e['cls']) in chain]
                    if not sel:
                        suppressed['narrowed to %s: REPENTOGON has no such name on it (the game\'s own method)' % cls] += 1
                        continue
                    cands = sel
                    is_narrowed = True
                else:
                    is_narrowed, reason = False, 'superset of %d class(es)' % len(cands)
                uses.append({'mod': label, 'file': rel, 'line': line_of(p), 'name': name, 'recv': recv,
                             'classes': sorted({e['cls'] for e in cands}), 'narrowed': is_narrowed, 'reason': reason,
                             'form': 'colon' if sep == ':' else 'dot',
                             'ambiguous': (not is_narrowed) and bool(vanilla_classes.get(name))})

            # constructors / bare calls: Name( where Name is a constructor class
            for m in re.finditer(r'(?<![\w.:])([A-Za-z_]\w*)\s*\(', text):
                nm = m.group(1)
                es = [e for e in by_name.get(nm, []) if e['kind'] == 'ctor']
                if es:
                    uses.append({'mod': label, 'file': rel, 'line': line_of(m.start()), 'name': nm, 'recv': None,
                                 'classes': sorted({e['cls'] for e in es}), 'narrowed': False, 'reason': 'constructor call',
                                 'form': 'ctor', 'ambiguous': False})
            for m in re.finditer(r'\bMC_[A-Z0-9_]+\b', text):
                if m.group(0) in cb_new:
                    cb_uses[m.group(0)][label].append((rel, line_of(m.start())))
            for m in re.finditer(r'\b([A-Z][A-Za-z]+)\.([A-Za-z0-9_]+)\b', text):
                if m.group(1) in enum_add and m.group(2) in enum_add[m.group(1)]:
                    enum_uses[m.group(1) + '.' + m.group(2)][label].append((rel, line_of(m.start())))

    # ---- aggregate
    labels = [l for l, _ in mods]
    res = collections.defaultdict(dict)   # cls -> name -> record
    amb = {}
    for u in uses:
        if u['ambiguous']:
            r = amb.setdefault(u['name'], {'vanilla_classes': sorted(vanilla_classes[u['name']]),
                                           'candidates': {},
                                           'uses': {l: 0 for l in labels}, 'example': {'mod': u['mod'], 'file': u['file'], 'line': u['line']}})
            r['uses'][u['mod']] += 1
            for c in u['classes']:
                r['candidates'].setdefault(c, pair_status(norm(c), u['name']))
    for u in uses:
        if u['ambiguous']:
            continue
        for cls in u['classes']:
            e = pairs[(norm(cls), u['name'])]
            r = res[cls].setdefault(u['name'], {'kind': e['kind'], 'status': pair_status(norm(cls), u['name']),
                                                'src': sorted(e['src']), 'uses': {l: 0 for l in labels},
                                                'narrowed_uses': 0, 'superset_uses': 0, 'example': None, 'reasons': {}})
            r['uses'][u['mod']] += 1
            r['narrowed_uses' if u['narrowed'] else 'superset_uses'] += 1
            r['reasons'][u['reason']] = r['reasons'].get(u['reason'], 0) + 1
            if r['example'] is None:
                r['example'] = {'mod': u['mod'], 'file': u['file'], 'line': u['line']}

    # per use status: missing if any candidate pair missing; else inert / unverified / implemented
    rank = {'missing': 3, 'inert': 2, 'unverified': 1, 'implemented': 0}
    per_name = collections.defaultdict(lambda: {'status': 'implemented', 'uses': {l: 0 for l in labels}, 'classes': set()})
    for u in uses:
        if u['ambiguous']:
            u['status'] = 'ambiguous'
            continue
        sts = [pair_status(norm(c), u['name']) for c in u['classes']]
        st = max(sts, key=lambda s: rank[s])
        u['status'] = st
        pn = per_name[u['name']]
        if rank[st] > rank[pn['status']]:
            pn['status'] = st
        pn['uses'][u['mod']] += 1
        if st != 'implemented':
            pn['classes'].update(c for c, s in zip(u['classes'], sts) if s == st)
    totals = {}
    for scope in labels + ['ALL']:
        sel = [u for u in uses if (scope == 'ALL' or u['mod'] == scope) and not u['ambiguous']]
        ambsel = {u['name'] for u in uses if (scope == 'ALL' or u['mod'] == scope) and u['ambiguous']}
        pairset = collections.defaultdict(set)
        for u in sel:
            for c in u['classes']:
                s = pair_status(norm(c), u['name'])
                pairset[s].add((c, u['name']))
        # a name's bucket is its worst status
        worst = {}
        for u in sel:
            if rank[u['status']] > rank.get(worst.get(u['name']), -1):
                worst[u['name']] = u['status']
        c = collections.Counter(worst.values())
        totals[scope] = {'names_used': len(worst), 'names_missing': c['missing'], 'names_inert': c['inert'],
                         'names_unverified': c['unverified'], 'names_implemented': c['implemented'],
                         'pairs_missing': len(pairset['missing']), 'pairs_inert': len(pairset['inert']),
                         'pairs_unverified': len(pairset['unverified']), 'names_ambiguous': len(ambsel),
                         'uses': len(sel), 'uses_narrowed': sum(1 for u in sel if u['narrowed']),
                         'uses_superset': sum(1 for u in sel if not u['narrowed'])}
    cbs = []
    for n in sorted(cb_uses, key=lambda x: cb_new[x]):
        i = cb_new[n]
        ent = {'name': n, 'id': i, 'fires': i in impl_ids, 'uses': {l: len(cb_uses[n].get(l, [])) for l in labels}}
        for l in labels:
            if cb_uses[n].get(l):
                ent.setdefault('example', {'mod': l, 'file': cb_uses[n][l][0][0], 'line': cb_uses[n][l][0][1]})
        cbs.append(ent)
    for scope in labels + ['ALL']:
        t = totals[scope]
        sel = [c for c in cbs if scope == 'ALL' or c['uses'][scope]]
        t['callbacks_used'] = len(sel)
        t['callbacks_missing'] = sum(1 for c in sel if not c['fires'])
    enums = []
    for k in sorted(enum_uses):
        cls, name = k.split('.')
        st = 'implemented' if norm(cls) + '.' + name in impl_n else ('exists' if norm(cls) + '.' + name in exists_n else 'missing')
        ent = {'name': k, 'status': st, 'uses': {l: len(enum_uses[k].get(l, [])) for l in labels}}
        for l in labels:
            if enum_uses[k].get(l):
                ent.setdefault('example', {'mod': l, 'file': enum_uses[k][l][0][0], 'line': enum_uses[k][l][0][1]})
        enums.append(ent)

    runtime = None
    if a.runtime_log:
        rt = set()
        for line in io.open(a.runtime_log, encoding='utf-8', errors='replace'):
            m = re.search(r'missing method ([A-Za-z_][\w]*)\.([A-Za-z_]\w*)', line)
            if m:
                rt.add(m.group(1) + '.' + m.group(2))
        static_pairs = {c + '.' + n for c, ns in res.items() for n, r in ns.items() if r['status'] != 'implemented'}
        static_names = {n for ns in res.values() for n in ns}
        runtime = {'seen': sorted(rt), 'not_in_static_list': sorted(x for x in rt if x not in static_pairs),
                   'name_not_in_static_at_all': sorted(x for x in rt if x.split('.')[1] not in static_names)}

    cmd = a.command or ' '.join(['python3', 'scripts/mac/rgon_backlog.py'] + [x if re.fullmatch(r'[\w./=:+-]+', x) else repr(x) for x in sys.argv[1:]])
    out = {'command': cmd, 'totals': totals, 'ambiguous': {n: amb[n] for n in sorted(amb)}, 'classes': {c: {n: res[c][n] for n in sorted(res[c])} for c in sorted(res)},
           'callbacks': cbs, 'enums': enums, 'suppressed': dict(suppressed), 'runtime': runtime,
           'uses': [{k: v for k, v in u.items()} for u in uses]}
    if a.out_json:
        with io.open(a.out_json, 'w', encoding='utf-8') as f:
            json.dump(out, f, indent=1, sort_keys=True)
            f.write('\n')
    if a.out_md:
        write_md(a.out_md, out, labels)
    t = totals['ALL']
    sys.stdout.write('names used %d (+%d ambiguous): missing %d, inert %d, unverified %d; callbacks used %d, not firing %d; uses %d (narrowed %d, superset %d)\n'
                     % (t['names_used'], t['names_ambiguous'], t['names_missing'], t['names_inert'], t['names_unverified'], t['callbacks_used'],
                        t['callbacks_missing'], t['uses'], t['uses_narrowed'], t['uses_superset']))


def write_md(path, out, labels):
    w = []
    w.append('# Backlog: REPENTOGON names used by %s that the Layer lacks\n' % ' and '.join(labels))
    w.append('Regenerate with:\n\n    %s\n' % (out['command'] or 'python3 scripts/mac/rgon_backlog.py --help'))
    w.append('Status: missing = absent on a candidate class; inert = exists as a stand-in (REPENTOGON-only name, not in '
             'Implemented); unverified = exists on the class but not Implemented and the bare name is not REPENTOGON-only '
             '(maybe the game has it, maybe a stand-in). A use with several candidate classes counts under every one '
             '("superset"); it is missing when ANY candidate lacks the name.\n')
    w.append('## Totals\n')
    w.append('| scope | names used | missing | inert | unverified | ambiguous | pairs missing | pairs inert | callbacks used | callbacks not firing | uses narrowed / superset |')
    w.append('|---|---|---|---|---|---|---|---|---|---|---|')
    for s, t in out['totals'].items():
        w.append('| %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d / %d |' % (
            s, t['names_used'], t['names_missing'], t['names_inert'], t['names_unverified'], t['names_ambiguous'], t['pairs_missing'],
            t['pairs_inert'], t['callbacks_used'], t['callbacks_missing'], t['uses_narrowed'], t['uses_superset']))
    w.append('\n"names" count bare names by their worst status (comparable to rgon_census.py); "pairs" count Class.Name.\n')
    classes = out['classes']

    def cnt(c, sts):
        return sum(1 for r in classes[c].values() if r['status'] in sts)
    order = sorted(classes, key=lambda c: (-cnt(c, ('missing',)), -cnt(c, ('inert', 'unverified')), c))
    w.append('## Functions, variables and constructors by class\n')
    for c in order:
        names = classes[c]
        bad = {n: r for n, r in names.items() if r['status'] != 'implemented'}
        if not bad:
            continue
        w.append('### %s (%d missing, %d inert, %d unverified)\n' % (c, cnt(c, ('missing',)), cnt(c, ('inert',)), cnt(c, ('unverified',))))
        w.append('| name | kind | status | ' + ' | '.join(labels) + ' | example | narrowing |')
        w.append('|---|---|---|' + '---|' * len(labels) + '---|---|')
        for n in sorted(bad, key=lambda n: (rank_of(bad[n]['status']), n)):
            r = bad[n]
            ex = r['example']
            reasons = '; '.join('%s (%d)' % (k, v) for k, v in sorted(r['reasons'].items(), key=lambda kv: -kv[1])[:3])
            w.append('| %s | %s | %s | %s | %s:%d | %s |' % (n, r['kind'], r['status'], ' | '.join(str(r['uses'][l]) for l in labels),
                                                          ex['file'], ex['line'], reasons))
        w.append('')
    w.append('## Ambiguous: REPENTOGON name that the game also has on another class, receiver class unknown\n')
    w.append('Not counted above. Each could be the game\'s own method (then fine) or REPENTOGON\'s on one of the candidates '
             '(then missing wherever the status below says so). Check by hand when the candidate list is short.\n')
    w.append('| name | ' + ' | '.join(labels) + ' | example | game has it on | REPENTOGON candidates (Layer status) |')
    w.append('|---|' + '---|' * len(labels) + '---|---|---|')
    for n, r in out['ambiguous'].items():
        ex = r['example']
        w.append('| %s | %s | %s:%d | %s | %s |' % (n, ' | '.join(str(r['uses'][l]) for l in labels), ex['file'], ex['line'],
                                                 ', '.join(r['vanilla_classes'][:6]) + (' ...' if len(r['vanilla_classes']) > 6 else ''),
                                                 ', '.join('%s %s' % (c, st) for c, st in sorted(r['candidates'].items()) if st != 'implemented') or 'all implemented'))
    w.append('\n## Implemented names used (for reference, use counts ' + ' / '.join(labels) + ')\n')
    for c in order:
        ok = ['%s(%s)' % (n, '/'.join(str(r['uses'][l]) for l in labels)) for n, r in classes[c].items() if r['status'] == 'implemented']
        if ok:
            w.append('- %s: %s' % (c, ', '.join(ok)))
    w.append('\n## Callbacks (ModCallbacks ids REPENTOGON adds)\n')
    w.append('| name | id | Layer fires | ' + ' | '.join(labels) + ' | example |')
    w.append('|---|---|---|' + '---|' * len(labels) + '---|')
    for c in out['callbacks']:
        ex = c.get('example')
        w.append('| %s | %d | %s | %s | %s |' % (c['name'], c['id'], 'yes' if c['fires'] else 'NO', ' | '.join(str(c['uses'][l]) for l in labels),
                                               '%s:%d' % (ex['file'], ex['line']) if ex else ''))
    w.append('\n## Inert stand-ins (exist, accept the call, do nothing)\n')
    w.append(', '.join('%s.%s' % (c, n) for c in order for n, r in classes[c].items() if r['status'] == 'inert') or 'none')
    w.append('\n## Unverified (exist on the class, not Implemented, bare name not REPENTOGON-only)\n')
    w.append(', '.join('%s.%s' % (c, n) for c in order for n, r in classes[c].items() if r['status'] == 'unverified') or 'none')
    w.append('\n## REPENTOGON enum entries used\n')
    w.append('| entry | status | ' + ' | '.join(labels) + ' |')
    w.append('|---|---|' + '---|' * len(labels))
    for e in out['enums']:
        w.append('| %s | %s | %s |' % (e['name'], e['status'], ' | '.join(str(e['uses'][l]) for l in labels)))
    w.append('\n## Not counted\n')
    for k, v in sorted(out['suppressed'].items(), key=lambda kv: -kv[1]):
        w.append('- %s: %d' % (k, v))
    if out['runtime']:
        rt = out['runtime']
        w.append('\n## Runtime check (lab_missing.lua)\n')
        w.append('Seen at runtime: %d. Not in the static list as a pair: %s\n' % (len(rt['seen']), ', '.join(rt['not_in_static_list']) or 'none'))
        w.append('Name absent from the static list altogether: %s\n' % (', '.join(rt['name_not_in_static_at_all']) or 'none'))
    with io.open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(w) + '\n')


def rank_of(s):
    return {'missing': 0, 'inert': 1, 'unverified': 2}.get(s, 3)


if __name__ == '__main__':
    main()
