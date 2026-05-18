#!/usr/bin/env bash
# run.sh — entry point for the Prescient pipeline.
# Modes: extract | train | predict | evaluate | adaptive | demo
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

PLUGIN="build/IRComplexityEstimator.so"
OUTPUT_DIR="output"
MODEL_DIR="src/model"
mkdir -p "$OUTPUT_DIR"

if [[ -t 1 ]]; then
  RED='\033[0;31m'; GREEN='\033[0;32m'; NC='\033[0m'
else
  RED=''; GREEN=''; NC=''
fi
err() { echo -e "${RED}error${NC}: $*" >&2; }

usage() {
  cat >&2 <<'EOF'
Usage: ./run.sh <mode> [args]

Modes:
  extract <file.c>   Extract IR features from a C source file -> output/features.json
  train              Train the model on testcases/training/
  predict <file.c>   Predict compile time for a C source file -> output/predictions.json
  evaluate           Run the full evaluation suite with metrics
  adaptive <file.c>  Run the adaptive pipeline and report savings
  demo               Run the end-to-end demo

All intermediate files are written to ./output/.
EOF
}

OPT_BIN="$(command -v opt-17 || command -v opt || true)"
CLANG_BIN="$(command -v clang-17 || command -v clang || true)"

require_plugin() {
  [[ -f "$PLUGIN" ]] || { err "$PLUGIN not found — run ./build.sh first"; exit 1; }
}

mode_extract() {
  local src="${1:-}"
  if [[ -z "$src" ]]; then err "extract requires a C source file"; usage; exit 1; fi
  [[ -f "$src" ]] || { err "no such file: $src"; exit 1; }
  [[ -n "$CLANG_BIN" ]] || { err "clang not found — run scripts/setup_env.sh"; exit 1; }
  [[ -n "$OPT_BIN"   ]] || { err "opt not found — run scripts/setup_env.sh"; exit 1; }
  require_plugin

  local ll="$OUTPUT_DIR/$(basename "${src%.*}").ll"
  local json="$OUTPUT_DIR/features.json"
  "$CLANG_BIN" -O0 -emit-llvm -S "$src" -o "$ll" \
    || { err "clang failed to emit IR for $src"; exit 1; }
  "$OPT_BIN" -load-pass-plugin "./$PLUGIN" -passes="ir-complexity" \
    -complexity-output="$json" -disable-output "$ll" \
    || { err "feature extraction pass failed"; exit 1; }
  echo -e "${GREEN}extract${NC}: features written to $json"
}

# Delegate to a Python script once it exists (added in later milestones).
run_py() {
  local script="$1"; shift
  if [[ -f "$MODEL_DIR/$script" ]]; then
    python3 "$MODEL_DIR/$script" "$@"
  else
    err "$MODEL_DIR/$script is not implemented yet (added in a later milestone)"
    exit 1
  fi
}

mode="${1:-}"
[[ -n "$mode" ]] || { err "no mode given"; usage; exit 1; }
shift || true

case "$mode" in
  extract)  mode_extract "${1:-}" ;;
  train)    run_py train.py "$@" ;;
  predict)
    if [[ -z "${1:-}" ]]; then err "predict requires a C source file"; usage; exit 1; fi
    run_py predict.py "$@" ;;
  evaluate) run_py evaluate.py "$@" ;;
  adaptive)
    if [[ -z "${1:-}" ]]; then err "adaptive requires a C source file"; usage; exit 1; fi
    run_py adaptive.py "$@" ;;
  demo)     exec "$ROOT_DIR/scripts/demo.sh" "$@" ;;
  -h|--help|help) usage ;;
  *)        err "unknown mode: $mode"; usage; exit 1 ;;
esac
