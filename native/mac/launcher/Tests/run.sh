#!/bin/bash
# Builds and runs the save-import / Workshop-sync / mod-scan tests. Usage: run.sh <scratch dir>
#
# Mod scan golden comparison (Swift ModScan vs createModScan in scripts/recomp/web/mods.mjs, byte for byte):
#  - always on Tests/fixtures/modscan/mods (edge cases)
#  - also on a real Workshop folder when REPENTANCE_MAC_GOLDEN_WORKSHOP is set (read only; never in the repo)
# The JS side needs `node`; without it that comparison is skipped and said so.
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
scratch="${1:?usage: run.sh <scratch dir>}"
mkdir -p "$scratch"
srcs=("$here/Sources/Model.swift" "$here/Sources/SyncSupport.swift" "$here/Sources/SaveImport.swift" \
      "$here/Sources/WorkshopSync.swift" "$here/Sources/ModScan.swift")
swiftc -O -swift-version 5 -target arm64-apple-macos13.0 -o "$scratch/sync-tests.bin" "$here/Tests/main.swift" "${srcs[@]}"
swiftc -O -swift-version 5 -target arm64-apple-macos13.0 -o "$scratch/golden.bin" "$here/Tests/golden/main.swift" "${srcs[@]}"
status=0
"$scratch/sync-tests.bin" "$scratch" || status=1

golden() { # <name> <mods dir>
  local name="$1" dir="$2" g="$scratch/golden-$1"
  mkdir -p "$g"
  "$scratch/golden.bin" lua "$dir" "$g/swift.lua"
  "$scratch/golden.bin" files "$dir" "$g/swift.files"
  if ! command -v node >/dev/null; then echo "SKIP  golden $name: no node, JS side not run"; return; fi
  node "$here/Tests/golden_scan.mjs" lua "$dir" "$g/js.lua"
  node "$here/Tests/golden_scan.mjs" files "$dir" "$g/js.files"
  if cmp -s "$g/swift.lua" "$g/js.lua"; then
    echo "ok    golden $name: modscan.lua byte-identical, sha256 $(shasum -a 256 < "$g/js.lua" | cut -d' ' -f1)"
  else echo "FAIL  golden $name: modscan.lua differs ($g/swift.lua vs $g/js.lua)"; status=1; fi
  if cmp -s "$g/swift.files" "$g/js.files"; then
    echo "ok    golden $name: stripLua and scanLua identical on all $(wc -l < "$g/js.files" | tr -d ' ') .lua file(s)"
  else echo "FAIL  golden $name: per-file facts differ (diff $g/swift.files $g/js.files)"; status=1; fi
}
golden fixtures "$here/Tests/fixtures/modscan/mods"
if [ -n "${REPENTANCE_MAC_GOLDEN_WORKSHOP:-}" ]; then golden workshop "$REPENTANCE_MAC_GOLDEN_WORKSHOP"
else echo "SKIP  golden workshop: REPENTANCE_MAC_GOLDEN_WORKSHOP not set"; fi
[ "$status" = 0 ] && echo "RUN.SH: ALL PASSED" || echo "RUN.SH: FAILURES"
exit $status
