#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}

echo "[ori] building core/orivm ..."
"$CC" -O2 -o "$ROOT/core/orivm" "$ROOT/core/orivm.c" -lm

echo "[ori] compiling tools/ori.ori -> tools/ori.orb ..."
ORI_HOME="$ROOT" ORI_WIN="" "$ROOT/core/orivm" "$ROOT/tools/oric.orb" \
    "$ROOT/tools/ori.ori" "$ROOT/tools/ori.orb"

echo "[ori] building ori (thin C bootstrap) ..."
"$CC" -O2 -o "$ROOT/ori" "$ROOT/tools/ori.c"

echo "[ori] done. Try: ./ori doctor && ./ori run samples/console"
