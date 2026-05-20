#!/usr/bin/env bash
# stop.sh — stop the Prescient web UI (and any other running containers).
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

if [[ -t 1 ]]; then
  GREEN='\033[0;32m'; NC='\033[0m'
else
  GREEN=''; NC=''
fi

if ! command -v docker >/dev/null 2>&1 || ! docker info >/dev/null 2>&1; then
  echo "Docker is not running — nothing to stop."
  exit 0
fi

docker compose down --remove-orphans 2>/dev/null
echo -e "${GREEN}Prescient stopped.${NC}"
