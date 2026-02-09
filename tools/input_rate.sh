#!/usr/bin/env bash
set -euo pipefail

DEV="${1:-}"

if [[ -z "${DEV}" ]]; then
  echo "Usage: $0 /dev/input/eventX" >&2
  echo "Available devices:" >&2
  if [[ -r /proc/bus/input/devices ]]; then
    awk '
      /^I:/ { name=""; handlers="" }
      /^N: Name=/ {
        name=$0
        sub(/^N: Name="/, "", name)
        sub(/"$/, "", name)
      }
      /^H: Handlers=/ {
        handlers=$0
        sub(/^H: Handlers=/, "", handlers)
        if (handlers ~ /event[0-9]+/) {
          match(handlers, /event[0-9]+/)
          ev=substr(handlers, RSTART, RLENGTH)
          printf("  /dev/input/%s  %s\n", ev, name)
        }
      }
    ' /proc/bus/input/devices >&2
  else
    echo "  (cannot read /proc/bus/input/devices)" >&2
  fi
  exit 1
fi

if [[ ! -e "${DEV}" ]]; then
  echo "Device not found: ${DEV}" >&2
  exit 1
fi

if [[ ! -r "${DEV}" ]]; then
  echo "Permission denied for ${DEV}. Try: sudo $0 ${DEV}" >&2
  exit 1
fi

if ! command -v evtest >/dev/null 2>&1; then
  echo "evtest not found. Install it (e.g. apt install evtest) and retry." >&2
  exit 1
fi

echo "[info] reading ${DEV}. Press Ctrl+C to stop." >&2

# Parse evtest stream and print events/sec.
# evtest prints lines like: "Event: time 1700000000.123456, type ..."
# Force line-buffered output from evtest to avoid bursty stats on some hosts.
if command -v stdbuf >/dev/null 2>&1; then
  EVTEST_CMD=(stdbuf -oL -eL evtest "${DEV}")
else
  EVTEST_CMD=(evtest "${DEV}")
fi

"${EVTEST_CMD[@]}" 2>&1 | awk '
  BEGIN { last = -1; count = 0 }
  /^Event: time/ {
    t = $3; sub(/,/, "", t)
    s = int(t)
    if (last == -1) {
      last = s
    }
    if (s != last) {
      printf("[stat] %d events/s\n", count)
      fflush()
      count = 0
      last = s
    }
    count++
  }
'
