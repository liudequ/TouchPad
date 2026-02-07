#include <bluefruit.h>

BLEHidAdafruit blehid;
BLEDis bledis;

// SoftDevice connection config (tune to reduce notify blocking)
static const uint16_t kMtuMax = BLE_GATT_ATT_MTU_DEFAULT;
static const uint16_t kEventLen = BLE_GAP_EVENT_LENGTH_DEFAULT;
static const uint8_t kHvnQueueSize = 6;
static const uint8_t kWrCmdQueueSize = 4;

// Test rate in Hz (change via serial: RATE <hz>)
static uint16_t reportHz = 60;
static uint32_t lastReportMs = 0;
static uint32_t lastStatMs = 0;
static uint32_t sentCount = 0;
static uint32_t okCount = 0;
static uint32_t failCount = 0;
static uint32_t callTotalUs = 0;
static uint32_t callMaxUs = 0;

void startAdv() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_MOUSE);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void onConnect(uint16_t conn_handle) {
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  if (connection) {
    // Request 15ms interval, no latency, 4s supervision timeout
    connection->requestConnectionParameter(12, 0, 400);
  }
  Serial.println("[ble] connected");
}

void onDisconnect(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  (void)reason;
  Serial.println("[ble] disconnected");
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Bluefruit.configPrphConn(kMtuMax, kEventLen, kHvnQueueSize, kWrCmdQueueSize);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("TP-HID-Rate");
  Bluefruit.autoConnLed(true);

  Bluefruit.Periph.setConnectCallback(onConnect);
  Bluefruit.Periph.setDisconnectCallback(onDisconnect);

  bledis.setManufacturer("TouchPad");
  bledis.setModel("nRF52840");
  bledis.begin();

  blehid.begin();
  startAdv();

  Serial.println("[ble] HID rate test ready");
  Serial.println("[ble] cmd: RATE <hz>");
}

void handleSerial() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      line.trim();
      if (line.startsWith("RATE ")) {
        int v = line.substring(5).toInt();
        if (v >= 10 && v <= 200) {
          reportHz = (uint16_t)v;
          Serial.print("[rate] set to ");
          Serial.println(reportHz);
        } else {
          Serial.println("[rate] invalid (10-200)");
        }
      }
      line = "";
    } else if (c != '\r') {
      line += c;
    }
  }
}

void loop() {
  handleSerial();

  uint32_t now = millis();
  uint32_t intervalMs = 1000 / reportHz;

  if (Bluefruit.connected() && (now - lastReportMs >= intervalMs)) {
    lastReportMs = now;
    uint32_t t0 = micros();
    bool ok = blehid.mouseReport((uint8_t)0, (int8_t)1, (int8_t)0, (int8_t)0, (int8_t)0);
    uint32_t dt = micros() - t0;
    callTotalUs += dt;
    if (dt > callMaxUs) {
      callMaxUs = dt;
    }
    if (ok) {
      okCount++;
    } else {
      failCount++;
    }
    sentCount++;
  }

  if (now - lastStatMs >= 1000) {
    lastStatMs = now;
    uint32_t avgUs = sentCount ? (callTotalUs / sentCount) : 0;
    Serial.print("[stat] sent=");
    Serial.print(sentCount);
    Serial.print("/s at ");
    Serial.print(reportHz);
    Serial.print(" Hz, ok=");
    Serial.print(okCount);
    Serial.print(", fail=");
    Serial.print(failCount);
    Serial.print(", avg_us=");
    Serial.print(avgUs);
    Serial.print(", max_us=");
    Serial.print(callMaxUs);
    if (Bluefruit.connected()) {
      BLEConnection* conn = Bluefruit.Connection(Bluefruit.connHandle());
      if (conn) {
        uint16_t interval = conn->getConnectionInterval();
        Serial.print(", conn_int=");
        Serial.print(interval);
        Serial.print(" (");
        Serial.print(interval * 1.25f, 2);
        Serial.print("ms)");
        Serial.print(", latency=");
        Serial.print(conn->getSlaveLatency());
        Serial.print(", timeout=");
        Serial.print(conn->getSupervisionTimeout() * 10);
        Serial.print("ms");
      }
    }
    Serial.println();
    sentCount = 0;
    okCount = 0;
    failCount = 0;
    callTotalUs = 0;
    callMaxUs = 0;
  }
}
