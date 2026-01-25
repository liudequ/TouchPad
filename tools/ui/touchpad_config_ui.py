#!/usr/bin/env python3
import sys

from PySide6 import QtCore, QtWidgets

from serial_client import SerialClient


ZONE_TYPES = ["NONE", "MOUSE", "KEYBOARD"]


class TouchpadConfigUI(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Touchpad Config")
        self.client = SerialClient()
        self._build_ui()
        self._refresh_ports()

    def _build_ui(self):
        layout = QtWidgets.QVBoxLayout(self)

        conn_group = QtWidgets.QGroupBox("Connection")
        conn_layout = QtWidgets.QHBoxLayout(conn_group)
        self.port_combo = QtWidgets.QComboBox()
        self.refresh_btn = QtWidgets.QPushButton("Refresh")
        self.connect_btn = QtWidgets.QPushButton("Connect")
        self.status_label = QtWidgets.QLabel("Disconnected")
        conn_layout.addWidget(self.port_combo)
        conn_layout.addWidget(self.refresh_btn)
        conn_layout.addWidget(self.connect_btn)
        conn_layout.addWidget(self.status_label)
        layout.addWidget(conn_group)

        scroll_group = QtWidgets.QGroupBox("Scroll")
        scroll_layout = QtWidgets.QFormLayout(scroll_group)
        self.scroll_sensitivity = QtWidgets.QDoubleSpinBox()
        self.scroll_sensitivity.setDecimals(8)
        self.scroll_sensitivity.setSingleStep(0.000001)
        self.scroll_sensitivity.setRange(0.0, 1.0)
        scroll_layout.addRow("scrollSensitivity", self.scroll_sensitivity)
        layout.addWidget(scroll_group)

        zone_group = QtWidgets.QGroupBox("Zones")
        zone_layout = QtWidgets.QFormLayout(zone_group)
        self.top_percent = QtWidgets.QSpinBox()
        self.top_percent.setRange(5, 50)
        self.side_percent = QtWidgets.QSpinBox()
        self.side_percent.setRange(5, 50)
        self.enable_zones = QtWidgets.QCheckBox("enableNavZones")
        zone_layout.addRow("topZonePercent", self.top_percent)
        zone_layout.addRow("sideZonePercent", self.side_percent)
        zone_layout.addRow(self.enable_zones)
        layout.addWidget(zone_group)

        action_group = QtWidgets.QGroupBox("Zone Bindings")
        action_layout = QtWidgets.QFormLayout(action_group)
        self.left_top = self._create_zone_widgets()
        self.right_top = self._create_zone_widgets()
        self.right_bottom = self._create_zone_widgets()
        self.left_bottom = self._create_zone_widgets()
        self._add_zone_rows(action_layout, "leftTop", self.left_top)
        self._add_zone_rows(action_layout, "rightTop", self.right_top)
        self._add_zone_rows(action_layout, "rightBottom", self.right_bottom)
        self._add_zone_rows(action_layout, "leftBottom", self.left_bottom)
        layout.addWidget(action_group)

        ops_layout = QtWidgets.QHBoxLayout()
        self.apply_btn = QtWidgets.QPushButton("Apply")
        self.refresh_values_btn = QtWidgets.QPushButton("Refresh Values")
        self.save_btn = QtWidgets.QPushButton("Save")
        self.load_btn = QtWidgets.QPushButton("Load")
        self.reset_btn = QtWidgets.QPushButton("Reset")
        ops_layout.addWidget(self.apply_btn)
        ops_layout.addWidget(self.refresh_values_btn)
        ops_layout.addWidget(self.save_btn)
        ops_layout.addWidget(self.load_btn)
        ops_layout.addWidget(self.reset_btn)
        layout.addLayout(ops_layout)

        self.log = QtWidgets.QPlainTextEdit()
        self.log.setReadOnly(True)
        layout.addWidget(self.log)

        self.refresh_btn.clicked.connect(self._refresh_ports)
        self.connect_btn.clicked.connect(self._toggle_connection)
        self.apply_btn.clicked.connect(self._apply)
        self.refresh_values_btn.clicked.connect(self._refresh_values)
        self.save_btn.clicked.connect(self._save)
        self.load_btn.clicked.connect(self._load)
        self.reset_btn.clicked.connect(self._reset)

    def _refresh_ports(self):
        ports = SerialClient.list_ports()
        self.port_combo.clear()
        self.port_combo.addItems(ports)
        if not ports:
            self.status_label.setText("No ports")

    def _toggle_connection(self):
        if self.client.is_connected():
            self.client.disconnect()
            self.status_label.setText("Disconnected")
            self.connect_btn.setText("Connect")
            return
        port = self.port_combo.currentText()
        if not port:
            self._log("No port selected")
            return
        try:
            self.client.connect(port)
        except Exception as exc:
            self._log(f"Connect failed: {exc}")
            return
        self.status_label.setText("Connected")
        self.connect_btn.setText("Disconnect")
        self._refresh_values()

    def _log(self, text):
        self.log.appendPlainText(text)

    def _refresh_values(self):
        if not self.client.is_connected():
            self._log("Not connected")
            return
        data = self.client.get_all()
        if "scrollSensitivity" in data:
            self.scroll_sensitivity.setValue(float(data["scrollSensitivity"]))
        if "topZonePercent" in data:
            self.top_percent.setValue(int(data["topZonePercent"]))
        if "sideZonePercent" in data:
            self.side_percent.setValue(int(data["sideZonePercent"]))
        if "enableNavZones" in data:
            self.enable_zones.setChecked(data["enableNavZones"] == "1")
        self._apply_zone_values(self.left_top, data, "leftTop")
        self._apply_zone_values(self.right_top, data, "rightTop")
        self._apply_zone_values(self.right_bottom, data, "rightBottom")
        self._apply_zone_values(self.left_bottom, data, "leftBottom")
        self._log("Values refreshed")

    def _set_combo(self, combo, value):
        value = value.strip().upper()
        if value in ZONE_TYPES:
            combo.setCurrentText(value)

    def _create_zone_widgets(self):
        type_combo = QtWidgets.QComboBox()
        type_combo.addItems(ZONE_TYPES)
        buttons = QtWidgets.QSpinBox()
        buttons.setRange(0, 7)
        modifier = QtWidgets.QSpinBox()
        modifier.setRange(0, 255)
        key = QtWidgets.QSpinBox()
        key.setRange(0, 255)
        return {
            "type": type_combo,
            "buttons": buttons,
            "modifier": modifier,
            "key": key,
        }

    def _add_zone_rows(self, layout, name, widgets):
        layout.addRow(f"{name}Type", widgets["type"])
        layout.addRow(f"{name}Buttons", widgets["buttons"])
        layout.addRow(f"{name}Modifier", widgets["modifier"])
        layout.addRow(f"{name}Key", widgets["key"])

    def _apply_zone_values(self, widgets, data, name):
        type_key = f"{name}Type"
        buttons_key = f"{name}Buttons"
        modifier_key = f"{name}Modifier"
        key_key = f"{name}Key"
        if type_key in data:
            self._set_combo(widgets["type"], data[type_key])
        if buttons_key in data:
            widgets["buttons"].setValue(int(data[buttons_key]))
        if modifier_key in data:
            widgets["modifier"].setValue(int(data[modifier_key]))
        if key_key in data:
            widgets["key"].setValue(int(data[key_key]))

    def _apply(self):
        if not self.client.is_connected():
            self._log("Not connected")
            return
        cmds = [
            f"SET scrollSensitivity {self.scroll_sensitivity.value():.8f}",
            f"SET topZonePercent {self.top_percent.value()}",
            f"SET sideZonePercent {self.side_percent.value()}",
            f"SET enableNavZones {1 if self.enable_zones.isChecked() else 0}",
        ]
        cmds += self._zone_set_cmds("leftTop", self.left_top)
        cmds += self._zone_set_cmds("rightTop", self.right_top)
        cmds += self._zone_set_cmds("rightBottom", self.right_bottom)
        cmds += self._zone_set_cmds("leftBottom", self.left_bottom)
        for cmd in cmds:
            for line in self.client.send(cmd):
                self._log(line)

    def _save(self):
        self._simple_cmd("SAVE")

    def _load(self):
        self._simple_cmd("LOAD")
        self._refresh_values()

    def _reset(self):
        self._simple_cmd("RESET")
        self._refresh_values()

    def _simple_cmd(self, cmd):
        if not self.client.is_connected():
            self._log("Not connected")
            return
        for line in self.client.send(cmd):
            self._log(line)

    def _zone_set_cmds(self, name, widgets):
        return [
            f"SET {name}Type {widgets['type'].currentText()}",
            f"SET {name}Buttons {widgets['buttons'].value()}",
            f"SET {name}Modifier {widgets['modifier'].value()}",
            f"SET {name}Key {widgets['key'].value()}",
        ]


def main():
    app = QtWidgets.QApplication(sys.argv)
    ui = TouchpadConfigUI()
    ui.resize(520, 640)
    ui.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
