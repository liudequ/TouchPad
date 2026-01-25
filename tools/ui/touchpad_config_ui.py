#!/usr/bin/env python3
import sys

from PySide6 import QtCore, QtGui, QtWidgets

from serial_client import SerialClient


ZONE_TYPES = ["NONE", "MOUSE", "KEYBOARD"]
HID_KEY_MAP = {
    QtCore.Qt.Key.Key_A: 4,
    QtCore.Qt.Key.Key_B: 5,
    QtCore.Qt.Key.Key_C: 6,
    QtCore.Qt.Key.Key_D: 7,
    QtCore.Qt.Key.Key_E: 8,
    QtCore.Qt.Key.Key_F: 9,
    QtCore.Qt.Key.Key_G: 10,
    QtCore.Qt.Key.Key_H: 11,
    QtCore.Qt.Key.Key_I: 12,
    QtCore.Qt.Key.Key_J: 13,
    QtCore.Qt.Key.Key_K: 14,
    QtCore.Qt.Key.Key_L: 15,
    QtCore.Qt.Key.Key_M: 16,
    QtCore.Qt.Key.Key_N: 17,
    QtCore.Qt.Key.Key_O: 18,
    QtCore.Qt.Key.Key_P: 19,
    QtCore.Qt.Key.Key_Q: 20,
    QtCore.Qt.Key.Key_R: 21,
    QtCore.Qt.Key.Key_S: 22,
    QtCore.Qt.Key.Key_T: 23,
    QtCore.Qt.Key.Key_U: 24,
    QtCore.Qt.Key.Key_V: 25,
    QtCore.Qt.Key.Key_W: 26,
    QtCore.Qt.Key.Key_X: 27,
    QtCore.Qt.Key.Key_Y: 28,
    QtCore.Qt.Key.Key_Z: 29,
    QtCore.Qt.Key.Key_1: 30,
    QtCore.Qt.Key.Key_2: 31,
    QtCore.Qt.Key.Key_3: 32,
    QtCore.Qt.Key.Key_4: 33,
    QtCore.Qt.Key.Key_5: 34,
    QtCore.Qt.Key.Key_6: 35,
    QtCore.Qt.Key.Key_7: 36,
    QtCore.Qt.Key.Key_8: 37,
    QtCore.Qt.Key.Key_9: 38,
    QtCore.Qt.Key.Key_0: 39,
    QtCore.Qt.Key.Key_Return: 40,
    QtCore.Qt.Key.Key_Enter: 40,
    QtCore.Qt.Key.Key_Escape: 41,
    QtCore.Qt.Key.Key_Backspace: 42,
    QtCore.Qt.Key.Key_Tab: 43,
    QtCore.Qt.Key.Key_Space: 44,
    QtCore.Qt.Key.Key_Minus: 45,
    QtCore.Qt.Key.Key_Equal: 46,
    QtCore.Qt.Key.Key_BracketLeft: 47,
    QtCore.Qt.Key.Key_BracketRight: 48,
    QtCore.Qt.Key.Key_Backslash: 49,
    QtCore.Qt.Key.Key_Semicolon: 51,
    QtCore.Qt.Key.Key_Apostrophe: 52,
    QtCore.Qt.Key.Key_QuoteLeft: 53,
    QtCore.Qt.Key.Key_Comma: 54,
    QtCore.Qt.Key.Key_Period: 55,
    QtCore.Qt.Key.Key_Slash: 56,
    QtCore.Qt.Key.Key_CapsLock: 57,
    QtCore.Qt.Key.Key_F1: 58,
    QtCore.Qt.Key.Key_F2: 59,
    QtCore.Qt.Key.Key_F3: 60,
    QtCore.Qt.Key.Key_F4: 61,
    QtCore.Qt.Key.Key_F5: 62,
    QtCore.Qt.Key.Key_F6: 63,
    QtCore.Qt.Key.Key_F7: 64,
    QtCore.Qt.Key.Key_F8: 65,
    QtCore.Qt.Key.Key_F9: 66,
    QtCore.Qt.Key.Key_F10: 67,
    QtCore.Qt.Key.Key_F11: 68,
    QtCore.Qt.Key.Key_F12: 69,
    QtCore.Qt.Key.Key_Print: 70,
    QtCore.Qt.Key.Key_ScrollLock: 71,
    QtCore.Qt.Key.Key_Pause: 72,
    QtCore.Qt.Key.Key_Insert: 73,
    QtCore.Qt.Key.Key_Home: 74,
    QtCore.Qt.Key.Key_PageUp: 75,
    QtCore.Qt.Key.Key_Delete: 76,
    QtCore.Qt.Key.Key_End: 77,
    QtCore.Qt.Key.Key_PageDown: 78,
    QtCore.Qt.Key.Key_Right: 79,
    QtCore.Qt.Key.Key_Left: 80,
    QtCore.Qt.Key.Key_Down: 81,
    QtCore.Qt.Key.Key_Up: 82,
}
MODIFIER_MASKS = [
    (QtCore.Qt.KeyboardModifier.ControlModifier, 0x01),
    (QtCore.Qt.KeyboardModifier.ShiftModifier, 0x02),
    (QtCore.Qt.KeyboardModifier.AltModifier, 0x04),
    (QtCore.Qt.KeyboardModifier.MetaModifier, 0x08),
]
MOUSE_BUTTONS = [
    ("无", 0),
    ("左键", 1),
    ("右键", 2),
    ("中键", 4),
]
ZONE_NAME_MAP = {
    "leftTop": "左上角",
    "rightTop": "右上角",
    "rightBottom": "右下角",
    "leftBottom": "左下角",
    "threeLeft": "三指左滑",
    "threeRight": "三指右滑",
}


class TouchpadConfigUI(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("触摸板配置")
        self.client = SerialClient()
        self._build_ui()
        self._refresh_ports()

    def _build_ui(self):
        layout = QtWidgets.QVBoxLayout(self)

        conn_group = QtWidgets.QGroupBox("连接")
        conn_layout = QtWidgets.QHBoxLayout(conn_group)
        self.port_combo = QtWidgets.QComboBox()
        self.refresh_btn = QtWidgets.QPushButton("刷新")
        self.connect_btn = QtWidgets.QPushButton("连接")
        self.status_label = QtWidgets.QLabel("未连接")
        conn_layout.addWidget(self.port_combo)
        conn_layout.addWidget(self.refresh_btn)
        conn_layout.addWidget(self.connect_btn)
        conn_layout.addWidget(self.status_label)
        layout.addWidget(conn_group)

        scroll_area = QtWidgets.QScrollArea()
        scroll_area.setWidgetResizable(True)
        scroll_area.setFrameShape(QtWidgets.QFrame.NoFrame)
        scroll_container = QtWidgets.QWidget()
        scroll_layout = QtWidgets.QVBoxLayout(scroll_container)

        scroll_group = QtWidgets.QGroupBox("滚动")
        scroll_group_layout = QtWidgets.QFormLayout(scroll_group)
        self.scroll_sensitivity = QtWidgets.QDoubleSpinBox()
        self.scroll_sensitivity.setDecimals(8)
        self.scroll_sensitivity.setSingleStep(0.000001)
        self.scroll_sensitivity.setRange(0.0, 1.0)
        scroll_group_layout.addRow("滚动灵敏度", self.scroll_sensitivity)
        scroll_layout.addWidget(scroll_group)

        zone_group = QtWidgets.QGroupBox("区域")
        zone_layout = QtWidgets.QFormLayout(zone_group)
        self.top_percent = QtWidgets.QSpinBox()
        self.top_percent.setRange(5, 50)
        self.side_percent = QtWidgets.QSpinBox()
        self.side_percent.setRange(5, 50)
        self.enable_zones = QtWidgets.QCheckBox("启用区域功能")
        zone_layout.addRow("上边高度百分比", self.top_percent)
        zone_layout.addRow("左右宽度百分比", self.side_percent)
        zone_layout.addRow(self.enable_zones)
        scroll_layout.addWidget(zone_group)

        action_group = QtWidgets.QGroupBox("区域绑定")
        action_layout = QtWidgets.QFormLayout(action_group)
        self.left_top = self._create_zone_widgets()
        self.right_top = self._create_zone_widgets()
        self.right_bottom = self._create_zone_widgets()
        self.left_bottom = self._create_zone_widgets()
        self._add_zone_rows(action_layout, "leftTop", self.left_top)
        self._add_zone_rows(action_layout, "rightTop", self.right_top)
        self._add_zone_rows(action_layout, "rightBottom", self.right_bottom)
        self._add_zone_rows(action_layout, "leftBottom", self.left_bottom)
        scroll_layout.addWidget(action_group)

        swipe_group = QtWidgets.QGroupBox("三指滑动")
        swipe_layout = QtWidgets.QFormLayout(swipe_group)
        self.three_left = self._create_zone_widgets()
        self.three_right = self._create_zone_widgets()
        self._add_zone_rows(swipe_layout, "threeLeft", self.three_left)
        self._add_zone_rows(swipe_layout, "threeRight", self.three_right)
        self.three_threshold = QtWidgets.QSpinBox()
        self.three_threshold.setRange(50, 800)
        self.three_timeout = QtWidgets.QSpinBox()
        self.three_timeout.setRange(50, 1000)
        self.three_cooldown = QtWidgets.QSpinBox()
        self.three_cooldown.setRange(0, 2000)
        swipe_layout.addRow("滑动阈值", self.three_threshold)
        swipe_layout.addRow("滑动超时(ms)", self.three_timeout)
        swipe_layout.addRow("冷却时间(ms)", self.three_cooldown)
        scroll_layout.addWidget(swipe_group)

        ops_layout = QtWidgets.QHBoxLayout()
        self.apply_btn = QtWidgets.QPushButton("应用")
        self.refresh_values_btn = QtWidgets.QPushButton("读取当前值")
        self.save_btn = QtWidgets.QPushButton("保存")
        self.load_btn = QtWidgets.QPushButton("加载")
        self.reset_btn = QtWidgets.QPushButton("恢复默认")
        ops_layout.addWidget(self.apply_btn)
        ops_layout.addWidget(self.refresh_values_btn)
        ops_layout.addWidget(self.save_btn)
        ops_layout.addWidget(self.load_btn)
        ops_layout.addWidget(self.reset_btn)
        scroll_layout.addLayout(ops_layout)

        scroll_layout.addStretch(1)
        scroll_area.setWidget(scroll_container)
        layout.addWidget(scroll_area)

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

    def _record_shortcut(self, widgets):
        dialog = QtWidgets.QDialog(self)
        dialog.setWindowTitle("录制快捷键")
        dialog.setModal(True)
        layout = QtWidgets.QVBoxLayout(dialog)
        label = QtWidgets.QLabel("请按下要绑定的快捷键（Esc 取消）。")
        layout.addWidget(label)

        dialog.keyPressEvent = lambda event: self._handle_key_capture(event, dialog, widgets)
        dialog.exec()

    def _handle_key_capture(self, event, dialog, widgets):
        if event.key() == QtCore.Qt.Key.Key_Escape:
            dialog.reject()
            return
        if event.key() in (
            QtCore.Qt.Key.Key_Shift,
            QtCore.Qt.Key.Key_Control,
            QtCore.Qt.Key.Key_Alt,
            QtCore.Qt.Key.Key_Meta,
        ):
            return
        hid_key = HID_KEY_MAP.get(event.key())
        if hid_key is None:
            QtWidgets.QMessageBox.warning(self, "不支持",
                                          "该按键不支持录制。")
            dialog.reject()
            return
        modifier = 0
        mods = event.modifiers()
        for qt_mod, mask in MODIFIER_MASKS:
            if mods & qt_mod:
                modifier |= mask
        widgets["type"].setCurrentText("KEYBOARD")
        widgets["modifier_value"] = modifier
        widgets["key_value"] = hid_key
        self._update_key_display(widgets)
        dialog.accept()

    def _refresh_ports(self):
        ports = SerialClient.list_ports()
        self.port_combo.clear()
        self.port_combo.addItems(ports)
        if not ports:
            self.status_label.setText("无串口")

    def _toggle_connection(self):
        if self.client.is_connected():
            self.client.disconnect()
            self.status_label.setText("未连接")
            self.connect_btn.setText("连接")
            return
        port = self.port_combo.currentText()
        if not port:
            self._log("未选择串口")
            return
        try:
            self.client.connect(port)
        except Exception as exc:
            self._log(f"连接失败：{exc}")
            return
        self.status_label.setText("已连接")
        self.connect_btn.setText("断开")
        self._refresh_values()

    def _log(self, text):
        self.log.appendPlainText(text)

    def _refresh_values(self):
        if not self.client.is_connected():
            self._log("未连接")
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
        self._apply_zone_values(self.three_left, data, "threeLeft")
        self._apply_zone_values(self.three_right, data, "threeRight")
        if "threeSwipeThreshold" in data:
            self.three_threshold.setValue(int(data["threeSwipeThreshold"]))
        if "threeSwipeTimeout" in data:
            self.three_timeout.setValue(int(data["threeSwipeTimeout"]))
        if "threeSwipeCooldown" in data:
            self.three_cooldown.setValue(int(data["threeSwipeCooldown"]))
        self._log("已读取当前值")

    def _set_combo(self, combo, value):
        value = value.strip().upper()
        if value in ZONE_TYPES:
            combo.setCurrentText(value)

    def _create_zone_widgets(self):
        type_combo = QtWidgets.QComboBox()
        type_combo.addItems(ZONE_TYPES)
        buttons = QtWidgets.QComboBox()
        for label, value in MOUSE_BUTTONS:
            buttons.addItem(label, value)
        record_btn = QtWidgets.QPushButton("录制")
        key_display = QtWidgets.QLineEdit()
        key_display.setReadOnly(True)
        key_display.setPlaceholderText("未绑定")
        return {
            "type": type_combo,
            "buttons": buttons,
            "key_display": key_display,
            "record": record_btn,
            "modifier_value": 0,
            "key_value": 0,
        }

    def _add_zone_rows(self, layout, name, widgets):
        label = ZONE_NAME_MAP.get(name, name)
        layout.addRow(f"{label} 类型", widgets["type"])
        layout.addRow(f"{label} 鼠标按钮", widgets["buttons"])
        layout.addRow(f"{label} 绑定结果", widgets["key_display"])
        layout.addRow(f"{label} 录制快捷键", widgets["record"])
        widgets["record"].clicked.connect(lambda _=False, w=widgets: self._record_shortcut(w))
        widgets["type"].currentTextChanged.connect(
            lambda _=None, w=widgets: self._update_zone_controls(w)
        )
        self._update_zone_controls(widgets)

    def _apply_zone_values(self, widgets, data, name):
        type_key = f"{name}Type"
        buttons_key = f"{name}Buttons"
        modifier_key = f"{name}Modifier"
        key_key = f"{name}Key"
        if type_key in data:
            self._set_combo(widgets["type"], data[type_key])
        if buttons_key in data:
            self._set_mouse_buttons(widgets["buttons"], int(data[buttons_key]))
        if modifier_key in data:
            widgets["modifier_value"] = int(data[modifier_key])
        if key_key in data:
            widgets["key_value"] = int(data[key_key])
        self._update_key_display(widgets)

    def _apply(self):
        if not self.client.is_connected():
            self._log("未连接")
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
        cmds += self._zone_set_cmds("threeLeft", self.three_left)
        cmds += self._zone_set_cmds("threeRight", self.three_right)
        cmds.append(f"SET threeSwipeThreshold {self.three_threshold.value()}")
        cmds.append(f"SET threeSwipeTimeout {self.three_timeout.value()}")
        cmds.append(f"SET threeSwipeCooldown {self.three_cooldown.value()}")
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
            self._log("未连接")
            return
        for line in self.client.send(cmd):
            self._log(line)

    def _zone_set_cmds(self, name, widgets):
        return [
            f"SET {name}Type {widgets['type'].currentText()}",
            f"SET {name}Buttons {widgets['buttons'].currentData()}",
            f"SET {name}Modifier {widgets['modifier_value']}",
            f"SET {name}Key {widgets['key_value']}",
        ]

    def _update_zone_controls(self, widgets):
        zone_type = widgets["type"].currentText()
        widgets["buttons"].setEnabled(zone_type == "MOUSE")
        widgets["record"].setEnabled(zone_type == "KEYBOARD")
        widgets["key_display"].setEnabled(zone_type == "KEYBOARD")

    def _update_key_display(self, widgets):
        keycode = widgets["key_value"]
        modifier = widgets["modifier_value"]
        if keycode == 0:
            widgets["key_display"].setText("未绑定")
            return
        parts = []
        if modifier & 0x01:
            parts.append("Ctrl")
        if modifier & 0x02:
            parts.append("Shift")
        if modifier & 0x04:
            parts.append("Alt")
        if modifier & 0x08:
            parts.append("Win")
        key_name = self._key_name_from_code(keycode)
        parts.append(key_name)
        widgets["key_display"].setText("+".join(parts))

    def _key_name_from_code(self, keycode):
        for qt_key, hid in HID_KEY_MAP.items():
            if hid == keycode:
                return qt_key.name.replace("Key_", "")
        return f"KEY_{keycode}"

    def _set_mouse_buttons(self, combo, value):
        for i in range(combo.count()):
            if combo.itemData(i) == value:
                combo.setCurrentIndex(i)
                return


def main():
    app = QtWidgets.QApplication(sys.argv)
    ui = TouchpadConfigUI()
    ui.resize(520, 640)
    ui.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
