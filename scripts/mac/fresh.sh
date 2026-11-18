#!/bin/bash
# fresh.sh <support dir>: move the continue-run state aside, so Enter on the save file starts a
# New Run instead of Continue. Moves gamestate1.dat to gamestate1.dat.old-<time>; deletes nothing.
set -u
d="${1:?usage: fresh.sh <support dir>}/saves/documents/my games/binding of isaac repentance+"
for f in gamestate1.dat; do
  [ -f "$d/$f" ] && mv "$d/$f" "$d/$f.old-$(date +%s)"
done
exit 0
