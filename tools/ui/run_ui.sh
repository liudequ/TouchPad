#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="${SCRIPT_DIR}/../.."
cd "${ROOT_DIR}"

if [ ! -d ".venv" ]; then
  echo "Missing .venv. Create it with:"
  echo "  python3 -m venv .venv"
  exit 1
fi

# shellcheck disable=SC1091
. ".venv/bin/activate"

python "tools/ui/touchpad_config_ui.py"
