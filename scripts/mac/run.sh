#!/bin/bash
# Run the dev game (output/mac/isaac-mac) against a support folder, with a fresh Layer snapshot.
#
#   scripts/mac/run.sh <support dir> <log file>
#
# The support folder holds mods/, saves/ and logs/ like the Launcher's (never point it at the real
# ~/Library/Application Support/Repentance+ Mac). The Layer is staged into <support dir>/.layer at
# every run, so Layer edits are picked up and a running game is never changed under it.
#
# Environment
#   FRAMES=N       stop after N presented frames (default 1500; ISAAC_MAC_RUN_FRAMES)
#   SCRIPT=...     scripted input (ISAAC_MAC_SCRIPT; grammar in native/mac/host/mac_script.c),
#                  e.g. SCRIPT='until=Room 1.2(Start Room):enter' reaches the first room
#   SHOT=F:png,..  screenshots at presented frames (ISAAC_MAC_SHOT; absolute png paths)
#   LAB=name.lua   a lab from scripts/recomp/rgon/ (ISAAC_RGON_LAB), e.g. lab_trapdoor.lua
#   HEADLESS=1     no window (the fake GL answers)
#   VAN=true       Vanilla Launch (Layer off);  DBG=true  debug console;  LUADBG=true  --luadebug (the game's own switch; mods' require of their own files then fails, so most mods stop at their main.lua)
#   FRESH=1        move gamestate1.dat aside first (scripts/mac/fresh.sh), so Enter starts a New Run
#   ALARM=s        kill the run after s seconds (default 600)
#   BUNDLE=dir     the game bundle (default .scratch/game-bundle, where build.sh's bundle stage writes)
#   ISAAC_BIN=path the binary to run (default output/mac/isaac-mac of the main checkout; a debug build goes elsewhere)
#
# From a git worktree: the Layer and the lab come from the worktree, the binary, lift segments and bundle from
# the main checkout.
#
# One heavy job at a time: refuses to start while any isaac-mac runs (the user may be playing).
set -u
S="${1:?usage: run.sh <support dir> <log file>}"; LOG="${2:?usage: run.sh <support dir> <log file>}"
R="$(cd "$(dirname "$0")/../.." && pwd)"
# In a git worktree R has the tracked files (the Layer is staged from here); the untracked shared inputs
# (output/, .scratch/game-bundle) live in the main checkout M. In the main checkout M = R.
M="$(git -C "$R" rev-parse --path-format=absolute --git-common-dir 2>/dev/null)"; M="${M%/.git}"; [ -d "$M/output" ] || M="$R"
if pgrep -f "MacOS/isaac-mac|output/mac/isaac-mac" >/dev/null; then
  echo "run.sh: a game is running (pgrep -f isaac-mac); wait for it to quit" >&2; exit 9
fi
mkdir -p "$S/logs" "$S/mods" "$S/saves"
S="$(cd "$S" && pwd)"
LOG="$(cd "$(dirname "$LOG")" && pwd)/$(basename "$LOG")"
BUNDLE="${BUNDLE:-$M/.scratch/game-bundle}"
[ "${FRESH:-0}" = 1 ] && bash "$R/scripts/mac/fresh.sh" "$S"
rm -rf "$S/.layer"
python3 "$R/scripts/mac/stage_layer.py" "$S/.layer" >/dev/null || exit 1
if [ -n "${LAB:-}" ]; then
  cp "$R/scripts/recomp/rgon/$LAB" "$S/.layer/" || exit 1
fi
cat > "$S/launch-settings.json" <<JSON
{ "version": 1, "vanilla_launch": ${VAN:-false}, "debug_console": ${DBG:-false}, "log_window": false, "lua_debug": ${LUADBG:-false},
  "support_dir": "$S", "mods_dir": "$S/mods", "save_dir": "$S/saves",
  "gate_log_path": "$S/logs/layer-gate.json", "game_log_path": "$S/logs/game.log", "modscan_path": "$S/modscan.lua",
  "layer_dir": "$S/.layer", "segs_path": "$M/output/recomp/host/isaac.segs.bin", "bundle_dir": "$BUNDLE" }
JSON
ENV=(ISAAC_MAC_RUN_FRAMES="${FRAMES:-1500}" REPENTANCE_MAC_LAUNCH_SETTINGS="$S/launch-settings.json")
[ -n "${SCRIPT:-}" ] && ENV+=("ISAAC_MAC_SCRIPT=$SCRIPT")
[ -n "${SHOT:-}" ] && ENV+=("ISAAC_MAC_SHOT=$SHOT")
[ -n "${LAB:-}" ] && ENV+=("ISAAC_RGON_LAB=$LAB")
[ "${HEADLESS:-0}" = 1 ] && ENV+=("ISAAC_MAC_HEADLESS=1")
cd "$R" && perl -e 'alarm shift; exec @ARGV' "${ALARM:-600}" env "${ENV[@]}" \
  "${ISAAC_BIN:-$M/output/mac/isaac-mac}" --launch-settings "$S/launch-settings.json" > "$LOG" 2>&1
rc=$?
echo "rc=$rc" >> "$LOG"
echo "run.sh: rc=$rc, log $LOG"
exit $rc
