#!/usr/bin/env bash
# start.sh — single entry point for every Prescient command.
#
# Usage:
#   ./start.sh                 Start the web UI at http://localhost:8080
#   ./start.sh extract <file>  Extract IR features from a C/C++ source file
#   ./start.sh train           Build corpus from testcases/ and train the model
#   ./start.sh predict <file>  Predict compile cost for a C/C++ source file
#   ./start.sh evaluate        Run the full evaluation suite
#   ./start.sh adaptive <file> Run the adaptive pipeline on a source file
#   ./start.sh demo            Run the end-to-end demo
#   ./start.sh build           Force-rebuild the Docker image
#   ./start.sh help            Show this message
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

IMAGE="prescient:latest"

# ─── colours ─────────────────────────────────────────────────────────
if [[ -t 1 ]]; then
  RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; NC='\033[0m'
else
  RED=''; GREEN=''; CYAN=''; NC=''
fi
err() { echo -e "${RED}error${NC}: $*" >&2; }
ok()  { echo -e "${GREEN}$*${NC}"; }
info() { echo -e "${CYAN}$*${NC}"; }

# ─── prerequisites ───────────────────────────────────────────────────
check_docker() {
  if ! command -v docker >/dev/null 2>&1; then
    err "docker is not installed — see https://docs.docker.com/get-docker/"
    exit 1
  fi
  if ! docker info >/dev/null 2>&1; then
    err "docker daemon is not running — start Docker Desktop (or dockerd)"
    exit 1
  fi
}

# Build the image if it doesn't exist yet (or if explicitly asked).
ensure_image() {
  if [[ "${1:-}" == "force" ]] || ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    info "Building the Prescient Docker image (first run takes a few minutes)..."
    docker compose build --quiet || { err "image build failed"; exit 1; }
    ok "Image built."
  fi
}

# ─── resolve a host file path into the container mount ───────────────
# testcases/ is mounted read-only; output/ is read-write.
container_path() {
  local host_path="$1"
  local abs
  abs="$(cd "$(dirname "$host_path")" && pwd)/$(basename "$host_path")"

  # Inside testcases/
  if [[ "$abs" == "$ROOT_DIR/testcases/"* ]]; then
    echo "${abs#"$ROOT_DIR/"}"
    return
  fi
  # Copy to a temp location in the container via a bind mount
  echo "$abs"
}

# ─── pipeline commands (run in a one-shot container) ─────────────────
run_pipeline() {
  local mode="$1"; shift
  local file="${1:-}"
  local -a extra_mounts=()

  # Modes that take a source file
  if [[ "$mode" =~ ^(extract|predict|adaptive)$ ]]; then
    if [[ -z "$file" ]]; then
      err "$mode requires a C/C++ source file"
      echo "Usage: ./start.sh $mode <file.c>" >&2
      exit 1
    fi
    if [[ ! -f "$file" ]]; then
      err "no such file: $file"
      exit 1
    fi

    local abs
    abs="$(cd "$(dirname "$file")" && pwd)/$(basename "$file")"

    # If the file is already under testcases/, the existing mount covers it.
    # Otherwise, bind-mount its parent directory so the container can see it.
    if [[ "$abs" != "$ROOT_DIR/testcases/"* ]]; then
      local dir
      dir="$(dirname "$abs")"
      extra_mounts=(-v "$dir:$dir:ro")
      file="$abs"
    fi
  fi

  if (( ${#extra_mounts[@]} > 0 )); then
    docker compose run --rm "${extra_mounts[@]}" prescient "$mode" "$file" "$@"
  else
    docker compose run --rm prescient "$mode" ${file:+"$file"} "$@"
  fi
}

# ─── web frontend ────────────────────────────────────────────────────
start_web() {
  # Stop any existing instance first
  docker compose down --remove-orphans >/dev/null 2>&1 || true
  info "Starting the Prescient web UI..."
  docker compose up frontend -d || { err "failed to start the web UI"; exit 1; }
  echo
  ok "Prescient is running at: http://localhost:8080"
  echo "  Stop it with: ./stop.sh"
  echo
}

# ─── main ────────────────────────────────────────────────────────────
MODE="${1:-web}"
shift 2>/dev/null || true

case "$MODE" in
  web|start|ui|frontend)
    check_docker
    ensure_image
    start_web
    ;;
  build)
    check_docker
    ensure_image force
    ;;
  extract|train|predict|evaluate|adaptive|demo)
    check_docker
    ensure_image
    run_pipeline "$MODE" "$@"
    ;;
  stop)
    # Also accept `./start.sh stop` as an alias for ./stop.sh
    check_docker
    docker compose down --remove-orphans 2>/dev/null
    ok "Prescient stopped."
    ;;
  -h|--help|help)
    cat <<'EOF'
Prescient — start.sh

Usage:
  ./start.sh                 Start the web UI at http://localhost:8080
  ./start.sh extract <file>  Extract IR features from a C/C++ source file
  ./start.sh train           Build corpus from testcases/ and train the model
  ./start.sh predict <file>  Predict compile cost for a C/C++ source file
  ./start.sh evaluate        Run the full evaluation suite
  ./start.sh adaptive <file> Run the adaptive pipeline on a source file
  ./start.sh demo            Run the end-to-end demo
  ./start.sh build           Force-rebuild the Docker image
  ./start.sh stop            Stop the web UI (same as ./stop.sh)
  ./start.sh help            Show this message

First run builds the Docker image automatically (~5 min).
The trained model persists in models/ between runs.
EOF
    ;;
  *)
    err "unknown command: $MODE"
    echo "Run ./start.sh help for usage." >&2
    exit 1
    ;;
esac
