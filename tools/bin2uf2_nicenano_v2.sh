#!/usr/bin/env bash
set -euo pipefail

if [[ ${1:-} == "" || ${1:-} == "-h" || ${1:-} == "--help" ]]; then
  echo "Usage: $(basename "$0") <input.bin|input.hex|input.elf> [output.uf2]"
  echo "Convert firmware to UF2 for nice!nano v2 (nRF52840)."
  echo
  echo "Defaults: base=0x26000, family=0xADA52840"
  exit 0
fi

input="$1"
if [[ ! -f "$input" ]]; then
  echo "Error: input file not found: $input" >&2
  exit 1
fi

output="${2:-}"
if [[ -z "$output" ]]; then
  output="${input%.*}.uf2"
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
uf2conv="$repo_root/hardware/pdcook/nrf52/tools/uf2conv/uf2conv.py"

if [[ ! -f "$uf2conv" ]]; then
  echo "Error: uf2conv.py not found at: $uf2conv" >&2
  exit 1
fi

base=0x26000
family=0xADA52840

ext="${input##*.}"
ext="${ext,,}"

tmp_hex=""
cleanup() {
  if [[ -n "$tmp_hex" && -f "$tmp_hex" ]]; then
    rm -f "$tmp_hex"
  fi
}
trap cleanup EXIT

case "$ext" in
  bin)
    python3 "$uf2conv" -c -b "$base" -f "$family" -o "$output" "$input"
    ;;
  hex)
    python3 "$uf2conv" -c -f "$family" -o "$output" "$input"
    ;;
  elf)
    if command -v arm-none-eabi-objcopy >/dev/null 2>&1; then
      tmp_hex="$(mktemp /tmp/firmware.XXXXXX.hex)"
      arm-none-eabi-objcopy -O ihex "$input" "$tmp_hex"
      python3 "$uf2conv" -c -f "$family" -o "$output" "$tmp_hex"
    else
      echo "Error: arm-none-eabi-objcopy not found; cannot convert .elf to .hex." >&2
      echo "Hint: export .hex from the Arduino build, or install the ARM GNU toolchain." >&2
      exit 1
    fi
    ;;
  *)
    echo "Error: unsupported input extension: .$ext" >&2
    echo "Supported: .bin .hex .elf" >&2
    exit 1
    ;;
esac

echo "Wrote: $output"
