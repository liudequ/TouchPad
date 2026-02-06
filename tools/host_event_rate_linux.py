#!/usr/bin/env python3
# Count mouse move events per second on Linux.
# Usage: sudo python3 tools/host_event_rate_linux.py /dev/input/eventX

import sys
import time

try:
    from evdev import InputDevice, ecodes
except ImportError:
    print("Missing evdev. Install: pip install evdev")
    sys.exit(1)

if len(sys.argv) != 2:
    print("Usage: sudo python3 tools/host_event_rate_linux.py /dev/input/eventX")
    sys.exit(1)

path = sys.argv[1]

dev = InputDevice(path)
print(f"Listening on {dev.path} ({dev.name})")

count = 0
last = time.time()

for event in dev.read_loop():
    if event.type == ecodes.EV_REL:
        if event.code in (ecodes.REL_X, ecodes.REL_Y):
            count += 1
    now = time.time()
    if now - last >= 1.0:
        print(f"events/s: {count}")
        count = 0
        last = now
