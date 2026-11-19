#!/bin/bash
# Download the exact pinned Windows game build (see game-pin.txt) with SteamCMD and merge the depots
# into one folder that build.sh accepts as --game.
#
#   scripts/mac/fetch-game.sh --user <steam login> --out <dir> [--steamcmd <path>] [--content-dir <dir>]
#   scripts/mac/fetch-game.sh --merge-from <content dir> --out <dir>
#
# You must own Rebirth and all of its DLCs (Afterbirth, Afterbirth+, Repentance, Repentance+) on the
# Steam account you log in with. Every depot is fetched at its pinned manifest, never the current one.
# Each is reported as fetched or not found (not owned, or failed). A missing required depot stops the
# script with the DLC's name and store app id.
#
# Options
#   --user <login>        your Steam account name. SteamCMD asks for the password and the Steam Guard
#                         code itself, in this terminal. This script never reads or stores them.
#   --out <dir>           the merged game folder to create (must be empty or absent)
#   --steamcmd <path>     the steamcmd executable (default: steamcmd from PATH)
#   --content-dir <dir>   where SteamCMD put the depots, when it is not found by itself: the folder
#                         that holds app_250900/depot_<id>/ (also used by --merge-from)
#   --merge-from <dir>    skip SteamCMD: merge depots already downloaded, e.g. with the Steam client
#                         console (below). <dir> holds depot_<id>/ folders, or app_250900/depot_<id>/.
#   --app-info-file <f>   check a saved `app_info_print <app>` dump for depots that are not pinned
#                         (with --merge-from; with SteamCMD the dump is taken in the same session)
#   --print-cmds          print the Steam console commands for the pinned depots and exit
#   --keep-depots         copy instead of move (needs the disk space twice)
#
# Other DLC depots: after login the same SteamCMD session prints app_info_print <app>; Windows depots
# with a dlcappid that are not in game-pin.txt are listed as a warning. They are NOT fetched (the
# current manifest would be the wrong patch): add a pinned manifest to game-pin.txt first. The check
# is best effort (it needs the `script` tool to keep the password prompt working, and parses a
# format Valve does not document).
#
# Without SteamCMD: open the Steam client console (steam://open/console), then run the lines printed
# by --print-cmds (download_depot <app> <depot> <manifest>). Steam says where each depot landed,
# usually <Steam>/steamapps/content/app_<app>/depot_<id>/. Then: fetch-game.sh --merge-from <that
# content folder> --out <dir>. (A Mac Steam client may refuse Windows depots; use a Windows or Linux
# client, or SteamCMD with the platform forced to windows, which this script does.)
#
# Disk: depots are moved (rsync --remove-source-files), not copied, so the disk is not doubled.
# The result is checked against exe_sha256 of game-pin.txt.
set -eu
set -o pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
PIN=${ISAAC_GAME_PIN:-$HERE/game-pin.txt}

die() { echo "fetch-game.sh: $*" >&2; exit 1; }
say() { echo "[fetch-game] $*"; }
warn() { echo "[fetch-game] WARNING: $*" >&2; }

USER_NAME=""; OUT=""; STEAMCMD=steamcmd; CONTENT=""; MERGE_FROM=""; KEEP=0; PRINT=0; INFO_FILE=""
while [ $# -gt 0 ]; do
    case "$1" in
        --user) USER_NAME=${2:?--user needs a Steam login}; shift 2 ;;
        --out) OUT=${2:?--out needs a folder}; shift 2 ;;
        --steamcmd) STEAMCMD=${2:?--steamcmd needs a path}; shift 2 ;;
        --content-dir) CONTENT=${2:?--content-dir needs a folder}; shift 2 ;;
        --merge-from) MERGE_FROM=${2:?--merge-from needs a folder}; shift 2 ;;
        --app-info-file) INFO_FILE=${2:?--app-info-file needs a file}; shift 2 ;;
        --keep-depots) KEEP=1; shift ;;
        --print-cmds) PRINT=1; shift ;;
        -h|--help) sed -n '2,/^set -eu/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "unknown option $1 (try --help)" ;;
    esac
done

[ -f "$PIN" ] || die "pin file $PIN not found"
pin_get() { awk -v k="$1" '$1==k {print $2; exit}' "$PIN"; }
APP=$(pin_get app); VERSION=$(pin_get version); BUILD=$(pin_get build_id); WANT_SHA=$(pin_get exe_sha256)
# one line per depot: id|manifest|dlcappid|required(1/0)|name
DEPOTS=$(awk '$1=="depot"||$1=="optional_depot" {
    n=""; for (i=5;i<=NF;i++) n=n (i>5?" ":"") $i
    print $2 "|" $3 "|" $4 "|" ($1=="depot"?1:0) "|" n }' "$PIN")
[ -n "$APP" ] && [ -n "$WANT_SHA" ] && [ -n "$DEPOTS" ] || die "$PIN is incomplete"

if [ "$PRINT" = 1 ]; then
    echo "$DEPOTS" | while IFS='|' read -r id man dlc req name; do echo "download_depot $APP $id $man"; done
    exit 0
fi

[ -n "$OUT" ] || die "--out <dir> is required"
if [ -e "$OUT" ] && [ -n "$(ls -A "$OUT" 2>/dev/null)" ]; then die "$OUT is not empty"; fi
command -v rsync >/dev/null || die "rsync not found"

# Windows depots with a dlcappid in an app_info_print dump (KeyValues text): prints "<depot> <dlcappid>".
parse_dlc_depots() { # <dump file>
    tr -d '\r' < "$1" | awk '
        { gsub(/^[ \t]+|[ \t]+$/, "") }
        /^"[^"]*"$/ { pend = substr($0, 2, length($0) - 2); next }
        $0 == "{" { depth++; key[depth] = pend; pend = ""
                    # depots/<id>/manifests: a depot with content (a bare depots/<dlcappid> entry has none)
                    if (depth >= 3 && key[depth-2] == "depots" && key[depth] == "manifests") man[key[depth-1]] = 1
                    next }
        $0 == "}" { if (depth > 0) depth--; next }
        match($0, /^"[^"]*"[ \t]+"[^"]*"$/) {
            split($0, q, "\""); k = q[2]; v = q[4]
            # a value directly inside depots/<id>
            if (depth >= 2 && key[depth-1] == "depots" && key[depth] ~ /^[0-9]+$/) {
                if (k == "dlcappid") dlc[key[depth]] = v
            }
            # depots/<id>/config/oslist
            if (depth >= 3 && key[depth-2] == "depots" && key[depth] == "config" && k == "oslist") os[key[depth-1]] = v
            pend = ""
        }
        END { for (d in dlc) if ((d in man) && (!(d in os) || os[d] ~ /windows/)) print d, dlc[d] }' | sort -n
}

check_unpinned() { # <dump file>
    [ -s "$1" ] || { warn "no app_info dump: other DLC depots were not checked"; return 0; }
    found=$(parse_dlc_depots "$1")
    [ -n "$found" ] || { warn "no DLC depots found in the app_info dump (format changed, or the app info was not printed); other DLCs were not checked"; return 0; }
    echo "$found" | while read -r d dlc; do
        echo "$DEPOTS" | cut -d'|' -f1 | grep -qx "$d" && continue
        warn "DLC depot $d (dlcappid $dlc) exists but is not in game-pin.txt: NOT fetched. Add a line with the manifest of the build matching the pinned exe."
    done
}

# The folder SteamCMD reported for one depot in this session's output:
#   Depot download complete : "<path>" (manifest <id>)
# SteamCMD with the platform forced to windows prints the part after its own folder with
# backslashes (".../MacOS\steamapps\content\app_250900\depot_250902"); the real path uses "/".
# Dies when SteamCMD reports a different manifest than the pin.
reported_depot() { # <depot id> <pinned manifest>
    [ -n "${DUMP:-}" ] && [ -s "$DUMP" ] || return 0
    line=$(tr -d '\r' < "$DUMP" | grep -E "Depot download complete : \".*[\\\\/]depot_$1\"" | tail -n 1) || true
    [ -n "$line" ] || return 0
    rman=$(printf '%s\n' "$line" | sed -n 's/.*(manifest \([0-9]*\)).*/\1/p')
    [ -z "$rman" ] || [ "$rman" = "$2" ] || die "SteamCMD fetched depot $1 at manifest $rman, the pin wants $2"
    printf '%s\n' "$line" | sed 's/^[^"]*"\(.*\)".*$/\1/' | tr '\\' '/'
}

# Find the folder of one depot (--content-dir / --merge-from, what SteamCMD reported, then SteamCMD's
# usual places, following a wrapper or symlink such as Homebrew's steamcmd).
find_depot() { # <depot id> <pinned manifest>
    id=$1
    scbin=$(command -v "$STEAMCMD" 2>/dev/null || echo /nonexistent/x)
    scdir=$(dirname "$scbin")
    screal=$(cd "$scdir" 2>/dev/null && dirname "$(pwd -P)/$(readlink "$(basename "$scbin")" 2>/dev/null || basename "$scbin")") || screal=""
    rep=$(reported_depot "$id" "$2") || exit 1
    if [ -n "$rep" ] && [ -d "$rep" ] && [ -n "$(ls -A "$rep" 2>/dev/null)" ]; then echo "$rep"; return 0; fi
    for base in "$CONTENT" "$MERGE_FROM" "$scdir" "$scdir/linux32" "$screal" \
        "$HOME/Library/Application Support/Steam" "$HOME/.steam/steam" "$HOME/.local/share/Steam" "$HOME/Steam"; do
        [ -n "$base" ] || continue
        for c in "$base/depot_$id" "$base/app_$APP/depot_$id" "$base/steamapps/content/app_$APP/depot_$id" \
                 "$base/linux32/steamapps/content/app_$APP/depot_$id"; do
            [ -d "$c" ] && [ -n "$(ls -A "$c" 2>/dev/null)" ] && { echo "$c"; return 0; }
        done
    done
    return 0
}

DUMP=""
if [ -z "$MERGE_FROM" ]; then
    command -v "$STEAMCMD" >/dev/null 2>&1 || [ -x "$STEAMCMD" ] || die "steamcmd not found (install it, or pass --steamcmd <path>; or use --merge-from)"
    [ -n "$USER_NAME" ] || die "--user <steam login> is required"
    say "pinned game $VERSION (Steam build $BUILD), app $APP"
    say "SteamCMD will ask for your password and Steam Guard code here; type them yourself."
    set -- +@sSteamCmdForcePlatformType windows +login "$USER_NAME"
    for dm in $(echo "$DEPOTS" | cut -d'|' -f1,2); do
        set -- "$@" +download_depot "$APP" "${dm%%|*}" "${dm##*|}"
    done
    set -- "$@" +app_info_update 1 +app_info_print "$APP" +quit
    # `script` records the session and keeps the terminal, so the prompts still work.
    DUMP=$(mktemp "${TMPDIR:-/tmp}/fetch-game-appinfo.XXXXXX")
    trap 'rm -f "$DUMP"' EXIT
    if command -v script >/dev/null 2>&1; then
        if [ "$(uname)" = Darwin ]; then script -q "$DUMP" "$STEAMCMD" "$@" || true
        else script -q -c "$(printf '%q ' "$STEAMCMD" "$@")" "$DUMP" || true; fi
    else
        "$STEAMCMD" "$@" || true
        : > "$DUMP"
    fi
    INFO_FILE=$DUMP
fi
[ -z "$INFO_FILE" ] || check_unpinned "$INFO_FILE"

# Report every depot; stop if a required one is missing. Nothing is moved before this passes.
LIST=""; missing=""
while IFS='|' read -r id man dlc req name; do
    [ -n "$id" ] || continue
    p=$(find_depot "$id" "$man") || exit 1
    if [ -n "$p" ]; then
        say "depot $id ($name): fetched"
        LIST="$LIST$id|$p
"
    elif [ "$req" = 1 ]; then
        say "depot $id ($name): NOT FOUND (not owned, or the download failed)"
        if [ "$dlc" = "-" ]; then missing="$missing  - $name (the base game, app $APP)
"
        else missing="$missing  - $name (DLC, store app id $dlc: https://store.steampowered.com/app/$dlc)
"; fi
    else
        say "depot $id ($name): not found, optional, skipped"
    fi
done <<EOF
$DEPOTS
EOF
[ -z "$missing" ] || die "required depots are missing, the game cannot be built without them:
$missing  Own them on the account you log in with, then run this again."

mkdir -p "$OUT"
mode=--remove-source-files; [ "$KEEP" = 1 ] && mode=""
printf '%s' "$LIST" | while IFS='|' read -r id p; do
    [ -n "$id" ] || continue
    say "depot $id: merging $(du -sh "$p" 2>/dev/null | cut -f1)"
    # --ignore-times: a later depot always overwrites, even a file of the same size and time
    # shellcheck disable=SC2086
    rsync -a --ignore-times $mode "$p/" "$OUT/"
    if [ "$KEEP" != 1 ]; then find "$p" -depth -type d -empty -delete 2>/dev/null || true; fi
done

[ -f "$OUT/isaac-ng.exe" ] || die "no isaac-ng.exe in $OUT after the merge"
got=$(shasum -a 256 "$OUT/isaac-ng.exe" | cut -d' ' -f1)
if [ "$got" = "$WANT_SHA" ]; then
    say "isaac-ng.exe matches the pin ($VERSION, build $BUILD): $OUT is ready for build.sh --game"
else
    die "isaac-ng.exe is $got, the pin wants $WANT_SHA ($VERSION, build $BUILD). The depots are not the pinned build."
fi
