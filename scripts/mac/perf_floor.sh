#!/bin/bash
# Windowed perf probe of one floor (the recipe is RUNBOOK section 12). Run from the main checkout.
#   scripts/mac/perf_floor.sh run   <name> "<console cmds, | separated>" <frames>   frame stats of a forced floor
#   scripts/mac/perf_floor.sh rooms <name> "<stage cmd>" <frames>                    per-room walk (lab_rooms.lua)
#   scripts/mac/perf_floor.sh sample <name> "<stage cmd>"                            run + `sample` of the main thread
# SUP=<support dir> (default .scratch/orch/support-95, the 95-mod folder), VAN=true for Vanilla Launch,
# ISAAC_BIN=<binary> to test a build elsewhere. Output: .scratch/perf/<name>.log (+ .sample). Needs the heavy lock
# (.scratch/orch/heavy.sh) and a closed game. Headless is useless here: the fake GL has no GPU wait and no frame stats.
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$R" || exit 1
MODE="${1:?run|rooms|sample}"; NAME="${2:?name}"; CMD="${3:?cmd}"; FRAMES="${4:-3200}"
SUP="${SUP:-$R/.scratch/orch/support-95}"; OUT="$R/.scratch/perf"; mkdir -p "$OUT"
LAB=lab_stage.lua; [ "$MODE" = rooms ] && LAB=lab_rooms.lua
go() {
  env HEADLESS=0 FRESH=1 FRAMES="$FRAMES" LAB="$LAB" DBG=true VAN="${VAN:-false}" ISAAC_MAC_STATS_EVERY=300 \
    ISAAC_MAC_CLOCK=wall ISAAC_LAB_CMD="$1" SCRIPT='until=Room 1.2(Start Room):enter' \
    "$R/.scratch/orch/heavy.sh" perf "$R/scripts/mac/run.sh" "$SUP" "$OUT/$NAME.log"
}
case "$MODE" in
  run|rooms) go "$CMD" ;;
  sample)
    SP="spawn 14.0|spawn 14.0|spawn 14.0|spawn 14.0|spawn 14.0|spawn 14.0|spawn 14.0|spawn 14.0"
    FRAMES=3200; ( go "$CMD|$SP" >/dev/null 2>&1 ) &
    until [ "$(grep -ac 'exec spawn' "$OUT/$NAME.log" 2>/dev/null)" -ge 6 ]; do sleep 2; done
    sleep 4; sample "$(pgrep -x isaac-mac)" 10 -file "$OUT/$NAME.sample" >/dev/null 2>&1; wait ;;
esac
python3 "$R/scripts/mac/framestats.py" "$OUT/$NAME.log" | tail -25
