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
    "threeUp": "三指上滑",
    "threeDown": "三指下滑",
    "threeTap": "三指单击",
    "threeDoubleTap": "三指双击",
    "fourLeft": "四指左滑",
    "fourRight": "四指右滑",
    "fourUp": "四指上滑",
    "fourDown": "四指下滑",
    "fourTap": "四指单击",
    "fourDoubleTap": "四指双击",
}


class TouchpadConfigUI(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("触摸板配置")
        self.serial_client = SerialClient()
        self.client = None
        self._build_ui()
        self._refresh_targets()

    def _build_ui(self):
        layout = QtWidgets.QVBoxLayout(self)

        conn_group = QtWidgets.QGroupBox("连接")
        conn_layout = QtWidgets.QHBoxLayout(conn_group)
        self.conn_type_label = QtWidgets.QLabel("USB串口")
        self.port_combo = QtWidgets.QComboBox()
        self.refresh_btn = QtWidgets.QPushButton("刷新")
        self.connect_btn = QtWidgets.QPushButton("连接")
        self.status_label = QtWidgets.QLabel("未连接")
        conn_layout.addWidget(self.conn_type_label)
        conn_layout.addWidget(self.port_combo)
        conn_layout.addWidget(self.refresh_btn)
        conn_layout.addWidget(self.connect_btn)
        conn_layout.addWidget(self.status_label)
        layout.addWidget(conn_group)

        self.ble_hint = QtWidgets.QLabel(
            "提示：当前 nRF 固件已移除 BLE 调参，仅支持 USB 串口调参。"
        )
        self.ble_hint.setWordWrap(True)
        self.ble_hint.setVisible(True)
        layout.addWidget(self.ble_hint)

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
        self.scroll_low_speed_boost_end = QtWidgets.QDoubleSpinBox()
        self.scroll_low_speed_boost_end.setDecimals(3)
        self.scroll_low_speed_boost_end.setSingleStep(0.01)
        self.scroll_low_speed_boost_end.setRange(0.001, 2.0)
        scroll_group_layout.addRow("低速扶正终点", self.scroll_low_speed_boost_end)
        self.scroll_min_active_speed = QtWidgets.QDoubleSpinBox()
        self.scroll_min_active_speed.setDecimals(3)
        self.scroll_min_active_speed.setSingleStep(0.01)
        self.scroll_min_active_speed.setRange(0.0, 2.0)
        scroll_group_layout.addRow("最小滚动速度", self.scroll_min_active_speed)
        self.scroll_accel_start_speed = QtWidgets.QDoubleSpinBox()
        self.scroll_accel_start_speed.setDecimals(3)
        self.scroll_accel_start_speed.setSingleStep(0.01)
        self.scroll_accel_start_speed.setRange(0.0, 2.0)
        scroll_group_layout.addRow("滚动加速起点", self.scroll_accel_start_speed)
        self.scroll_accel_factor = QtWidgets.QDoubleSpinBox()
        self.scroll_accel_factor.setDecimals(3)
        self.scroll_accel_factor.setSingleStep(0.05)
        self.scroll_accel_factor.setRange(0.0, 5.0)
        scroll_group_layout.addRow("滚动加速系数", self.scroll_accel_factor)
        self.scroll_max_accel = QtWidgets.QDoubleSpinBox()
        self.scroll_max_accel.setDecimals(2)
        self.scroll_max_accel.setSingleStep(0.1)
        self.scroll_max_accel.setRange(1.0, 10.0)
        scroll_group_layout.addRow("滚动最大加速", self.scroll_max_accel)
        scroll_layout.addWidget(scroll_group)

        single_group = QtWidgets.QGroupBox("单指")
        single_layout = QtWidgets.QFormLayout(single_group)
        self.sensitivity = QtWidgets.QDoubleSpinBox()
        self.sensitivity.setDecimals(3)
        self.sensitivity.setSingleStep(0.01)
        self.sensitivity.setRange(0.01, 5.0)
        single_layout.addRow("灵敏度", self.sensitivity)
        self.smooth_factor = QtWidgets.QDoubleSpinBox()
        self.smooth_factor.setDecimals(3)
        self.smooth_factor.setSingleStep(0.01)
        self.smooth_factor.setRange(0.0, 1.0)
        single_layout.addRow("平滑系数", self.smooth_factor)
        self.accel_factor = QtWidgets.QDoubleSpinBox()
        self.accel_factor.setDecimals(3)
        self.accel_factor.setSingleStep(0.01)
        self.accel_factor.setRange(0.0, 1.0)
        single_layout.addRow("加速系数", self.accel_factor)
        self.max_accel = QtWidgets.QDoubleSpinBox()
        self.max_accel.setDecimals(2)
        self.max_accel.setSingleStep(0.1)
        self.max_accel.setRange(0.0, 10.0)
        single_layout.addRow("最大加速", self.max_accel)
        self.max_delta = QtWidgets.QSpinBox()
        self.max_delta.setRange(1, 200)
        single_layout.addRow("单次最大位移", self.max_delta)
        self.move_deadband = QtWidgets.QSpinBox()
        self.move_deadband.setRange(0, 20)
        single_layout.addRow("移动死区", self.move_deadband)
        scroll_layout.addWidget(single_group)

        rate_group = QtWidgets.QGroupBox("上报频率")
        rate_layout = QtWidgets.QFormLayout(rate_group)
        self.report_rate = QtWidgets.QSpinBox()
        self.report_rate.setRange(10, 200)
        self.report_rate.setValue(120)
        self.report_rate.setSuffix(" Hz")
        rate_layout.addRow("频率(写入)", self.report_rate)
        scroll_layout.addWidget(rate_group)

        power_group = QtWidgets.QGroupBox("蓝牙/省电")
        power_layout = QtWidgets.QFormLayout(power_group)
        self.use_ble_when_usb = QtWidgets.QCheckBox("USB 连接时仍允许 BLE")
        power_layout.addRow(self.use_ble_when_usb)
        self.ble_idle_sleep_enabled = QtWidgets.QCheckBox("启用空闲省电")
        power_layout.addRow(self.ble_idle_sleep_enabled)
        self.ble_idle_light_ms = QtWidgets.QSpinBox()
        self.ble_idle_light_ms.setRange(1000, 3600000)
        self.ble_idle_light_ms.setSingleStep(1000)
        self.ble_idle_light_ms.setSuffix(" ms")
        self.ble_idle_light_ms.setValue(600000)
        power_layout.addRow("第一阶段触发时长", self.ble_idle_light_ms)
        self.ble_idle_medium_ms = QtWidgets.QSpinBox()
        self.ble_idle_medium_ms.setRange(1000, 3600000)
        self.ble_idle_medium_ms.setSingleStep(1000)
        self.ble_idle_medium_ms.setSuffix(" ms")
        self.ble_idle_medium_ms.setValue(1800000)
        power_layout.addRow("第二阶段触发时长", self.ble_idle_medium_ms)
        self.ble_idle_sleep_ms = QtWidgets.QSpinBox()
        self.ble_idle_sleep_ms.setRange(1000, 3600000)
        self.ble_idle_sleep_ms.setSingleStep(1000)
        self.ble_idle_sleep_ms.setSuffix(" ms")
        self.ble_idle_sleep_ms.setValue(3600000)
        power_layout.addRow("第三阶段触发时长", self.ble_idle_sleep_ms)
        self.light_idle_rate = QtWidgets.QSpinBox()
        self.light_idle_rate.setRange(5, 120)
        self.light_idle_rate.setSuffix(" Hz")
        power_layout.addRow("轻度空闲频率", self.light_idle_rate)
        scroll_layout.addWidget(power_group)

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
        self.three_up = self._create_zone_widgets()
        self.three_down = self._create_zone_widgets()
        self._add_zone_rows(swipe_layout, "threeLeft", self.three_left)
        self._add_zone_rows(swipe_layout, "threeRight", self.three_right)
        self._add_zone_rows(swipe_layout, "threeUp", self.three_up)
        self._add_zone_rows(swipe_layout, "threeDown", self.three_down)
        self.three_tap = self._create_zone_widgets()
        self.three_double_tap = self._create_zone_widgets()
        self._add_zone_rows(swipe_layout, "threeTap", self.three_tap)
        self._add_zone_rows(swipe_layout, "threeDoubleTap", self.three_double_tap)
        self.three_threshold = QtWidgets.QSpinBox()
        self.three_threshold.setRange(50, 800)
        self.three_threshold_y = QtWidgets.QSpinBox()
        self.three_threshold_y.setRange(50, 800)
        self.three_timeout = QtWidgets.QSpinBox()
        self.three_timeout.setRange(50, 1000)
        self.three_cooldown = QtWidgets.QSpinBox()
        self.three_cooldown.setRange(0, 2000)
        swipe_layout.addRow("水平阈值", self.three_threshold)
        swipe_layout.addRow("垂直阈值", self.three_threshold_y)
        swipe_layout.addRow("滑动超时(ms)", self.three_timeout)
        swipe_layout.addRow("冷却时间(ms)", self.three_cooldown)
        scroll_layout.addWidget(swipe_group)

        four_group = QtWidgets.QGroupBox("四指手势")
        four_layout = QtWidgets.QFormLayout(four_group)
        self.four_left = self._create_zone_widgets()
        self.four_right = self._create_zone_widgets()
        self.four_up = self._create_zone_widgets()
        self.four_down = self._create_zone_widgets()
        self.four_tap = self._create_zone_widgets()
        self.four_double_tap = self._create_zone_widgets()
        self._add_zone_rows(four_layout, "fourLeft", self.four_left)
        self._add_zone_rows(four_layout, "fourRight", self.four_right)
        self._add_zone_rows(four_layout, "fourUp", self.four_up)
        self._add_zone_rows(four_layout, "fourDown", self.four_down)
        self._add_zone_rows(four_layout, "fourTap", self.four_tap)
        self._add_zone_rows(four_layout, "fourDoubleTap", self.four_double_tap)
        self.four_threshold = QtWidgets.QSpinBox()
        self.four_threshold.setRange(50, 800)
        self.four_threshold_y = QtWidgets.QSpinBox()
        self.four_threshold_y.setRange(50, 800)
        self.four_timeout = QtWidgets.QSpinBox()
        self.four_timeout.setRange(50, 1000)
        self.four_cooldown = QtWidgets.QSpinBox()
        self.four_cooldown.setRange(0, 2000)
        four_layout.addRow("水平阈值", self.four_threshold)
        four_layout.addRow("垂直阈值", self.four_threshold_y)
        four_layout.addRow("滑动超时(ms)", self.four_timeout)
        four_layout.addRow("冷却时间(ms)", self.four_cooldown)
        scroll_layout.addWidget(four_group)

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

        self.refresh_btn.clicked.connect(self._refresh_targets)
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
        dialog.setWindowModality(QtCore.Qt.ApplicationModal)
        dialog.setWindowFlag(QtCore.Qt.WindowStaysOnTopHint, True)
        layout = QtWidgets.QVBoxLayout(dialog)
        label = QtWidgets.QLabel("请按下要绑定的快捷键（Esc 取消）。")
        layout.addWidget(label)

        dialog.keyPressEvent = lambda event: self._handle_key_capture(event, dialog, widgets)
        dialog.grabKeyboard()
        dialog.activateWindow()
        dialog.exec()
        dialog.releaseKeyboard()

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

    def _current_client(self):
        return self.serial_client

    def _refresh_targets(self):
        self.port_combo.clear()
        ports = SerialClient.list_ports()
        self.port_combo.addItems(ports)
        if not ports:
            self.status_label.setText("无串口")

    def _toggle_connection(self):
        if self.client and self.client.is_connected():
            self.client.disconnect()
            self.status_label.setText("未连接")
            self.connect_btn.setText("连接")
            return
        target = self.port_combo.currentData()
        if not target:
            target = self.port_combo.currentText()
        if not target:
            self._log("未选择设备")
            return
        client = self._current_client()
        if not client:
            self._log("连接方式不可用")
            return
        try:
            client.connect(target)
        except Exception as exc:
            self._log(f"连接失败：{exc}")
            return
        self.client = client
        self.status_label.setText("已连接")
        self.connect_btn.setText("断开")
        self._refresh_values()

    def _log(self, text):
        self.log.appendPlainText(text)

    def _refresh_values(self):
        if not self.client or not self.client.is_connected():
            self._log("未连接")
            return
        data = self.client.get_all()
        if "scrollSensitivity" in data:
            self.scroll_sensitivity.setValue(float(data["scrollSensitivity"]))
        if "scrollLowSpeedBoostEnd" in data:
            self.scroll_low_speed_boost_end.setValue(float(data["scrollLowSpeedBoostEnd"]))
        if "scrollMinActiveSpeed" in data:
            self.scroll_min_active_speed.setValue(float(data["scrollMinActiveSpeed"]))
        if "scrollAccelStartSpeed" in data:
            self.scroll_accel_start_speed.setValue(float(data["scrollAccelStartSpeed"]))
        if "scrollAccelFactor" in data:
            self.scroll_accel_factor.setValue(float(data["scrollAccelFactor"]))
        if "scrollMaxAccel" in data:
            self.scroll_max_accel.setValue(float(data["scrollMaxAccel"]))
        if "sensitivity" in data:
            self.sensitivity.setValue(float(data["sensitivity"]))
        if "smoothFactor" in data:
            self.smooth_factor.setValue(float(data["smoothFactor"]))
        if "accelFactor" in data:
            self.accel_factor.setValue(float(data["accelFactor"]))
        if "maxAccel" in data:
            self.max_accel.setValue(float(data["maxAccel"]))
        if "maxDelta" in data:
            self.max_delta.setValue(int(data["maxDelta"]))
        if "moveDeadband" in data:
            self.move_deadband.setValue(int(data["moveDeadband"]))
        if "rate" in data:
            self.report_rate.setValue(int(data["rate"]))
        if "useBleWhenUsb" in data:
            self.use_ble_when_usb.setChecked(data["useBleWhenUsb"] == "1")
        if "bleIdleSleepEnabled" in data:
            self.ble_idle_sleep_enabled.setChecked(data["bleIdleSleepEnabled"] == "1")
        if "bleIdleLightMs" in data:
            self.ble_idle_light_ms.setValue(int(data["bleIdleLightMs"]))
        if "bleIdleMediumMs" in data:
            self.ble_idle_medium_ms.setValue(int(data["bleIdleMediumMs"]))
        if "bleIdleSleepMs" in data:
            self.ble_idle_sleep_ms.setValue(int(data["bleIdleSleepMs"]))
        if "lightIdleRate" in data:
            self.light_idle_rate.setValue(int(data["lightIdleRate"]))
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
        self._apply_zone_values(self.three_up, data, "threeUp")
        self._apply_zone_values(self.three_down, data, "threeDown")
        self._apply_zone_values(self.three_tap, data, "threeTap")
        self._apply_zone_values(self.three_double_tap, data, "threeDoubleTap")
        if "threeSwipeThresholdX" in data:
            self.three_threshold.setValue(int(data["threeSwipeThresholdX"]))
        if "threeSwipeThresholdY" in data:
            self.three_threshold_y.setValue(int(data["threeSwipeThresholdY"]))
        if "threeSwipeTimeout" in data:
            self.three_timeout.setValue(int(data["threeSwipeTimeout"]))
        if "threeSwipeCooldown" in data:
            self.three_cooldown.setValue(int(data["threeSwipeCooldown"]))
        self._apply_zone_values(self.four_left, data, "fourLeft")
        self._apply_zone_values(self.four_right, data, "fourRight")
        self._apply_zone_values(self.four_up, data, "fourUp")
        self._apply_zone_values(self.four_down, data, "fourDown")
        self._apply_zone_values(self.four_tap, data, "fourTap")
        self._apply_zone_values(self.four_double_tap, data, "fourDoubleTap")
        if "fourSwipeThresholdX" in data:
            self.four_threshold.setValue(int(data["fourSwipeThresholdX"]))
        if "fourSwipeThresholdY" in data:
            self.four_threshold_y.setValue(int(data["fourSwipeThresholdY"]))
        if "fourSwipeTimeout" in data:
            self.four_timeout.setValue(int(data["fourSwipeTimeout"]))
        if "fourSwipeCooldown" in data:
            self.four_cooldown.setValue(int(data["fourSwipeCooldown"]))
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
        if not self.client or not self.client.is_connected():
            self._log("未连接")
            return
        cmds = [
            f"SET scrollSensitivity {self.scroll_sensitivity.value():.8f}",
            f"SET sensitivity {self.sensitivity.value():.3f}",
            f"SET smoothFactor {self.smooth_factor.value():.3f}",
            f"SET accelFactor {self.accel_factor.value():.3f}",
            f"SET maxAccel {self.max_accel.value():.2f}",
            f"SET maxDelta {self.max_delta.value()}",
            f"SET moveDeadband {self.move_deadband.value()}",
            f"SET topZonePercent {self.top_percent.value()}",
            f"SET sideZonePercent {self.side_percent.value()}",
            f"SET enableNavZones {1 if self.enable_zones.isChecked() else 0}",
            f"SET rate {self.report_rate.value()}",
            f"SET scrollLowSpeedBoostEnd {self.scroll_low_speed_boost_end.value():.3f}",
            f"SET scrollMinActiveSpeed {self.scroll_min_active_speed.value():.3f}",
            f"SET scrollAccelStartSpeed {self.scroll_accel_start_speed.value():.3f}",
            f"SET scrollAccelFactor {self.scroll_accel_factor.value():.3f}",
            f"SET scrollMaxAccel {self.scroll_max_accel.value():.2f}",
            f"SET useBleWhenUsb {1 if self.use_ble_when_usb.isChecked() else 0}",
            f"SET bleIdleSleepEnabled {1 if self.ble_idle_sleep_enabled.isChecked() else 0}",
            f"SET bleIdleLightMs {self.ble_idle_light_ms.value()}",
            f"SET bleIdleMediumMs {self.ble_idle_medium_ms.value()}",
            f"SET bleIdleSleepMs {self.ble_idle_sleep_ms.value()}",
            f"SET lightIdleRate {self.light_idle_rate.value()}",
        ]
        cmds += self._zone_set_cmds("leftTop", self.left_top)
        cmds += self._zone_set_cmds("rightTop", self.right_top)
        cmds += self._zone_set_cmds("rightBottom", self.right_bottom)
        cmds += self._zone_set_cmds("leftBottom", self.left_bottom)
        cmds += self._zone_set_cmds("threeLeft", self.three_left)
        cmds += self._zone_set_cmds("threeRight", self.three_right)
        cmds += self._zone_set_cmds("threeUp", self.three_up)
        cmds += self._zone_set_cmds("threeDown", self.three_down)
        cmds += self._zone_set_cmds("threeTap", self.three_tap)
        cmds += self._zone_set_cmds("threeDoubleTap", self.three_double_tap)
        cmds.append(f"SET threeSwipeThresholdX {self.three_threshold.value()}")
        cmds.append(f"SET threeSwipeThresholdY {self.three_threshold_y.value()}")
        cmds.append(f"SET threeSwipeTimeout {self.three_timeout.value()}")
        cmds.append(f"SET threeSwipeCooldown {self.three_cooldown.value()}")
        cmds += self._zone_set_cmds("fourLeft", self.four_left)
        cmds += self._zone_set_cmds("fourRight", self.four_right)
        cmds += self._zone_set_cmds("fourUp", self.four_up)
        cmds += self._zone_set_cmds("fourDown", self.four_down)
        cmds += self._zone_set_cmds("fourTap", self.four_tap)
        cmds += self._zone_set_cmds("fourDoubleTap", self.four_double_tap)
        cmds.append(f"SET fourSwipeThresholdX {self.four_threshold.value()}")
        cmds.append(f"SET fourSwipeThresholdY {self.four_threshold_y.value()}")
        cmds.append(f"SET fourSwipeTimeout {self.four_timeout.value()}")
        cmds.append(f"SET fourSwipeCooldown {self.four_cooldown.value()}")
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
        if not self.client or not self.client.is_connected():
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
