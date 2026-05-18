#!/usr/bin/env bash
# run_adaptive.sh — end-to-end adaptive pipeline workflow for one C file
# (issue #23). Produces baseline and adaptive timings, optimised binaries,
# a correctness comparison and a savings report.
#
# Usage:
#   ./scripts/run_adaptive.sh <input.c>
#
# If a test harness exists alongside the input as <input>_test.c it is used
# to build runnable binaries; otherwise a trivial stub main is synthesised
# so the binaries still link and run.
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

PLUGIN="build/IRComplexityEstimator.so"
OUT="output"

if [[ -t 1 ]]; then
  RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
else
  RED=''; GREEN=''; YELLOW=''; NC=''
fi
die()  { echo -e "${RED}error${NC}: $*" >&2; exit 1; }
step() { echo -e "${YELLOW}==>${NC} $*"; }

# --- arguments and preconditions -----------------------------------------
[[ $# -eq 1 ]] || die "usage: $(basename "$0") <input.c>"
INPUT="$1"
[[ -f "$INPUT" ]] || die "input file does not exist: $INPUT"
[[ "$INPUT" == *.c ]] || die "input must be a .c file: $INPUT"

[[ -f "$PLUGIN" ]] || die "plugin not found: $PLUGIN — run ./build.sh first"
[[ -f "models/training_metadata.json" ]] \
  || die "no trained model found — run the training pipeline first"

CLANG="$(command -v clang-17 || command -v clang || true)"
OPT="$(command -v opt-17 || command -v opt || true)"
LLC="$(command -v llc-17 || command -v llc || true)"
[[ -n "$CLANG" && -n "$OPT" && -n "$LLC" ]] \
  || die "clang-17/opt-17/llc-17 not found — run scripts/setup_env.sh"

mkdir -p "$OUT"
NAME="$(basename "${INPUT%.c}")"
LL="$OUT/$NAME.ll"

# --- 1. compile to unoptimised IR (optnone disabled so O2 can run) -------
step "1/8 compiling $INPUT to IR"
"$CLANG" -O0 -Xclang -disable-O0-optnone -emit-llvm -S "$INPUT" -o "$LL" \
  || die "clang failed to emit IR"

# --- 2. extract IR-complexity features -----------------------------------
step "2/8 extracting features"
"$OPT" -load-pass-plugin "$PLUGIN" -passes="ir-complexity" \
  -complexity-output="$OUT/features.json" -disable-output "$LL" \
  || die "feature extraction failed"

# --- 3. generate predictions ---------------------------------------------
step "3/8 predicting compile times"
python3 src/model/predict.py --features "$OUT/features.json" \
  --models-dir models --output "$OUT/predictions.json" \
  || die "prediction failed"

# --- 4. baseline run: full O2, timed -------------------------------------
step "4/8 baseline O2 run"
B_START=$(date +%s%N)
"$OPT" -load-pass-plugin "$PLUGIN" -passes="default<O2>" \
  -timing-output="$OUT/baseline_timings.csv" "$LL" -o "$OUT/baseline.bc" \
  || die "baseline O2 run failed"
B_END=$(date +%s%N)
BASELINE_MS=$(( (B_END - B_START) / 1000000 ))

# --- 5. adaptive run: per-function pipeline, timed -----------------------
step "5/8 adaptive pipeline run"
A_START=$(date +%s%N)
COMPLEXITY_PREDICTIONS="$OUT/predictions.json" \
  "$OPT" -load-pass-plugin "$PLUGIN" -passes="adaptive-pipeline" \
  -timing-output="$OUT/adaptive_timings.csv" "$LL" -o "$OUT/adaptive.bc" \
  || die "adaptive run failed"
A_END=$(date +%s%N)
ADAPTIVE_MS=$(( (A_END - A_START) / 1000000 ))

# --- 6. compile both optimised IR files to binaries ----------------------
step "6/8 building binaries (llc + clang)"
"$LLC" "$OUT/baseline.bc" -o "$OUT/baseline.s" || die "llc failed (baseline)"
"$LLC" "$OUT/adaptive.bc" -o "$OUT/adaptive.s" || die "llc failed (adaptive)"

HARNESS="${INPUT%.c}_test.c"
if [[ -f "$HARNESS" ]]; then
  HARNESS_SRC="$HARNESS"
  HAVE_HARNESS=1
else
  # No harness: synthesise a stub main so the binaries still link and run.
  HARNESS_SRC="$OUT/${NAME}_stub_main.c"
  printf 'int main(void) { return 0; }\n' > "$HARNESS_SRC"
  HAVE_HARNESS=0
fi
"$CLANG" "$OUT/baseline.s" "$HARNESS_SRC" -o "$OUT/output_baseline" \
  || die "failed to link output_baseline"
"$CLANG" "$OUT/adaptive.s" "$HARNESS_SRC" -o "$OUT/output_adaptive" \
  || die "failed to link output_adaptive"

# --- 7. correctness comparison -------------------------------------------
step "7/8 correctness check"
"$OUT/output_baseline" > "$OUT/baseline.out" 2>&1; BASE_RC=$?
"$OUT/output_adaptive" > "$OUT/adaptive.out" 2>&1; ADAP_RC=$?
if diff -q "$OUT/baseline.out" "$OUT/adaptive.out" >/dev/null 2>&1 \
   && [[ "$BASE_RC" -eq "$ADAP_RC" ]]; then
  CORRECTNESS="PASS"
else
  CORRECTNESS="FAIL"
fi

# --- 8. savings report ----------------------------------------------------
step "8/8 generating savings report"
python3 scripts/generate_savings_report.py \
  --baseline "$OUT/baseline_timings.csv" \
  --adaptive "$OUT/adaptive_timings.csv" \
  --predictions "$OUT/predictions.json" \
  --output "$OUT/savings_report.md" \
  || die "savings report generation failed"

# --- final summary --------------------------------------------------------
SAVED_MS=$(( BASELINE_MS - ADAPTIVE_MS ))
echo
echo "============ Adaptive Pipeline Summary ============"
echo "  input             : $INPUT"
echo "  baseline wall time : ${BASELINE_MS} ms"
echo "  adaptive wall time : ${ADAPTIVE_MS} ms"
echo "  wall time saved    : ${SAVED_MS} ms"
echo "  savings report     : $OUT/savings_report.md"
if [[ "$HAVE_HARNESS" -eq 1 ]]; then
  echo "  correctness        : $CORRECTNESS (test harness: $HARNESS)"
else
  echo "  correctness        : $CORRECTNESS (no test harness — stub main)"
fi
echo "==================================================="

if [[ "$CORRECTNESS" == "PASS" ]]; then
  echo -e "${GREEN}adaptive run OK${NC}"
  exit 0
fi
echo -e "${RED}correctness check FAILED — baseline and adaptive differ${NC}" >&2
exit 1
