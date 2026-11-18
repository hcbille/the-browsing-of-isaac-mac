#!/bin/bash
# Build Repentance+.app (the Mac Build) from a game folder. One resumable command.
#
#   scripts/mac/build.sh --game <dir> [--out <dir>] [--jobs N] [--force-stage X]...
#
# Every stage records a stamp (a hash of its inputs and of the stamps of the stages it
# depends on) and skips itself when the stamp matches and its output exists. See
# docs/mac/build.md for the stage list, the environment variables and the resume rules.
#
# Options
#   --game <dir>        the Windows game folder (needs isaac-ng.exe and resources/). Read only.
#   --out <dir>         where Repentance+.app is written (default: output/mac-app)
#   --jobs N            parallel compile / lift jobs for the heavy stages (default 3)
#   --force-stage X     run stage X even if current (repeatable; "all" forces every stage).
#                       Stages after X rerun on their own, their stamps include X's.
#   --no-seed           do not adopt existing outputs that have no stamp yet (default: adopt them)
#   --dry-run           print what each stage would do, run nothing
#   --list              print the stage names and exit
#
# Environment (no machine paths are built in; all have a discovery default)
#   ISAAC_TOOLCHAIN_ENV  a shell file to source for Python, Java, Ghidra and Emscripten
#                        (default: <repo>/../toolchain/env.sh, then <repo>/toolchain/env.sh;
#                        none found: the current environment is used as it is)
#   ISAAC_TOOLCHAIN_DIR  folder with steamless/ and angle/ (default: the env file's folder,
#                        else <repo>/../toolchain)
#   STEAMLESS_DIR        Steamless.CLI.exe and Plugins/ (default: $ISAAC_TOOLCHAIN_DIR/steamless)
#   ANGLE_DIR            libEGL.dylib and libGLESv2.dylib, Metal backend, @rpath names
#                        (default: $ISAAC_TOOLCHAIN_DIR/angle)
#   SDL_PREFIX           SDL3 install prefix (default: `brew --prefix sdl3`)
#   STEAM_API_DYLIB      libsteam_api.dylib to bundle (default: the local Steam client's copy;
#                        absent: skipped with a warning, the Mac Build runs without Steam)
#   ISAAC_STAMP_DIR      where the pipeline stages keep their stamps (default: output/mac/stamps)
#   ISAAC_NATIVE_OUT     where build_native.py writes isaac-mac (default: output/mac)
#
# NEVER commit the outputs: they contain game-derived data.
set -eu
set -o pipefail

REPO=$(cd "$(dirname "$0")/../.." && pwd)
cd "$REPO"

GAME=""
OUT="$REPO/output/mac-app"
JOBS=3
FORCE=" "
SEED=1
DRY=0
STAGES="steamless ghidra tables lift build_boot bundle memimage build_native launcher layer app dylibs config sign"

die() { echo "build.sh: $*" >&2; exit 1; }
say() { echo "[build] $*"; }
warn() { echo "[build] WARNING: $*" >&2; }

while [ $# -gt 0 ]; do
    case "$1" in
        --game) GAME=${2:?--game needs a folder}; shift 2 ;;
        --out) OUT=${2:?--out needs a folder}; shift 2 ;;
        --jobs) JOBS=${2:?--jobs needs a number}; shift 2 ;;
        --force-stage) FORCE="$FORCE${2:?--force-stage needs a name} "; shift 2 ;;
        --no-seed) SEED=0; shift ;;
        --dry-run) DRY=1; shift ;;
        --list) echo $STAGES; exit 0 ;;
        -h|--help) sed -n '2,/^set -eu/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "unknown option $1 (try --help)" ;;
    esac
done
[ -n "$GAME" ] || die "--game <dir> is required"
GAME=$(cd "$GAME" 2>/dev/null && pwd) || die "game folder not found"
[ -f "$GAME/isaac-ng.exe" ] || die "$GAME/isaac-ng.exe not found"
[ -d "$GAME/resources" ] || die "$GAME/resources not found"
for s in $FORCE; do
    case " $STAGES all " in *" $s "*) ;; *) die "unknown stage $s (stages: $STAGES)" ;; esac
done
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

# ---- toolchain discovery --------------------------------------------------------------
TC_ENV=${ISAAC_TOOLCHAIN_ENV:-}
if [ -z "$TC_ENV" ]; then
    for c in "$REPO/../toolchain/env.sh" "$REPO/toolchain/env.sh"; do
        [ -f "$c" ] && { TC_ENV=$c; break; }
    done
fi
if [ -n "$TC_ENV" ]; then
    [ -f "$TC_ENV" ] || die "ISAAC_TOOLCHAIN_ENV=$TC_ENV not found"
    # shellcheck disable=SC1090
    . "$TC_ENV"
    say "toolchain env: sourced"
else
    say "toolchain env: none found, using the current environment"
fi
if [ -z "${ISAAC_TOOLCHAIN_DIR:-}" ]; then
    if [ -n "$TC_ENV" ]; then ISAAC_TOOLCHAIN_DIR=$(cd "$(dirname "$TC_ENV")" && pwd)
    else ISAAC_TOOLCHAIN_DIR="$REPO/../toolchain"; fi
fi
STEAMLESS_DIR=${STEAMLESS_DIR:-$ISAAC_TOOLCHAIN_DIR/steamless}
ANGLE_DIR=${ANGLE_DIR:-$ISAAC_TOOLCHAIN_DIR/angle}
export ANGLE_DIR
PY=python
command -v "$PY" >/dev/null 2>&1 || PY=python3
STAMPS=${ISAAC_STAMP_DIR:-$REPO/output/mac/stamps}
NATIVE_OUT=${ISAAC_NATIVE_OUT:-$REPO/output/mac}
APP_STAMPS="$OUT/.stamps"
WORK="$OUT/work"
APP="$OUT/Repentance+.app"
C="$APP/Contents"
mkdir -p "$STAMPS" "$APP_STAMPS" "$WORK"

sha() { shasum -a 256 | cut -c1-16; }
# fingerprint helpers: content hashes of files
fp_files() { for f in "$@"; do [ -e "$f" ] && shasum -a 256 "$f"; done | sha; }
fp_tree() { # <dir> [find args]: content of every regular file under dir
    ( cd "$1" && find . -type f ! -name '.DS_Store' ! -name '*.pyc' ! -path '*/__pycache__/*' ! -name '*.o' | LC_ALL=C sort | while read -r f; do shasum -a 256 "$f"; done ) | sha
}
fp_glob() { # <dir> <pattern>...: files directly in dir
    d=$1; shift
    ( cd "$d" && for p in "$@"; do for f in $p; do [ -f "$f" ] && shasum -a 256 "$f"; done; done ) | sha
}
fp_game() { # the game folder as the asset bundle sees it: names, sizes, mtimes (not contents)
    { find "$GAME" -maxdepth 1 -type f -exec stat -f '%N %z %m' {} +
      find "$GAME/resources" -type f -exec stat -f '%N %z %m' {} +; } | LC_ALL=C sort | sha
}

# ---- stage plumbing ---------------------------------------------------------------------
# A stage has: fp_<name> (inputs fingerprint), ok_<name> (outputs exist), run_<name>.
# stamp file content: "<fingerprint> <run id>". S_<name> holds it, downstream stages hash it in.
stage_dir() { case "$1" in launcher|layer|app|dylibs|config|sign) echo "$APP_STAMPS" ;; *) echo "$STAMPS" ;; esac; }
forced() { case "$FORCE" in *" $1 "*|*" all "*) return 0 ;; esac; return 1; }

do_stage() {
    name=$1
    sf="$(stage_dir "$name")/$name.stamp"
    fp=$( { "fp_$name"; } | sha )
    cur=""
    [ -f "$sf" ] && cur=$(cat "$sf")
    if forced "$name"; then
        reason="forced"
    elif "ok_$name" 2>/dev/null && [ "${cur%% *}" = "$fp" ]; then
        eval "S_$name=\"\$cur\""
        say "$name: current, skipped"
        return 0
    elif "ok_$name" 2>/dev/null && [ -z "$cur" ] && [ "$SEED" = 1 ] && is_seedable "$name"; then
        if [ "$DRY" = 1 ]; then say "$name: would seed its stamp from the existing output"; eval "S_$name=\"\$fp seed\""; return 0; fi
        printf '%s seed\n' "$fp" > "$sf"
        eval "S_$name=\"\$fp seed\""
        say "$name: output exists, no stamp: seeded (inputs hash $fp), skipped"
        return 0
    elif [ -n "$cur" ]; then reason="inputs changed"
    else reason="no output"; fi
    say "$name: running ($reason)"
    if [ "$DRY" = 1 ]; then eval "S_$name=\"\$fp dry\""; return 0; fi
    t0=$(date +%s)
    "run_$name"
    "ok_$name" || die "stage $name finished but its output is missing"
    run="$(date +%Y%m%dT%H%M%S)"
    printf '%s %s\n' "$fp" "$run" > "$sf"
    eval "S_$name=\"\$fp \$run\""
    say "$name: done in $(( $(date +%s) - t0 )) s"
}
# the cheap app stages are rebuilt, never adopted
is_seedable() { case "$1" in steamless|ghidra|tables|lift|build_boot|bundle|memimage|build_native) return 0 ;; esac; return 1; }

# ---- stages -----------------------------------------------------------------------------
EXE=tools/isaac-ng.unpacked.exe
CANON_SHA=5129df723e645daaea59514394195f3ea1dce1671bb0433d724648a845017200
BUNDLE=.scratch/game-bundle
LIFT=output/recomp/lift/gu
S_steamless=""; S_ghidra=""; S_tables=""; S_lift=""; S_build_boot=""; S_bundle=""
S_memimage=""; S_build_native=""; S_launcher=""; S_layer=""; S_app=""; S_dylibs=""; S_config=""

# 1. Steamless unpack, then the project's 21 historical patch spans -> the canonical exe
fp_steamless() { fp_files "$GAME/isaac-ng.exe" scripts/recomp/prepare-input.py; }
ok_steamless() { [ -f "$EXE" ] && [ "$(shasum -a 256 "$EXE" | cut -d' ' -f1)" = "$CANON_SHA" ]; }
run_steamless() {
    command -v mono >/dev/null || die "mono not found (needed for Steamless; brew install mono)"
    [ -f "$STEAMLESS_DIR/Steamless.CLI.exe" ] || die "Steamless.CLI.exe not found in $STEAMLESS_DIR (set STEAMLESS_DIR)"
    tmp=$(mktemp -d "${TMPDIR:-/tmp}/isaac-steamless.XXXXXX")
    cp "$GAME/isaac-ng.exe" "$tmp/isaac-ng.exe"     # the game folder itself is never written
    MONO_PATH="$STEAMLESS_DIR/Plugins${MONO_PATH:+:$MONO_PATH}" \
        mono "$STEAMLESS_DIR/Steamless.CLI.exe" --quiet "$tmp/isaac-ng.exe"
    [ -f "$tmp/isaac-ng.exe.unpacked.exe" ] || die "Steamless produced no unpacked file"
    mkdir -p tools
    "$PY" scripts/recomp/prepare-input.py --input "$tmp/isaac-ng.exe.unpacked.exe"
    rm -rf "$tmp"
}

# 2. Ghidra bulk export (about 50 min), the census, the PE index the lifter reads
fp_ghidra() { echo "$S_steamless"; fp_glob scripts/recomp export-all.mjs; fp_glob scripts/recomp/census '*.py'; fp_files scripts/decomp/tools/build-pe-index.py; }
ok_ghidra() { [ -f output/recomp/export/decompile-complete.marker ] && [ -f output/recomp/export/functions.jsonl ] \
    && [ -f output/recomp/census/imports.json ] && [ -d output/decomp ] && [ -n "$(ls output/decomp 2>/dev/null)" ]; }
run_ghidra() {
    node scripts/recomp/export-all.mjs --stage all
    "$PY" scripts/recomp/census/run_all.py
    "$PY" scripts/decomp/tools/build-pe-index.py
}

# 3. Tables and shims: IAT/boot tables, then gen_shims (shim-table.json + host/generated/*)
fp_tables() { echo "$S_ghidra"; fp_glob scripts/recomp/host boot_tables.py gen_shims.py pe.py; }
ok_tables() { [ -f output/recomp/host/shim-table.json ] && [ -f output/recomp/host/boot-tables.json ] \
    && [ -f scripts/recomp/host/generated/shim_table.c ]; }
run_tables() {
    "$PY" scripts/recomp/host/boot_tables.py
    "$PY" scripts/recomp/host/gen_shims.py
}

# 4. Lift: start list, then the parallel lifter (exact arguments of docs/DEVELOPING.md)
fp_lift() { echo "$S_tables"; fp_glob scripts/recomp/lift '*.py' '*.c' '*.h'; fp_files scripts/recomp/host/src/missing_fns.c; }
ok_lift() { [ -f "$LIFT/lifted_000.c" ] && [ -f output/recomp/lift/starts_union.txt ] && [ -f "$LIFT/stats.json" ]; }
run_lift() {
    "$PY" scripts/recomp/lift/prepare_starts.py
    "$PY" scripts/recomp/lift/orphan_starts.py --write
    "$PY" scripts/recomp/lift/lift_parallel.py --jobs "$JOBS" \
        --exe "$EXE" \
        --ghidra-functions output/recomp/export/functions.jsonl \
        --fragments-tsv output/recomp/export/recovered-functions.tsv \
        --imports output/recomp/census/imports.json \
        --shim-table output/recomp/host/shim-table.json \
        --va-file output/recomp/lift/starts_union.txt \
        --hand-written scripts/recomp/host/src/missing_fns.c \
        --dispatch auto --follow --max-insns 100000 --split-va 0x30000 --trace-va \
        --out "$LIFT" --module lifted \
        --stats "$LIFT/stats.json"
    "$PY" scripts/recomp/lift/patch_reentry.py --dir "$LIFT" --exe "$EXE"
}

# 5. build_boot: applies the lift patches to the lifted tree in place and writes
#    stubs.c / dispatch_tbl.c (the Mac Build compiles those). Same command as DEVELOPING.md,
#    so it also links the Browser Build's module (the slow part; needs Emscripten).
fp_build_boot() { echo "$S_lift"; fp_glob scripts/recomp/lift 'build_boot.py' 'lift_patches.py' 'split_giants.py' 'mkstubs.py' 'mkdispatch.py' 'check_lifted.py'; }
ok_build_boot() { [ -f "$LIFT/dispatch_tbl.c" ] && [ -f "$LIFT/stubs.c" ]; }
run_build_boot() { "$PY" scripts/recomp/lift/build_boot.py --web --fast --jobs "$JOBS"; }

# 6. Asset bundle (hard links into the game folder; the game folder is read only)
fp_bundle() { fp_game; fp_files scripts/recomp/assets/bundle.py; }
ok_bundle() { [ -f "$BUNDLE/.bundle.json" ] && [ -d "$BUNDLE/resources" ]; }
run_bundle() { "$PY" scripts/recomp/assets/bundle.py build "$GAME" "$BUNDLE" --strict; }

# 7. Memory image (isaac.segs.bin: the exe's sections, zero runs elided)
fp_memimage() { echo "$S_steamless"; fp_glob scripts/recomp/host memimage.py verify_memimage.py pe.py; }
ok_memimage() { [ -f output/recomp/host/isaac.segs.bin ] && [ -f output/recomp/host/memimage-verify.json ]; }
run_memimage() {
    "$PY" scripts/recomp/host/memimage.py
    "$PY" scripts/recomp/host/verify_memimage.py
}

# 8. Native executable (arm64, ANGLE + SDL3); build_native.py is itself incremental per object
fp_build_native() {
    echo "$S_build_boot"; echo "$S_tables"
    fp_tree native/mac/host
    fp_tree scripts/recomp/host/src; fp_tree scripts/recomp/host/include; fp_tree scripts/recomp/host/generated
    fp_glob scripts/recomp/lift boot_integration.c recomp_rt.c recomp_rt.h
    fp_files scripts/mac/build_native.py
}
ok_build_native() { [ -x "$NATIVE_OUT/isaac-mac" ]; }
run_build_native() {
    if [ ! -f tools/lua-5.3.3/src/lua.h ]; then   # Lua 5.3.3 source, from lua.org, checksum checked
        lua_sha=5113c06884f7de453ce57702abaac1d618307f33f6789fa870e87a59d772aca2
        if [ ! -f tools/lua-5.3.3.tar.gz ]; then
            say "downloading Lua 5.3.3 from lua.org"
            curl -fsSL -o tools/lua-5.3.3.tar.gz https://www.lua.org/ftp/lua-5.3.3.tar.gz
        fi
        [ "$(shasum -a 256 tools/lua-5.3.3.tar.gz | cut -d' ' -f1)" = "$lua_sha" ] || die "tools/lua-5.3.3.tar.gz has the wrong checksum"
        tar -xzf tools/lua-5.3.3.tar.gz -C tools
    fi
    [ -f "$ANGLE_DIR/libGLESv2.dylib" ] || die "ANGLE not found in $ANGLE_DIR (set ANGLE_DIR; docs/mac/graphics-spike.md)"
    "$PY" scripts/mac/build_native.py --jobs "$JOBS" --out "$NATIVE_OUT"
}

# 9. Launcher (SwiftUI, plain swiftc)
fp_launcher() { fp_tree native/mac/launcher/Sources; fp_files native/mac/launcher/build.sh native/mac/launcher/Info.plist; }
ok_launcher() { [ -x "$WORK/Launcher" ]; }
run_launcher() { native/mac/launcher/build.sh "$WORK"; }

# 10. Layer files and layer-info.json
fp_layer() {
    fp_tree scripts/recomp/rgon; fp_tree third_party/REPENTOGON/repentogon/resources/scripts
    fp_files scripts/mac/stage_layer.py native/mac/launcher/gen_layer_info.py third_party/REPENTOGON/LICENSE
}
ok_layer() { [ -f "$WORK/layer/rgon.lua" ] && [ -f "$WORK/layer-info.json" ]; }
run_layer() {
    rm -rf "$WORK/layer"
    "$PY" scripts/mac/stage_layer.py "$WORK/layer"
    "$PY" native/mac/launcher/gen_layer_info.py \
        --enums third_party/REPENTOGON/repentogon/resources/scripts/enums_ex.lua --out "$WORK/layer-info.json"
}

# 11. App skeleton: Info.plist, both programs, memory image, asset bundle (APFS clone, no copy
#     of the data when the volumes match), Layer, icon
fp_app() {
    echo "$S_build_native"; echo "$S_launcher"; echo "$S_layer"; echo "$S_memimage"; echo "$S_bundle"
    fp_files native/mac/launcher/Info.plist scripts/mac/make_icon.py scripts/mac/build.sh
    shasum -a 256 "$NATIVE_OUT/isaac-mac" 2>/dev/null
}
ok_app() { [ -x "$C/MacOS/isaac-mac" ] && [ -f "$C/Info.plist" ] && [ -d "$C/Resources/game-bundle" ]; }
clone() { cp -c -R "$1" "$2" 2>/dev/null || cp -R "$1" "$2"; }
run_app() {
    rm -rf "$APP"
    mkdir -p "$C/MacOS" "$C/Frameworks" "$C/Resources"
    cp native/mac/launcher/Info.plist "$C/Info.plist"
    launcher_exe=$(plutil -extract CFBundleExecutable raw "$C/Info.plist")
    cp "$WORK/Launcher" "$C/MacOS/$launcher_exe"
    clone "$NATIVE_OUT/isaac-mac" "$C/MacOS/isaac-mac"
    chmod u+w "$C/MacOS/isaac-mac"
    cp output/recomp/host/isaac.segs.bin "$C/Resources/isaac.segs.bin"
    clone "$BUNDLE" "$C/Resources/game-bundle"
    clone "$WORK/layer" "$C/Resources/layer"
    cp "$WORK/layer-info.json" "$C/Resources/layer-info.json"
    if ! "$PY" scripts/mac/make_icon.py "$WORK/AppIcon.icns" 2>/dev/null; then
        warn "icon not made (needs Pillow and iconutil); the app gets the default icon"
    elif [ -f "$WORK/AppIcon.icns" ]; then
        cp "$WORK/AppIcon.icns" "$C/Resources/AppIcon.icns"
    fi
}

# 12. Frameworks: ANGLE, SDL3, libsteam_api (when found) with @rpath ids, LC_RPATH fixed
fix_deps() { # <macho> <rpath to set>: non-system deps -> @rpath/<leaf>, replace all LC_RPATHs
    f=$1; rp=$2
    otool -arch arm64 -L "$f" | tail -n +2 | awk '{print $1}' | while read -r d; do
        case "$d" in /usr/lib/*|/System/*|@rpath/*) ;;
            *) install_name_tool -change "$d" "@rpath/$(basename "$d")" "$f" ;; esac
    done
    otool -arch arm64 -l "$f" | awk '/LC_RPATH/{r=1} r&&/ path /{print $2; r=0}' | while read -r p; do
        install_name_tool -delete_rpath "$p" "$f"
    done
    install_name_tool -add_rpath "$rp" "$f"
}
fp_dylibs() {
    echo "$S_app"; fp_files scripts/mac/build.sh
    fp_files "$ANGLE_DIR/libEGL.dylib" "$ANGLE_DIR/libGLESv2.dylib" "$(sdl_dylib)" "$(steam_dylib)"
}
ok_dylibs() { [ -f "$C/Frameworks/libGLESv2.dylib" ] && [ -f "$C/Frameworks/libSDL3.0.dylib" ] \
    && otool -arch arm64 -l "$C/MacOS/isaac-mac" | grep -q '@executable_path/../Frameworks'; }
sdl_dylib() {
    p=${SDL_PREFIX:-$(brew --prefix sdl3 2>/dev/null || true)}
    [ -n "$p" ] && [ -f "$p/lib/libSDL3.0.dylib" ] && echo "$p/lib/libSDL3.0.dylib" || true
}
steam_dylib() {
    if [ -n "${STEAM_API_DYLIB:-}" ]; then [ -f "$STEAM_API_DYLIB" ] && echo "$STEAM_API_DYLIB"; return 0; fi
    for g in "$HOME"/Library/Application\ Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/Frameworks/*/Contents/MacOS/libsteam_api.dylib; do
        [ -f "$g" ] && { echo "$g"; return 0; }
    done
    return 0
}
run_dylibs() {
    fw="$C/Frameworks"
    [ -f "$ANGLE_DIR/libGLESv2.dylib" ] && [ -f "$ANGLE_DIR/libEGL.dylib" ] || die "ANGLE not found in $ANGLE_DIR (set ANGLE_DIR)"
    sdl=$(sdl_dylib); [ -n "$sdl" ] || die "SDL3 not found (brew install sdl3, or set SDL_PREFIX)"
    cp "$ANGLE_DIR/libEGL.dylib" "$ANGLE_DIR/libGLESv2.dylib" "$fw/"
    cp -L "$sdl" "$fw/libSDL3.0.dylib"
    steam=$(steam_dylib)
    if [ -n "$steam" ]; then cp "$steam" "$fw/libsteam_api.dylib"
    else warn "libsteam_api.dylib not found (no local Steam client; set STEAM_API_DYLIB): Steam features are skipped"; fi
    chmod u+w "$fw"/*.dylib
    for l in "$fw"/*.dylib; do
        install_name_tool -id "@rpath/$(basename "$l")" "$l" 2>/dev/null
        fix_deps "$l" "@loader_path" 2>&1 | grep -v "invalidate the code signature" || true
    done
    fix_deps "$C/MacOS/isaac-mac" "@executable_path/../Frameworks" 2>&1 | grep -v "invalidate the code signature" || true
    bad=$( { for f in "$fw"/*.dylib "$C/MacOS/isaac-mac"; do otool -arch arm64 -L "$f" | tail -n +2 | awk '{print $1}' \
        | grep -v -e '^/usr/lib/' -e '^/System/' -e '^@rpath/' || true; done; } )
    [ -z "$bad" ] || die "non-system, non-@rpath dependencies remain: $bad"
}

# 13. sync-config.json: the Workshop fallback and the save search, this machine's (never committed).
#     The Launcher reads <support>/sync-config.json; it must also read this copy (docs/mac/build.md).
fp_config() { echo "$S_dylibs"; echo "$GAME"; fp_files scripts/mac/build.sh; }
ok_config() { [ -f "$C/Resources/sync-config.json" ]; }
run_config() {
    "$PY" - "$GAME" "$C/Resources/sync-config.json" <<'EOF'
import json, sys
game, out = sys.argv[1], sys.argv[2]
json.dump({"workshop_fallback": [game + "/steamapps/workshop/content/250900"],
           "save_search": [game]}, open(out, "w"), indent=2)
open(out, "a").write("\n")
EOF
}

# 14. Ad-hoc codesign, inner code first, then the app
fp_sign() { echo "$S_config"; fp_files scripts/mac/build.sh; }
ok_sign() { codesign --verify --deep --strict "$APP" 2>/dev/null; }
run_sign() {
    for f in "$C"/Frameworks/*.dylib "$C/MacOS/isaac-mac" "$C/MacOS/$(plutil -extract CFBundleExecutable raw "$C/Info.plist")"; do
        codesign --force --sign - --timestamp=none "$f"
    done
    codesign --force --sign - --timestamp=none "$APP"
    codesign --verify --deep --strict "$APP"
}

# ---- go ---------------------------------------------------------------------------------
say "game: $GAME"
say "app:  $APP"
for s in $STAGES; do do_stage "$s"; done
say "done: $APP"
