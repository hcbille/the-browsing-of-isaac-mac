#!/usr/bin/env python3
"""framestats.py <game.log> -- join the host's "[mac][frame]" timing lines with the Layer's room/level lines.

  python3 scripts/mac/framestats.py <support>/logs/game.log [--top N]

Prints one row per stats interval (with the floor and room the player was in at the end of it and the live entity
count), then the N slowest intervals (default 10, by game-work average), then a per-floor summary.

Log lines read:
  [mac][frame] frames A-B: avg .. | game work avg W max M ms | K over 20 ms [| p95 P ms work p95 Q ms]   (host, mac_gl.c)
  [isaac][rgon] level: stage S type T (name) frame F                                (api_framectx.lua)
  [isaac][rgon] room: stage S type T (name) room I roomtype R entities N frame F    (api_framectx.lua)
  [isaac][rgon] ents N room I frame F                                               (api_framectx.lua)
Older logs have no room lines: the context columns show "-" and the floor summary has one "unknown" row.
"""
import re
import sys

FRAME = re.compile(r'\[mac\]\[frame\] frames (\d+)-(\d+): avg ([\d.]+) ms \(([\d.]+) fps\) min ([\d.]+) max ([\d.]+) \| '
                   r'game work avg ([\d.]+) max ([\d.]+) ms \| (\d+) over 20 ms(?: \| p95 ([\d.]+) ms work p95 ([\d.]+) ms)?')
ROOM = re.compile(r'\[isaac\]\[rgon\] room: stage (\d+) type (\d+) \((.*?)\) room (-?\d+) roomtype (\d+) entities (-?\d+)')
LEVEL = re.compile(r'\[isaac\]\[rgon\] level: stage (\d+) type (\d+) \((.*?)\)')
ENTS = re.compile(r'\[isaac\]\[rgon\] ents (-?\d+) room (-?\d+)')


def parse(path):
    rows = []
    floor, room, ents = None, None, None
    with open(path, errors='replace') as f:
        for line in f:
            m = FRAME.search(line)
            if m:
                g = m.groups()
                rows.append(dict(a=int(g[0]), b=int(g[1]), avg=float(g[2]), fps=float(g[3]), max=float(g[5]),
                                 work=float(g[6]), wmax=float(g[7]), slow=int(g[8]),
                                 p95=float(g[9]) if g[9] else None, wp95=float(g[10]) if g[10] else None,
                                 floor=floor, room=room, ents=ents))
                continue
            m = LEVEL.search(line)
            if m:
                floor = '%s (stage %s.%s)' % (m.group(3), m.group(1), m.group(2))
                continue
            m = ROOM.search(line)
            if m:
                room = 'room %s type %s' % (m.group(4), m.group(5))
                ents = int(m.group(6))
                if floor is None:
                    floor = 'stage %s.%s' % (m.group(1), m.group(2))
                continue
            m = ENTS.search(line)
            if m:
                ents = int(m.group(1))
    return rows


def fmt(r):
    p = '%6.2f %6.2f' % (r['p95'], r['wp95']) if r['p95'] is not None else '     -      -'
    return '%7d-%-7d %7.2f %7.2f %7.2f %7.2f %s %4d %5s  %-28s %s' % (
        r['a'], r['b'], r['avg'], r['work'], r['wmax'], r['max'], p, r['slow'],
        '-' if r['ents'] is None else r['ents'], r['floor'] or '-', r['room'] or '-')


HEAD = '%-15s %7s %7s %7s %7s %6s %6s %4s %5s  %-28s %s' % (
    'frames', 'avg', 'work', 'wmax', 'max', 'p95', 'wp95', '>20', 'ents', 'floor', 'room')


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not args:
        print(__doc__)
        return 2
    top = 10
    if '--top' in sys.argv:
        top = int(sys.argv[sys.argv.index('--top') + 1])
        args = [a for a in args if a != str(top)]
    rows = parse(args[0])
    if not rows:
        print('no [mac][frame] lines in', args[0])
        return 1
    print('== every interval (times in ms; work = game work per frame; p95 columns need a build from 2026-11-19 or later)')
    print(HEAD)
    for r in rows:
        print(fmt(r))
    print('\n== the %d slowest intervals by game work avg' % top)
    print(HEAD)
    for r in sorted(rows, key=lambda r: -r['work'])[:top]:
        print(fmt(r))
    print('\n== per floor (intervals weighted by frames; "ents" = mean of the entity counts logged)')
    by = {}
    for r in rows:
        by.setdefault(r['floor'] or 'unknown', []).append(r)
    print('%-28s %5s %7s %8s %8s %8s %7s' % ('floor', 'ivals', 'frames', 'avg ms', 'work avg', 'work max', 'ents'))
    for k, rs in by.items():
        n = sum(r['b'] - r['a'] + 1 for r in rs)
        e = [r['ents'] for r in rs if r['ents'] is not None]
        print('%-28s %5d %7d %8.2f %8.2f %8.2f %7s' % (
            k, len(rs), n, sum(r['avg'] * (r['b'] - r['a'] + 1) for r in rs) / n,
            sum(r['work'] * (r['b'] - r['a'] + 1) for r in rs) / n, max(r['wmax'] for r in rs),
            '-' if not e else '%.0f' % (sum(e) / len(e))))
    return 0


if __name__ == '__main__':
    sys.exit(main())
