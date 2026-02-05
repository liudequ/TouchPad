#!/usr/bin/env bash
set -euo pipefail

if [[ ${1:-} == "-h" || ${1:-} == "--help" ]]; then
  echo "Usage: $(basename "$0") [build_dir] [output.uf2]"
  echo "Find the newest .hex/.elf/.bin under build_dir and convert to UF2 for nice!nano v2."
  echo
  echo "Defaults: build_dir=sketches, output=<input>.uf2"
  exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"

build_dir="${1:-$repo_root/sketches}"
if [[ ! -d "$build_dir" ]]; then
  echo "Error: build_dir not found: $build_dir" >&2
  exit 1
fi

# Find newest firmware artifact
mapfile -t candidates < <(
  find "$build_dir" -type f \( -name '*.hex' -o -name '*.elf' -o -name '*.bin' \) \
    ! -path '*/.git/*' -printf '%T@\t%p\n' 2>/dev/null | sort -n
)

if [[ ${#candidates[@]} -eq 0 ]]; then
  echo "Error: no .hex/.elf/.bin found under: $build_dir" >&2
  exit 1
fi

newest_line="${candidates[-1]}"
newest_path="${newest_line#*$'\t'}"

output="${2:-${newest_path%.*}.uf2}"

bin2uf2="$repo_root/tools/bin2uf2_nicenano_v2.sh"
if [[ ! -x "$bin2uf2" ]]; then
  echo "Error: converter not found or not executable: $bin2uf2" >&2
  exit 1
fi

"$bin2uf2" "$newest_path" "$output"

echo "Input: $newest_path"
