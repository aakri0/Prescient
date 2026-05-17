#!/usr/bin/env bash
# build.sh — configure and build the IRComplexityEstimator plugin.
# Run after scripts/setup_env.sh has succeeded.
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

if [[ -t 1 ]]; then
  RED='\033[0;31m'; GREEN='\033[0;32m'; NC='\033[0m'
else
  RED=''; GREEN=''; NC=''
fi
die() { echo -e "${RED}BUILD FAILED${NC}: $*" >&2; exit 1; }

# --- preconditions --------------------------------------------------------
LLVM_CONFIG="$(command -v llvm-config-17 || command -v llvm-config || true)"
[[ -n "$LLVM_CONFIG" ]] || die "llvm-config not found — run scripts/setup_env.sh"
LLVM_VER="$("$LLVM_CONFIG" --version)"
[[ "$LLVM_VER" == 17.* ]] || die "LLVM 17 required, found $LLVM_VER"

command -v cmake >/dev/null || die "cmake not found — run scripts/setup_env.sh"
CMAKE_VER="$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?')"
CMAKE_MAJOR="${CMAKE_VER%%.*}"
CMAKE_MINOR="$(echo "$CMAKE_VER" | cut -d. -f2)"
if (( CMAKE_MAJOR < 3 || (CMAKE_MAJOR == 3 && CMAKE_MINOR < 20) )); then
  die "cmake >= 3.20 required, found $CMAKE_VER"
fi

python3 -c "import sklearn, pandas, joblib" 2>/dev/null \
  || die "Python packages missing — run scripts/setup_env.sh"

# --- configure & build ----------------------------------------------------
mkdir -p build
( cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR="$("$LLVM_CONFIG" --cmakedir)" ) \
  || die "cmake configuration failed"
( cd build && make -j"$(nproc)" ) || die "make failed"

ARTIFACT="build/IRComplexityEstimator.so"
[[ -f "$ARTIFACT" ]] || die "$ARTIFACT was not produced"

# --- python deps ----------------------------------------------------------
python3 -m pip install -r requirements.txt >/dev/null 2>&1 \
  || die "pip install -r requirements.txt failed"

echo -e "${GREEN}BUILD SUCCESSFUL${NC}: $ARTIFACT"
