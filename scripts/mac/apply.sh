#!/bin/sh
# Lay the Mac Build over a checkout of chiikabu/the-browsing-of-isaac.
#
#   scripts/mac/apply.sh /path/to/the-browsing-of-isaac
#
# Copies this repo's Mac files into the checkout and applies the Host patch
# (patches/host-native.patch). The patch was made against upstream commit
# b986dd8; other commits may need it rebased.
set -eu

UPSTREAM_BASE=b986dd8
here=$(cd "$(dirname "$0")/../.." && pwd)
dst=${1:?usage: apply.sh /path/to/the-browsing-of-isaac}

git -C "$dst" rev-parse --git-dir >/dev/null 2>&1 || { echo "$dst is not a git checkout"; exit 1; }
head=$(git -C "$dst" rev-parse --short=7 HEAD)
if [ "$head" != "$UPSTREAM_BASE" ]; then
    echo "note: checkout is at $head, the patch was made against $UPSTREAM_BASE"
fi

git -C "$dst" apply --check "$here/patches/host-native.patch"
git -C "$dst" apply "$here/patches/host-native.patch"

mkdir -p "$dst/native/mac" "$dst/scripts/mac" "$dst/docs/adr" "$dst/docs/mac"
cp -R "$here/native/mac/." "$dst/native/mac/"
cp "$here/scripts/mac/build_native.py" "$dst/scripts/mac/"
cp -R "$here/docs/adr/." "$dst/docs/adr/"
cp -R "$here/docs/mac/." "$dst/docs/mac/"
cp "$here/GLOSSARY.md" "$dst/"

echo "applied. Next: follow the upstream docs/DEVELOPING.md build through"
echo "build_boot.py, then: python scripts/mac/build_native.py"
