#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [ ! -d "${ROOT_DIR}/.venv" ]; then
  echo "Missing .venv. Create it with:"
  echo "  python3 -m venv .venv"
  exit 1
fi

# shellcheck disable=SC1091
. "${ROOT_DIR}/.venv/bin/activate"

python "${ROOT_DIR}/tools/ui/touchpad_config_ui.py"
