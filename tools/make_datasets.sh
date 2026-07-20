#!/usr/bin/env bash
# Regenerate the standard benchmark datasets. Deterministic (fixed seed).
#
#   tools/make_datasets.sh [gen-binary] [out-dir] [count]
#
# Defaults assume an existing build: ./build/gen, writing to ./datasets.
set -euo pipefail

GEN=${1:-./build/gen}
OUT=${2:-datasets}
COUNT=${3:-1000000}
SEED=42

mkdir -p "$OUT"
for scenario in mixed cross cancel wide deep fuzz; do
    "$GEN" "$scenario" "$COUNT" "$SEED" > "$OUT/$scenario.in"
    echo "wrote $OUT/$scenario.in ($COUNT messages)"
done
