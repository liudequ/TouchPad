#!/usr/bin/env python3
import time

import serial
from serial.tools import list_ports


class SerialClient:
    def __init__(self, baud=115200, timeout=0.3):
        self._baud = baud
        self._timeout = timeout
        self._ser = None

    @staticmethod
    def list_ports():
        return [p.device for p in list_ports.comports()]

    def connect(self, port):
        self.disconnect()
        self._ser = serial.Serial(port, self._baud, timeout=self._timeout)

    def disconnect(self):
        if self._ser:
            self._ser.close()
            self._ser = None

    def is_connected(self):
        return self._ser is not None and self._ser.is_open

    def send(self, command):
        if not self.is_connected():
            raise RuntimeError("Serial not connected")
        self._ser.write((command.strip() + "\n").encode("ascii"))
        self._ser.flush()
        time.sleep(0.05)
        lines = []
        while self._ser.in_waiting:
            line = self._ser.readline().decode("ascii", errors="ignore").strip()
            if line:
                lines.append(line)
        return lines

    def get_all(self):
        lines = self.send("GET")
        data = {}
        for line in lines:
            if "=" in line:
                key, value = line.split("=", 1)
                data[key.strip()] = value.strip()
        return data
