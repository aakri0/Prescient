#!/usr/bin/env bash
# setup_env.sh — prepare a clean Ubuntu 22.04 machine for the Prescient project.
# Installs LLVM 17, build tooling, Python and the required Python packages.
# Idempotent: safe to run repeatedly.
set -euo pipefail

LLVM_VERSION=17

# --- colour output --------------------------------------------------------
if [[ -t 1 ]]; then
  RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
else
  RED=''; GREEN=''; YELLOW=''; NC=''
fi
info()  { echo -e "${YELLOW}[setup]${NC} $*"; }
ok()    { echo -e "${GREEN}[ ok ]${NC} $*"; }
fail()  { echo -e "${RED}[fail]${NC} $*" >&2; }

SUDO=""
if [[ "$(id -u)" -ne 0 ]]; then
  SUDO="sudo"
fi

trap 'fail "setup_env.sh aborted on line $LINENO"; exit 1' ERR

# --- LLVM apt repository --------------------------------------------------
info "Adding the LLVM ${LLVM_VERSION} apt repository"
$SUDO apt-get update -y
$SUDO apt-get install -y wget gnupg lsb-release software-properties-common ca-certificates

if [[ ! -f /etc/apt/sources.list.d/llvm.list ]]; then
  wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key \
    | $SUDO gpg --dearmor -o /usr/share/keyrings/llvm-snapshot.gpg
  CODENAME="$(lsb_release -cs)"
  echo "deb [signed-by=/usr/share/keyrings/llvm-snapshot.gpg] http://apt.llvm.org/${CODENAME}/ llvm-toolchain-${CODENAME}-${LLVM_VERSION} main" \
    | $SUDO tee /etc/apt/sources.list.d/llvm.list >/dev/null
  $SUDO apt-get update -y
fi

# --- LLVM / Clang toolchain ----------------------------------------------
info "Installing LLVM ${LLVM_VERSION} toolchain"
$SUDO apt-get install -y \
  "llvm-${LLVM_VERSION}" \
  "clang-${LLVM_VERSION}" \
  "llvm-${LLVM_VERSION}-dev" \
  "libclang-${LLVM_VERSION}-dev" \
  "lld-${LLVM_VERSION}"

# --- build tooling --------------------------------------------------------
info "Installing build tooling"
# libzstd-dev is required: LLVM 17's exported CMake targets reference the
# zstd::libzstd_shared imported target as a transitive link dependency.
$SUDO apt-get install -y cmake ninja-build build-essential git libzstd-dev

# --- Python ---------------------------------------------------------------
info "Installing Python 3"
$SUDO apt-get install -y python3 python3-pip python3-venv

info "Installing Python packages"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REQ_FILE="${SCRIPT_DIR}/../requirements.txt"
if [[ -f "$REQ_FILE" ]]; then
  python3 -m pip install --upgrade pip
  python3 -m pip install -r "$REQ_FILE"
else
  python3 -m pip install --upgrade pip
  python3 -m pip install scikit-learn pandas numpy joblib matplotlib seaborn
fi

# --- symlinks so unsuffixed tool names resolve to v17 ---------------------
info "Setting up unsuffixed tool symlinks"
for tool in clang clang++ opt llc llvm-config; do
  src="/usr/bin/${tool}-${LLVM_VERSION}"
  if [[ -x "$src" ]]; then
    $SUDO ln -sf "$src" "/usr/local/bin/${tool}"
  fi
done

# --- verification ---------------------------------------------------------
info "Verifying installation"
LLVM_CONFIG="$(command -v llvm-config-${LLVM_VERSION} || command -v llvm-config)"
VER="$("$LLVM_CONFIG" --version)"
if [[ "$VER" != 17.* ]]; then
  fail "llvm-config reported version '$VER', expected 17.x"
  exit 1
fi
ok "llvm-config --version = $VER"

if python3 -c "import sklearn, pandas, joblib, numpy, matplotlib, seaborn" 2>/dev/null; then
  ok "Python packages import cleanly"
else
  fail "Python package import check failed"
  exit 1
fi

trap - ERR
echo
ok "Environment setup complete — LLVM ${LLVM_VERSION} and Python deps ready."
