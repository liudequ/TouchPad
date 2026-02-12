void updateConnectionState() {
  bool bleConnected = Bluefruit.connected();
  if (bleConnected != lastBleConnected) {
    lastBleConnected = bleConnected;
    resetGestureState();
    if (bleConnected) {
      waitFingerReleaseAfterReconnect = true;
    }
  }
}

void normalizeIdleThresholds() {
  if (bleIdleLightMs < 1000) bleIdleLightMs = 1000;
  if (bleIdleMediumMs <= bleIdleLightMs) bleIdleMediumMs = bleIdleLightMs + 1000;
  if (bleIdleSleepMs <= bleIdleMediumMs) bleIdleSleepMs = bleIdleMediumMs + 1000;
  if (lightIdleReportIntervalMs < reportIntervalMs) lightIdleReportIntervalMs = reportIntervalMs;
}

uint32_t effectiveReportIntervalMs() {
  if (powerStage == POWER_IDLE_LIGHT || powerStage == POWER_IDLE_MEDIUM) {
    return max(reportIntervalMs, lightIdleReportIntervalMs);
  }
  return reportIntervalMs;
}

void leaveIdlePowerStage() {
  if (powerStage == POWER_ACTIVE && !advSuppressedByIdle) return;
  powerStage = POWER_ACTIVE;
  advSuppressedByIdle = false;
  setLedPatternActive();
  unsigned long now = millis();
  lastReportMs = now;
  lastScrollReportMs = now;

  if (useBleTransport() && !Bluefruit.connected()) {
    startAdv();
  }
}

void enterIdleLightStage() {
  if (powerStage >= POWER_IDLE_LIGHT) return;
  powerStage = POWER_IDLE_LIGHT;
  setLedPatternIdleLight();
  if (!Bluefruit.connected()) {
    Bluefruit.Advertising.stop();
    advSuppressedByIdle = true;
  }
}

void enterIdleMediumStage() {
  if (powerStage >= POWER_IDLE_MEDIUM) return;
  powerStage = POWER_IDLE_MEDIUM;
  setLedPatternIdleMedium();
  if (Bluefruit.connected()) {
    Bluefruit.disconnect(0);
    delay(50);
  }
  Bluefruit.Advertising.stop();
  advSuppressedByIdle = true;
}

void handleIdlePower(unsigned long now) {
  if (!bleIdleSleepEnabled || !useBleTransport()) {
    leaveIdlePowerStage();
    return;
  }

  unsigned long idleMs = now - lastActivityMs;
  if (idleMs >= bleIdleSleepMs) {
    enterDeepSleep();
    return;
  }
  if (idleMs >= bleIdleMediumMs) {
    enterIdleMediumStage();
    return;
  }
  if (idleMs >= bleIdleLightMs) {
    enterIdleLightStage();
    return;
  }
  leaveIdlePowerStage();
}

void markActivity() {
  lastActivityMs = millis();
  leaveIdlePowerStage();
}

uint16_t readBatteryMilliVolts() {
#if defined(ARDUINO_ARCH_NRF52) && defined(SAADC_CH_PSELP_PSELP_VDDHDIV5)
  analogReference(AR_INTERNAL_3_0);
  analogReadResolution(12);
  delay(1);
  uint32_t raw = analogReadVDDHDIV5();
  analogReference(AR_DEFAULT);
  analogReadResolution(10);

  // VDDH/5, 12-bit, 0..3.0V reference.
  float mv = ((float)raw * 3000.0f / 4095.0f) * 5.0f;
  if (mv < 0.0f) mv = 0.0f;
  if (mv > 65535.0f) mv = 65535.0f;
  return (uint16_t)(mv + 0.5f);
#else
  return 0;
#endif
}

uint8_t batteryPercentFromMilliVolts(uint16_t mv) {
  if (mv <= 3300) return 0;
  if (mv < 3600) return (uint8_t)((mv - 3300) / 30);
  if (mv >= 4200) return 100;

  uint32_t delta = (uint32_t)(mv - 3600);
  uint8_t percent = (uint8_t)(10 + (delta * 90UL) / 600UL);
  if (percent > 100) percent = 100;
  return percent;
}

void refreshBatteryStatus(bool forceNotify) {
  uint16_t mv = readBatteryMilliVolts();
  if (mv == 0) return;
  uint8_t percent = batteryPercentFromMilliVolts(mv);

  bool changed = (!batteryReady) || (percent != batteryPercent);
  batteryMilliVolts = mv;
  batteryPercent = percent;
  batteryReady = true;

  if (!changed && !forceNotify) return;
  blebas.write(batteryPercent);
  if (Bluefruit.connected()) {
    blebas.notify(batteryPercent);
  }
}

void handleBattery(unsigned long now) {
  if (!batteryReady) {
    lastBatterySampleMs = now;
    refreshBatteryStatus(false);
    return;
  }
  if (now - lastBatterySampleMs < kBatterySampleIntervalMs) return;
  lastBatterySampleMs = now;
  refreshBatteryStatus(false);
}
