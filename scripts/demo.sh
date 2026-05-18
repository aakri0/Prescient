#!/usr/bin/env bash
# demo.sh — narrated five-act demo of the Prescient pipeline.
# Invokable via ./run.sh demo.  Fully non-interactive; designed for
# screen recording.  Total runtime: < 2 minutes on a warm build.
#
# Acts:
#   1. The Problem        — measure raw O2 cost of a pathological function
#   2. Feature Extraction — contrast a simple and a complex function
#   3. Prediction         — show tier + microsecond estimates
#   4. Adaptive Pipeline  — measure actual compile savings
#   5. Honest Evaluation  — display the documented test07 failure
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

PLUGIN="build/IRComplexityEstimator.so"
WORK="output/demo"
SIMPLE="testcases/training/t01_simple_leaf.c"
COMPLEX="testcases/training/t10_pathological.c"
FAILURE="testcases/evaluation/test07_failure_case.c"

# --- colour palette -------------------------------------------------------
# tput respects $TERM=dumb (CI smoke-test) — it just emits empty strings.
GREEN="$(tput setaf 2 2>/dev/null || true)"
RED="$(tput setaf 1 2>/dev/null || true)"
YELLOW="$(tput setaf 3 2>/dev/null || true)"
CYAN="$(tput setaf 6 2>/dev/null || true)"
BOLD="$(tput bold 2>/dev/null || true)"
RESET="$(tput sgr0 2>/dev/null || true)"

PAUSE="${DEMO_PAUSE:-1}"   # seconds between acts; override with DEMO_PAUSE=0

act() {
    echo
    echo "${BOLD}${CYAN}=== Act $1: $2 ===${RESET}"
    echo
}

say() { echo "$*"; }

fail() {
    echo "${RED}error${RESET}: $*" >&2
    exit 1
}

pause() { sleep "$PAUSE"; }

# --- preconditions --------------------------------------------------------
[[ -f "$PLUGIN" ]] || fail "$PLUGIN missing — run ./build.sh first"
[[ -f "models/training_metadata.json" ]] \
    || fail "no trained model in models/ — run training first"

CLANG="$(command -v clang-17 || command -v clang || true)"
OPT="$(command -v opt-17 || command -v opt || true)"
[[ -n "$CLANG" && -n "$OPT" ]] \
    || fail "clang-17 / opt-17 not found — run scripts/setup_env.sh"

mkdir -p "$WORK"

# ==========================================================================
# Act 1 — The Problem
# ==========================================================================
act 1 "The Problem"
say "Compiling ${BOLD}${COMPLEX}${RESET} through the full O2 pipeline,"
say "measuring how long the optimisation passes take."
pause

LL="$WORK/act1.ll"
CSV="$WORK/act1.csv"
"$CLANG" -O0 -Xclang -disable-O0-optnone -emit-llvm -S "$COMPLEX" -o "$LL" \
    || fail "clang failed on $COMPLEX"

START_NS=$(date +%s%N)
"$OPT" -load-pass-plugin "./$PLUGIN" -passes="default<O2>" \
       -timing-output="$CSV" -disable-output "$LL" 2>/dev/null \
    || fail "opt failed running O2"
END_NS=$(date +%s%N)
WALL_MS=$(( (END_NS - START_NS) / 1000000 ))
TOTAL_US=$(awk -F, 'NR>1 { gsub(/"/,"",$3); s+=$3 } END { printf "%d", s }' "$CSV")

say
say "Wall time : ${RED}${WALL_MS} ms${RESET}"
say "Per-pass total: ${RED}${TOTAL_US} µs${RESET} across the timed function-level passes"
say
say "${YELLOW}Question:${RESET} can we predict that cost ${BOLD}BEFORE${RESET} the pipeline runs?"
pause

# ==========================================================================
# Act 2 — Feature Extraction
# ==========================================================================
act 2 "Feature Extraction"
say "Two contrasting functions, one extractor pass, twelve features each."
pause

LL_S="$WORK/act2_simple.ll"
LL_C="$WORK/act2_complex.ll"
F_S="$WORK/act2_simple.json"
F_C="$WORK/act2_complex.json"

"$CLANG" -O0 -Xclang -disable-O0-optnone -emit-llvm -S "$SIMPLE"  -o "$LL_S"
"$CLANG" -O0 -Xclang -disable-O0-optnone -emit-llvm -S "$COMPLEX" -o "$LL_C"

"$OPT" -load-pass-plugin "./$PLUGIN" -passes="ir-complexity" \
       -complexity-output="$F_S" -disable-output "$LL_S" 2>/dev/null
"$OPT" -load-pass-plugin "./$PLUGIN" -passes="ir-complexity" \
       -complexity-output="$F_C" -disable-output "$LL_C" 2>/dev/null

extract() {
    python3 -c "
import json, sys
d = json.load(open(sys.argv[1]))[0]
keys = ['instruction_count','basic_block_count','cyclomatic_complexity',
        'max_loop_depth','loop_instruction_ratio','phi_density']
print(d['function_name'])
for k in keys:
    print(f'  {k:<28} {d.get(k, 0)}')
" "$1"
}

say
say "${GREEN}-- simple function --${RESET}"
extract "$F_S"
say
say "${RED}-- complex function --${RESET}"
extract "$F_C"
pause

# ==========================================================================
# Act 3 — Prediction
# ==========================================================================
act 3 "Prediction"
say "Feeding both feature sets through the trained model."
pause

P_S="$WORK/act3_simple.json"
P_C="$WORK/act3_complex.json"
python3 src/model/predict.py --features "$F_S" --models-dir models \
        --output "$P_S" >/dev/null 2>&1 || fail "predict failed (simple)"
python3 src/model/predict.py --features "$F_C" --models-dir models \
        --output "$P_C" >/dev/null 2>&1 || fail "predict failed (complex)"

show_prediction() {
    python3 -c "
import json, sys
p = json.load(open(sys.argv[1]))[0]
print(f\"  {p['function_name']}\")
print(f\"    tier:           {p['complexity_tier']}\")
print(f\"    predicted µs:   {p['predicted_total_us']}\")
print(f\"    confidence:     {p['confidence']}\")
print(f\"    rationale:      {p['tier_rationale']}\")
" "$1"
}

say
say "${GREEN}simple${RESET}"
show_prediction "$P_S"
say
say "${RED}complex${RESET}"
show_prediction "$P_C"
pause

# ==========================================================================
# Act 4 — Adaptive Pipeline in Action
# ==========================================================================
act 4 "Adaptive Pipeline in Action"
say "Same complex file: baseline full-O2 vs adaptive pipeline."
pause

LL_A="$WORK/act4.ll"
PRED_A="$WORK/act4_predictions.json"
CSV_BASE="$WORK/act4_baseline.csv"
CSV_ADAP="$WORK/act4_adaptive.csv"
BC_BASE="$WORK/act4_baseline.bc"
BC_ADAP="$WORK/act4_adaptive.bc"

"$CLANG" -O0 -Xclang -disable-O0-optnone -emit-llvm -S "$COMPLEX" -o "$LL_A"

F_A="$WORK/act4_features.json"
"$OPT" -load-pass-plugin "./$PLUGIN" -passes="ir-complexity" \
       -complexity-output="$F_A" -disable-output "$LL_A" 2>/dev/null
python3 src/model/predict.py --features "$F_A" --models-dir models \
        --output "$PRED_A" >/dev/null 2>&1 || fail "predict failed (act4)"

B_START=$(date +%s%N)
"$OPT" -load-pass-plugin "./$PLUGIN" -passes="default<O2>" \
       -timing-output="$CSV_BASE" "$LL_A" -o "$BC_BASE" 2>/dev/null \
    || fail "baseline O2 failed"
B_END=$(date +%s%N)
BASE_MS=$(( (B_END - B_START) / 1000000 ))

A_START=$(date +%s%N)
COMPLEXITY_PREDICTIONS="$PRED_A" \
    "$OPT" -load-pass-plugin "./$PLUGIN" -passes="adaptive-pipeline" \
           -timing-output="$CSV_ADAP" "$LL_A" -o "$BC_ADAP" 2>/dev/null \
    || fail "adaptive run failed"
A_END=$(date +%s%N)
ADAP_MS=$(( (A_END - A_START) / 1000000 ))

SAVED_MS=$(( BASE_MS - ADAP_MS ))
if [[ "$BASE_MS" -gt 0 ]]; then
    SAVED_PCT=$(awk -v b="$BASE_MS" -v s="$SAVED_MS" 'BEGIN{printf "%.1f", (s/b)*100}')
else
    SAVED_PCT="0.0"
fi

say
say "  baseline wall time : ${RED}${BASE_MS} ms${RESET}"
say "  adaptive wall time : ${GREEN}${ADAP_MS} ms${RESET}"
say "  ${BOLD}saved              : ${SAVED_MS} ms (${SAVED_PCT}%)${RESET}"
pause

# ==========================================================================
# Act 5 — Honest Evaluation
# ==========================================================================
act 5 "Honest Evaluation"
say "${YELLOW}Not every prediction is good.${RESET}"
say "${BOLD}${FAILURE}${RESET} — a documented failure case."
pause

LL_F="$WORK/act5.ll"
F_F="$WORK/act5_features.json"
P_F="$WORK/act5_predictions.json"
CSV_F="$WORK/act5_timings.csv"

"$CLANG" -O0 -Xclang -disable-O0-optnone -emit-llvm -S "$FAILURE" -o "$LL_F"
"$OPT" -load-pass-plugin "./$PLUGIN" -passes="ir-complexity" \
       -complexity-output="$F_F" -disable-output "$LL_F" 2>/dev/null
python3 src/model/predict.py --features "$F_F" --models-dir models \
        --output "$P_F" >/dev/null 2>&1 || fail "predict failed (act5)"

"$OPT" -load-pass-plugin "./$PLUGIN" -passes="default<O2>" \
       -timing-output="$CSV_F" -disable-output "$LL_F" 2>/dev/null \
    || fail "opt failed (act5)"

PRED_US=$(python3 -c "import json,sys; p=json.load(open(sys.argv[1]))[0]; print(p['predicted_total_us'])" "$P_F")
ACT_US=$(awk -F, 'NR>1 { gsub(/"/,"",$3); s+=$3 } END { printf "%d", s }' "$CSV_F")

say
say "  function       : ${YELLOW}misleading_complex${RESET}"
say "  predicted µs   : ${RED}${PRED_US}${RESET}   (model says: expensive HIGH tier)"
say "  actual µs      : ${GREEN}${ACT_US}${RESET}   (reality: fast — InstCombine + SCCP fold it away)"
say
say "Why: the loop body is loop-invariant constants. The static extractor cannot"
say "see that the optimiser will fold the work away — and we say so out loud."
say "See ${CYAN}testcases/evaluation/test07_analysis.md${RESET} for the full analysis."
echo

echo "${BOLD}${GREEN}demo complete${RESET}"
