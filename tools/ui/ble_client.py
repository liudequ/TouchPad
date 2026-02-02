#!/usr/bin/env python3
import asyncio
import subprocess
import threading
import time

from bleak import BleakClient, BleakScanner


NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"


class BleClient:
    def __init__(self, scan_timeout=3.0, response_timeout=0.3):
        self._scan_timeout = scan_timeout
        self._response_timeout = response_timeout
        self._client = None
        self._rx_buffer = bytearray()
        self._lock = threading.Lock()
        self._loop = asyncio.new_event_loop()
        self._thread = threading.Thread(target=self._run_loop, daemon=True)
        self._thread.start()

    def _run_loop(self):
        asyncio.set_event_loop(self._loop)
        self._loop.run_forever()

    @staticmethod
    def _format_device(device):
        name = device.name or "Unknown"
        return f"{name} ({device.address})"

    @staticmethod
    def _bluetoothctl_devices(subcommand):
        try:
            output = subprocess.check_output(
                ["bluetoothctl", subcommand],
                text=True,
                stderr=subprocess.STDOUT,
            )
        except Exception:
            return []
        items = []
        for line in output.splitlines():
            line = line.strip()
            if not line.startswith("Device "):
                continue
            parts = line.split(" ", 2)
            if len(parts) < 2:
                continue
            address = parts[1]
            name = parts[2] if len(parts) >= 3 else "Unknown"
            items.append((name, address))
        return items

    def _list_paired_devices(self):
        return self._bluetoothctl_devices("paired-devices")

    def _list_connected_devices(self):
        return self._bluetoothctl_devices("devices Connected")

    def list_devices(self):
        async def _scan():
            devices = await BleakScanner.discover(timeout=self._scan_timeout)
            items = []
            for dev in devices:
                items.append((self._format_device(dev), dev.address))
            return items

        items = []
        seen = set()
        connected = self._list_connected_devices()
        connected_addrs = {addr for _, addr in connected}

        def add_item(label, address):
            if not address or address in seen:
                return
            seen.add(address)
            items.append((label, address))

        for name, address in connected:
            base = f"{name} ({address})" if name else address
            add_item(f"{base} [connected]", address)

        try:
            future = asyncio.run_coroutine_threadsafe(_scan(), self._loop)
            scanned = future.result()
        except Exception:
            scanned = []
        for label, address in scanned:
            if address in connected_addrs and "[connected]" not in label:
                label = f"{label} [connected]"
            add_item(label, address)

        for name, address in self._list_paired_devices():
            base = f"{name} ({address})" if name else address
            label = f"{base} [paired]"
            if address in connected_addrs:
                label = f"{base} [connected]"
            add_item(label, address)

        return items

    def _notify_handler(self, _sender, data):
        with self._lock:
            self._rx_buffer.extend(data)

    def connect(self, address):
        async def _connect():
            client = BleakClient(address)
            await client.connect()
            await client.start_notify(NUS_TX_UUID, self._notify_handler)
            return client

        self.disconnect()
        future = asyncio.run_coroutine_threadsafe(_connect(), self._loop)
        self._client = future.result()

    def disconnect(self):
        if not self._client:
            return

        async def _disconnect(client):
            try:
                await client.stop_notify(NUS_TX_UUID)
            except Exception:
                pass
            await client.disconnect()

        client = self._client
        self._client = None
        future = asyncio.run_coroutine_threadsafe(_disconnect(client), self._loop)
        future.result()

    def is_connected(self):
        return self._client is not None and self._client.is_connected

    def send(self, command):
        if not self.is_connected():
            raise RuntimeError("BLE not connected")

        async def _write(data):
            await self._client.write_gatt_char(NUS_RX_UUID, data, response=False)

        with self._lock:
            self._rx_buffer.clear()

        payload = (command.strip() + "\n").encode("ascii")
        future = asyncio.run_coroutine_threadsafe(_write(payload), self._loop)
        future.result()

        deadline = time.time() + self._response_timeout
        last_len = -1
        while time.time() < deadline:
            with self._lock:
                current_len = len(self._rx_buffer)
            if current_len == last_len:
                time.sleep(0.05)
                if current_len == last_len:
                    break
            else:
                last_len = current_len
                time.sleep(0.05)

        with self._lock:
            data = bytes(self._rx_buffer)

        lines = []
        for line in data.decode("ascii", errors="ignore").splitlines():
            line = line.strip()
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
