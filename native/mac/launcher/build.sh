#!/bin/bash
# Builds the Launcher into <out dir>/Launcher (a bare executable) and, if
# --app is given, wraps it as <out dir>/Repentance+.app/Contents/MacOS/Repentance+
# Usage: build.sh <out dir> [--app]
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:?usage: build.sh <out dir> [--app]}"
mkdir -p "$out"
swiftc -O -swift-version 5 -target arm64-apple-macos13.0 \
  -parse-as-library -o "$out/Launcher" "$here"/Sources/*.swift
if [ "${2:-}" = "--app" ]; then
  app="$out/Repentance+.app/Contents"
  mkdir -p "$app/MacOS" "$app/Resources"
  cp "$out/Launcher" "$app/MacOS/Repentance+"
  cp "$here/Info.plist" "$app/Info.plist"
  codesign --force --sign - "$out/Repentance+.app" >/dev/null 2>&1 || true
fi
echo "built $out/Launcher"
