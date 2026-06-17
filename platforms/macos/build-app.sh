#!/usr/bin/env sh
set -eu

ORB="$1"
OUT_APP="$2"
APP_NAME="${3:-Ori}"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

rm -rf "$OUT_APP"
mkdir -p "$OUT_APP/Contents/MacOS" "$OUT_APP/Contents/Resources"

INFO="$OUT_APP/Contents/Info.plist"
sed "s/__APP_NAME__/$APP_NAME/g" "$SCRIPT_DIR/Info.plist" > "$INFO"
cp "$ORB" "$OUT_APP/Contents/Resources/app.orb"

cc -fobjc-arc -O2 \
  "$SCRIPT_DIR/orimac.m" \
  -framework Cocoa \
  -o "$OUT_APP/Contents/MacOS/$APP_NAME"

chmod +x "$OUT_APP/Contents/MacOS/$APP_NAME"
echo "[macos] built $OUT_APP"
