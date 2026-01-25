#!/usr/bin/env python3
import argparse
import sys
import time

import serial


def send_command(ser, command):
    ser.write((command.strip() + "\n").encode("ascii"))
    ser.flush()
    time.sleep(0.05)
    lines = []
    while ser.in_waiting:
        line = ser.readline().decode("ascii", errors="ignore").strip()
        if line:
            lines.append(line)
    return lines


def repl(ser):
    print("Enter commands (HELP/GET/SET scrollSensitivity <v>/SAVE/LOAD). Ctrl+C to exit.")
    while True:
        try:
            line = input("> ").strip()
        except KeyboardInterrupt:
            print()
            return
        if not line:
            continue
        for resp in send_command(ser, line):
            print(resp)


def main():
    parser = argparse.ArgumentParser(description="Touchpad serial config tool")
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM3 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    sub = parser.add_subparsers(dest="cmd")

    sub.add_parser("help")

    get_cmd = sub.add_parser("get")
    get_cmd.add_argument("key", nargs="?", help="Optional key, e.g. scrollSensitivity")

    set_cmd = sub.add_parser("set")
    set_cmd.add_argument("key", help="Config key, e.g. scrollSensitivity")
    set_cmd.add_argument("value", help="Value, e.g. 0.00002")

    sub.add_parser("save")
    sub.add_parser("load")
    raw_cmd = sub.add_parser("raw")
    raw_cmd.add_argument("command", help="Raw command to send, e.g. GET")

    sub.add_parser("repl")

    args = parser.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
    except serial.SerialException as exc:
        print(f"Failed to open port: {exc}", file=sys.stderr)
        return 1

    try:
        if args.cmd == "help":
            for line in send_command(ser, "HELP"):
                print(line)
        elif args.cmd == "get":
            cmd = "GET" if args.key is None else f"GET {args.key}"
            for line in send_command(ser, cmd):
                print(line)
        elif args.cmd == "set":
            for line in send_command(ser, f"SET {args.key} {args.value}"):
                print(line)
        elif args.cmd == "save":
            for line in send_command(ser, "SAVE"):
                print(line)
        elif args.cmd == "load":
            for line in send_command(ser, "LOAD"):
                print(line)
        elif args.cmd == "raw":
            for line in send_command(ser, args.command):
                print(line)
        else:
            repl(ser)
    finally:
        ser.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
