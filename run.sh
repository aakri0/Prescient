#!/usr/bin/env bash
# run.sh — entry point for the Prescient pipeline.
# Modes: extract | train | predict | evaluate | adaptive | demo
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

PLUGIN="build/IRComplexityEstimator.so"
OUTPUT_DIR="output"
mkdir -p "$OUTPUT_DIR"

if [[ -t 1 ]]; then
  RED='\033[0;31m'; GREEN='\033[0;32m'; NC='\033[0m'
else
  RED=''; GREEN=''; NC=''
fi
err() { echo -e "${RED}error${NC}: $*" >&2; }
ok()  { echo -e "${GREEN}$*${NC}"; }

usage() {
  cat >&2 <<'EOF'
Usage: ./run.sh <mode> [args]

Modes:
  extract <file.c>   Extract IR features from a C source file -> output/features.json
  train              Build the training corpus and train the model
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

require_models() {
  [[ -f "models/training_metadata.json" ]] \
    || { err "no trained model found — run ./run.sh train first"; exit 1; }
}

# Extract IR-complexity features from a C file into output/features.json.
# A second argument of "quiet" suppresses the feature table — used by
# mode_predict, whose own output already prints the feature table.
mode_extract() {
  local src="${1:-}"
  local quiet="${2:-}"
  if [[ -z "$src" ]]; then err "extract requires a C source file"; usage; exit 1; fi
  [[ -f "$src" ]] || { err "no such file: $src"; exit 1; }
  [[ -n "$CLANG_BIN" ]] || { err "clang not found — run scripts/setup_env.sh"; exit 1; }
  [[ -n "$OPT_BIN"   ]] || { err "opt not found — run scripts/setup_env.sh"; exit 1; }
  require_plugin

  local ll="$OUTPUT_DIR/$(basename "${src%.*}").ll"
  local json="$OUTPUT_DIR/features.json"
  "$CLANG_BIN" -O0 -Xclang -disable-O0-optnone -emit-llvm -S "$src" -o "$ll" \
    || { err "clang failed to emit IR for $src"; exit 1; }

  # The pass prints one progress line per function to stderr; capture it so
  # a clean run shows only the feature table, and surface it on failure.
  local optlog
  if ! optlog="$("$OPT_BIN" -load-pass-plugin "./$PLUGIN" -passes="ir-complexity" \
      -complexity-output="$json" -disable-output "$ll" 2>&1)"; then
    err "feature extraction pass failed"
    [[ -n "$optlog" ]] && printf '%s\n' "$optlog" >&2
    exit 1
  fi

  # Show the extracted features as a readable table (needs python3).
  if [[ "$quiet" != "quiet" ]] && command -v python3 >/dev/null 2>&1; then
    python3 "$ROOT_DIR/src/model/show_features.py" "$json" || true
  fi
  ok "extract: features written to $json"
}

# Build the training corpus from testcases/training/ and train the model.
mode_train() {
  require_plugin
  local data="$OUTPUT_DIR/training_data.csv"
  python3 scripts/generate_corpus.py \
    --input-dir testcases/training --output "$data" --plugin "$PLUGIN" \
    || { err "corpus generation failed"; exit 1; }
  python3 src/model/train_model.py \
    --data "$data" --models-dir models --docs-dir docs \
    || { err "model training failed"; exit 1; }
  ok "train: model written to models/"
}

# Predict compile time for one C file.
mode_predict() {
  local src="${1:-}"
  if [[ -z "$src" ]]; then err "predict requires a C source file"; usage; exit 1; fi
  require_models
  mode_extract "$src" quiet
  python3 src/model/predict.py \
    --features "$OUTPUT_DIR/features.json" --models-dir models \
    --output "$OUTPUT_DIR/predictions.json" \
    || { err "prediction failed"; exit 1; }
  ok "predict: predictions written to $OUTPUT_DIR/predictions.json"
}

mode="${1:-}"
[[ -n "$mode" ]] || { err "no mode given"; usage; exit 1; }
shift || true

case "$mode" in
  extract)  mode_extract "${1:-}" ;;
  train)    mode_train ;;
  predict)  mode_predict "${1:-}" ;;
  evaluate)
    require_plugin; require_models
    python3 scripts/evaluate.py --eval-dir testcases/evaluation \
      --models-dir models --plugin "$PLUGIN" --docs-dir docs ;;
  adaptive)
    if [[ -z "${1:-}" ]]; then err "adaptive requires a C source file"; usage; exit 1; fi
    exec "$ROOT_DIR/scripts/run_adaptive.sh" "$1" ;;
  demo)     exec "$ROOT_DIR/scripts/demo.sh" "$@" ;;
  -h|--help|help) usage ;;
  *)        err "unknown mode: $mode"; usage; exit 1 ;;
esac
