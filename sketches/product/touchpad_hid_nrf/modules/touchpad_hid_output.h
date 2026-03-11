void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
  if (!useBleTransport()) {
    uint8_t report[5] = { buttons, (uint8_t)x, (uint8_t)y, (uint8_t)wheel, 0 };
    usb_hid.sendReport(RID_MOUSE, report, sizeof(report));
    return;
  }
  if (!Bluefruit.connected()) return;
  blehid.mouseReport(buttons, x, y, wheel, (int8_t)0);
}

void sendMouseMove(int8_t x, int8_t y) {
  sendMouseReport((uint8_t)0, x, y, (int8_t)0);
}

void sendMouseMoveWithButtons(int8_t x, int8_t y, uint8_t buttons) {
  sendMouseReport(buttons, x, y, (int8_t)0);
}

void sendMouseWheel(int8_t wheel) {
  sendMouseReport((uint8_t)0, (int8_t)0, (int8_t)0, wheel);
}

void sendMouseButtons(uint8_t buttons) {
  sendMouseReport(buttons, (int8_t)0, (int8_t)0, (int8_t)0);
}

void sendMouseClick(uint8_t buttons) {
  sendMouseButtons(buttons);
  delay(5);
  sendMouseButtons((uint8_t)0);
}

void sendKeyboard(uint8_t modifier, uint8_t keycode) {
  if (!useBleTransport()) {
    uint8_t report[8] = { modifier, 0, keycode, 0, 0, 0, 0, 0 };
    usb_hid.sendReport(RID_KEYBOARD, report, sizeof(report));
    delay(5);
    for (uint8_t i = 0; i < sizeof(report); i++) report[i] = 0;
    usb_hid.sendReport(RID_KEYBOARD, report, sizeof(report));
    return;
  }
  if (!Bluefruit.connected()) return;
  uint8_t keys[6] = { keycode, 0, 0, 0, 0, 0 };
  blehid.keyboardReport(modifier, keys);
  delay(5);
  blehid.keyRelease();
}
