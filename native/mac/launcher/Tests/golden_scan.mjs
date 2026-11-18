// Golden driver: the JS side of the Swift mod-scan comparison (Tests/run.sh). Not shipped.
//   node golden_scan.mjs lua   <mods dir> <out file>   what createModScan(...).lua() writes for those mods
//   node golden_scan.mjs files <mods dir> <out file>   per-file stripLua hash and scanLua facts
// A mod is a folder of <mods dir>; its id is the folder name; folders with a `disable.it`, dot-folders and
// the Layer's own folder are left out, as the Launcher does.
import { readFileSync, readdirSync, statSync, writeFileSync, existsSync } from 'node:fs';
import { join } from 'node:path';
import { createHash } from 'node:crypto';
import { createModScan, stripLua, scanLua } from '../../../../scripts/recomp/web/mods.mjs';

const [mode, dir, out] = process.argv.slice(2);
const sha = (s) => createHash('sha256').update(s, 'utf8').digest('hex');
const cmp = (a, b) => Buffer.compare(Buffer.from(a), Buffer.from(b));
const walk = (d, base, acc = []) => {
  for (const n of readdirSync(d)) {
    const p = join(d, n);
    const s = statSync(p);
    if (s.isDirectory()) walk(p, base, acc); else if (s.isFile()) acc.push({ rel: p.slice(base.length + 1), path: p });
  }
  return acc;
};
const mods = readdirSync(dir).sort().filter((n) => !n.startsWith('.') && n.toLowerCase() !== 'repentogon'
  && statSync(join(dir, n)).isDirectory() && !existsSync(join(dir, n, 'disable.it')));
const t0 = Date.now();
if (mode === 'lua') {
  const scan = createModScan();
  for (const id of mods) {
    const md = join(dir, id);
    scan.add(id, walk(md, md).map((f) => ({ rel: f.rel, bytes: readFileSync(f.path) })));
  }
  writeFileSync(out, scan.lua());
  console.error(`js lua: ${mods.length} enabled, ${scan.size()} name REPENTOGON, ${Date.now() - t0} ms`);
} else {
  const dec = new TextDecoder('utf-8');
  const lines = [];
  for (const id of mods) {
    const md = join(dir, id);
    for (const f of walk(md, md)) {
      if (!/\.lua$/i.test(f.rel)) continue;
      const t = dec.decode(readFileSync(f.path));
      const r = scanLua(t);
      const names = [...r.names].sort(cmp).join('\n'), defd = [...r.defined].sort(cmp).join('\n');
      lines.push(`${id}/${f.rel}\t${sha(stripLua(t))}\t${r.says} ${r.quits} ${r.yields}\t${sha(names)}\t${sha(defd)}`);
    }
  }
  lines.sort(cmp);
  writeFileSync(out, lines.join('\n') + '\n');
  console.error(`js files: ${lines.length} .lua file(s), ${Date.now() - t0} ms`);
}
