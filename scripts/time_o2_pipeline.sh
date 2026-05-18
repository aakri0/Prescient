#!/usr/bin/env bash
# time_o2_pipeline.sh — run the full -O2 optimisation pipeline through opt
# with the timing plugin loaded, capturing per-pass timings for every
# function. This is the primary data-collection tool for the training set.
#
# Usage:
#   ./scripts/time_o2_pipeline.sh <input.ll> <output_timing.csv>
#
# The plugin path defaults to ./build/IRComplexityEstimator.so and can be
# overridden with the PLUGIN environment variable.
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLUGIN="${PLUGIN:-$ROOT_DIR/build/IRComplexityEstimator.so}"

if [[ -t 2 ]]; then
  RED='\033[0;31m'; GREEN='\033[0;32m'; NC='\033[0m'
else
  RED=''; GREEN=''; NC=''
fi
die() { echo -e "${RED}error${NC}: $*" >&2; exit 1; }

# --- arguments ------------------------------------------------------------
[[ $# -eq 2 ]] || die "usage: $(basename "$0") <input.ll> <output_timing.csv>"
INPUT_LL="$1"
OUTPUT_CSV="$2"

[[ -f "$INPUT_LL" && -r "$INPUT_LL" ]] \
  || die "input IR file does not exist or is not readable: $INPUT_LL"
[[ -f "$PLUGIN" ]] || die "plugin not found: $PLUGIN (run ./build.sh first)"

OPT_BIN="$(command -v opt-17 || command -v opt || true)"
[[ -n "$OPT_BIN" ]] || die "opt not found — run scripts/setup_env.sh"

# --- run the O2 pipeline under the timing plugin --------------------------
# -passes="default<O2>" runs the complete standard O2 pipeline. Some of its
# passes run at module or loop level; those produce no per-function timing
# rows by design (the wrapper only records function-level passes).
# -disable-output skips writing the optimised IR — only timing data matters.
START_NS=$(date +%s%N)
"$OPT_BIN" \
  -load-pass-plugin "$PLUGIN" \
  -passes="default<O2>" \
  -timing-output="$OUTPUT_CSV" \
  -disable-output \
  "$INPUT_LL" \
  || die "opt failed while running the O2 pipeline on $INPUT_LL"
END_NS=$(date +%s%N)

# --- verify and summarise -------------------------------------------------
[[ -f "$OUTPUT_CSV" ]] || die "no timing CSV produced at $OUTPUT_CSV"

ROWS=$(($(wc -l < "$OUTPUT_CSV") - 1))   # minus the header line
[[ "$ROWS" -gt 0 ]] || die "timing CSV $OUTPUT_CSV contains no data rows"

FUNCS=$(tail -n +2 "$OUTPUT_CSV" | cut -d, -f1 | sort -u | wc -l | tr -d ' ')
PASSES=$(tail -n +2 "$OUTPUT_CSV" | cut -d, -f2 | sort -u | wc -l | tr -d ' ')
ELAPSED_MS=$(( (END_NS - START_NS) / 1000000 ))

echo -e "${GREEN}O2 timing complete${NC}: $OUTPUT_CSV"
echo "  rows recorded   : $ROWS"
echo "  functions timed : $FUNCS"
echo "  distinct passes : $PASSES"
echo "  wall time       : ${ELAPSED_MS} ms"
