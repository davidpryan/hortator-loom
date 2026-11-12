#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# The LOOM colour theme's check: tests/sim_cap.c drives the firmware's own UI to every screen it can reach in the
# simulator (the Bluetooth page and the frozen crash / update screens are not among them) and dumps each one; the
# screens of this checkout are compared with those of a reference commit (tools/theme_check.py).
#   tools/theme_check.sh [REF]      REF: the firmware without the colour theme (default: upstream 1.0.1, 44c9ad4)
# Output: build/theme/ (sheet-NN.png: every screen in LOOM).
set -euo pipefail
cd "$(dirname "$0")/.."
PY=${PYTHON:-python3}
REF=${1:-44c9ad4}
OUT=build/theme
rm -rf "$OUT"
mkdir -p "$OUT/gen" "$OUT/ref"
git archive "$REF" firmware tests VERSION.txt | tar -x -C "$OUT/ref"
PYTHONDONTWRITEBYTECODE=1 "$PY" -c 'import sys; sys.path.insert(0, "tools"); import build; from pathlib import Path; build.GEN = Path(sys.argv[1]).resolve(); build.generate()' "$OUT/gen" >/dev/null
awk '/static const int8_t FM1_KEYMAP/,/^};/' firmware/hal/fm1_input.h > "$OUT/gen/sim_keymap.h"
CF=(-O2 -w -I"$OUT/gen" -DFELUCCA_VERSION="\"DRUM-$(cat VERSION.txt)\"")
cc "${CF[@]}" -I"$OUT/ref/firmware/src" -I"$OUT/ref/tests" -o "$OUT/cap_ref" tests/sim_cap.c -lm
cc "${CF[@]}" -Ifirmware/src -Itests -o "$OUT/cap_new" tests/sim_cap.c -lm
ABS=$PWD/$OUT
run() { mkdir -p "$ABS/$3"; (cd "$1" && "$ABS/$2" "$ABS/$3" "$4" >/dev/null); }   # (run from the tree: felucca.c is read)
run "$OUT/ref" cap_ref ref_red 16
run "$OUT/ref" cap_ref ref_inv 17
run . cap_new new_red 16
run . cap_new new_inv 17
run . cap_new loom 22
"$PY" tools/theme_check.py "$OUT"
