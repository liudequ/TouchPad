#!/usr/bin/env python3
import sys

from PySide6 import QtCore, QtWidgets

from serial_client import SerialClient


ACTIONS = ["NONE", "BACK", "FORWARD", "RIGHT_CLICK", "LEFT_CLICK"]


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

        action_group = QtWidgets.QGroupBox("Zone Actions")
        action_layout = QtWidgets.QFormLayout(action_group)
        self.left_top_action = QtWidgets.QComboBox()
        self.right_top_action = QtWidgets.QComboBox()
        self.right_bottom_action = QtWidgets.QComboBox()
        self.left_bottom_action = QtWidgets.QComboBox()
        for cb in (self.left_top_action, self.right_top_action,
                   self.right_bottom_action, self.left_bottom_action):
            cb.addItems(ACTIONS)
        action_layout.addRow("leftTopAction", self.left_top_action)
        action_layout.addRow("rightTopAction", self.right_top_action)
        action_layout.addRow("rightBottomAction", self.right_bottom_action)
        action_layout.addRow("leftBottomAction", self.left_bottom_action)
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
        if "leftTopAction" in data:
            self._set_combo(self.left_top_action, data["leftTopAction"])
        if "rightTopAction" in data:
            self._set_combo(self.right_top_action, data["rightTopAction"])
        if "rightBottomAction" in data:
            self._set_combo(self.right_bottom_action, data["rightBottomAction"])
        if "leftBottomAction" in data:
            self._set_combo(self.left_bottom_action, data["leftBottomAction"])
        self._log("Values refreshed")

    def _set_combo(self, combo, value):
        value = value.strip().upper()
        if value in ACTIONS:
            combo.setCurrentText(value)

    def _apply(self):
        if not self.client.is_connected():
            self._log("Not connected")
            return
        cmds = [
            f"SET scrollSensitivity {self.scroll_sensitivity.value():.8f}",
            f"SET topZonePercent {self.top_percent.value()}",
            f"SET sideZonePercent {self.side_percent.value()}",
            f"SET enableNavZones {1 if self.enable_zones.isChecked() else 0}",
            f"SET leftTopAction {self.left_top_action.currentText()}",
            f"SET rightTopAction {self.right_top_action.currentText()}",
            f"SET rightBottomAction {self.right_bottom_action.currentText()}",
            f"SET leftBottomAction {self.left_bottom_action.currentText()}",
        ]
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


def main():
    app = QtWidgets.QApplication(sys.argv)
    ui = TouchpadConfigUI()
    ui.resize(520, 640)
    ui.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
