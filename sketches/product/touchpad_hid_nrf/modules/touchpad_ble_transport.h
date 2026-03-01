void startAdv() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_MOUSE);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.addService(blebas);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void updateTransport() {
  bool usbMounted = isUsbMounted();
  if (usbMounted == lastUsbMounted) return;
  lastUsbMounted = usbMounted;
  resetGestureState();
  if (usbMounted) {
    waitFingerReleaseAfterReconnect = true;
    bool leavingIdle = (powerStage != POWER_ACTIVE) || advSuppressedByIdle;
    advSuppressedByIdle = false;
    powerStage = POWER_ACTIVE;
    if (leavingIdle) {
      setLedPatternActive();
    }
  }

  if (usbMounted) {
    if (!useBleWhenUsb && Bluefruit.connected()) {
      Bluefruit.disconnect(0);
      delay(50);
    }
    if (!useBleWhenUsb) {
      Bluefruit.Advertising.stop();
    } else if (!advSuppressedByIdle) {
      startAdv();
    }
  } else if (!advSuppressedByIdle) {
    startAdv();
  }
}

void initBle() {
  Bluefruit.configPrphConn(kBleMtuMax, kBleEventLen, kBleHvnQueueSize, kBleWrCmdQueueSize);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("NiceTouchPad");
  Bluefruit.autoConnLed(false);
  // Request a faster, steadier connection interval (7.5–11.25 ms).
  Bluefruit.Periph.setConnInterval(6, 9);

  Bluefruit.Periph.setConnectCallback(onConnect);
  Bluefruit.Periph.setDisconnectCallback(onDisconnect);
  Bluefruit.Security.setIOCaps(false, false, false);

  bledis.setManufacturer("TouchPad");
  bledis.setModel("NRF52840");
  bledis.begin();

  blebas.begin();
  blehid.begin();
  refreshBatteryStatus(true);
  startAdv();
}
