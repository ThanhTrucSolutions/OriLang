#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CC=${CC:-cc}

"$CC" -O2 -o "$ROOT/core/orivm" "$ROOT/core/orivm.c" -lm
echo "[ori] built $ROOT/core/orivm"
