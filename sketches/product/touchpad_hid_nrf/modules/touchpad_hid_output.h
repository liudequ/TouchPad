void sendMouseMove(int8_t x, int8_t y) {
  if (!useBleTransport()) {
    uint8_t report[5] = { 0, (uint8_t)x, (uint8_t)y, 0, 0 };
    usb_hid.sendReport(RID_MOUSE, report, sizeof(report));
    return;
  }
  if (!Bluefruit.connected()) return;
  blehid.mouseReport((uint8_t)0, (int8_t)x, (int8_t)y, (int8_t)0, (int8_t)0);
}

void sendMouseWheel(int8_t wheel) {
  if (!useBleTransport()) {
    uint8_t report[5] = { 0, 0, 0, (uint8_t)wheel, 0 };
    usb_hid.sendReport(RID_MOUSE, report, sizeof(report));
    return;
  }
  if (!Bluefruit.connected()) return;
  blehid.mouseReport((uint8_t)0, (int8_t)0, (int8_t)0, wheel, (int8_t)0);
}

void sendMouseClick(uint8_t buttons) {
  if (!useBleTransport()) {
    uint8_t report[5] = { buttons, 0, 0, 0, 0 };
    usb_hid.sendReport(RID_MOUSE, report, sizeof(report));
    delay(5);
    report[0] = 0;
    usb_hid.sendReport(RID_MOUSE, report, sizeof(report));
    return;
  }
  if (!Bluefruit.connected()) return;
  blehid.mouseReport((uint8_t)buttons, (int8_t)0, (int8_t)0, (int8_t)0, (int8_t)0);
  delay(5);
  blehid.mouseReport((uint8_t)0, (int8_t)0, (int8_t)0, (int8_t)0, (int8_t)0);
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
