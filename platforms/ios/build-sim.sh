#!/usr/bin/env sh
set -eu

ORB="$1"
OUT_APP="$2"
APP_NAME="${3:-Ori}"
BUNDLE_ID="${4:-ori.app}"
MODE="${5:---build-only}"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SDK=$(xcrun --sdk iphonesimulator --show-sdk-path)
MIN_IOS="${ORI_IOS_MIN:-13.0}"

rm -rf "$OUT_APP"
mkdir -p "$OUT_APP"

sed "s/__APP_NAME__/$APP_NAME/g; s/__BUNDLE_ID__/$BUNDLE_ID/g" "$SCRIPT_DIR/Info.plist" > "$OUT_APP/Info.plist"
cp "$ORB" "$OUT_APP/app.orb"

xcrun clang -fobjc-arc -O2 \
  -isysroot "$SDK" \
  -mios-simulator-version-min="$MIN_IOS" \
  "$SCRIPT_DIR/oriios.m" \
  -framework UIKit -framework Foundation \
  -o "$OUT_APP/$APP_NAME"

codesign -s - --force --deep "$OUT_APP" >/dev/null 2>&1 || true
echo "[ios] built $OUT_APP"

if [ "$MODE" = "--launch" ]; then
  UDID=$(xcrun simctl list devices booted | awk -F '[()]' '/Booted/ {print $2; exit}')
  if [ -z "$UDID" ]; then
    UDID=$(xcrun simctl list devices available | awk -F '[()]' '/iPhone/ && /Shutdown/ {print $2; exit}')
    if [ -z "$UDID" ]; then
      echo "[ios] no available iPhone simulator found; built app only"
      exit 0
    fi
    xcrun simctl boot "$UDID" >/dev/null 2>&1 || true
    xcrun simctl bootstatus "$UDID" -b
  fi
  xcrun simctl install "$UDID" "$OUT_APP"
  xcrun simctl launch "$UDID" "$BUNDLE_ID"
  echo "[ios] launched $BUNDLE_ID on $UDID"
fi
