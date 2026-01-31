#include <bluefruit.h>

// Basic BLE connection test for nRF52 + Bluefruit.
// Focused on the same BLE features used by touchpad_hid_nrf:
// - BLE HID (mouse)
// - BLE UART
// - Device Info Service
BLEDis bledis;
BLEHidAdafruit blehid;
BLEUart bleuart;

static uint32_t lastStatusMs = 0;

void onConnect(uint16_t conn_handle);
void onDisconnect(uint16_t conn_handle, uint8_t reason);

void startAdv() {
  Bluefruit.Advertising.clearData();
  Bluefruit.ScanResponse.clearData();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_MOUSE);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void initBle() {
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("TouchPadTest");
  Bluefruit.autoConnLed(false);

  Bluefruit.Periph.setConnectCallback(onConnect);
  Bluefruit.Periph.setDisconnectCallback(onDisconnect);
  Bluefruit.Security.setIOCaps(false, false, false);

  bledis.setManufacturer("TouchPad");
  bledis.setModel("nRF52840");
  bledis.begin();

  blehid.begin();
  bleuart.begin();

  startAdv();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("[ble] nRF52 BLE connection test");
  Serial.println("[ble] commands: HELP, INFO, UART <text>, HID CLICK, HID MOVE x y, PAIRCLR");

  initBle();
}

void loop() {
  handleSerial();

  uint32_t now = millis();
  if (now - lastStatusMs >= 5000) {
    lastStatusMs = now;
    printStatus();
  }
}


void onConnect(uint16_t conn_handle) {
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  if (connection) {
    connection->requestConnectionParameter(6, 0, 200);
  }
  Serial.println("[ble] connected");
}

void onDisconnect(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  (void)reason;
  Serial.println("[ble] disconnected");
}

void printStatus() {
  if (Bluefruit.connected()) {
    int8_t rssi = Bluefruit.Connection(0)->getRssi();
    Serial.print("[ble] connected, RSSI=");
    Serial.println(rssi);
  } else {
    Serial.println("[ble] advertising");
  }
}

void sendMouseClick(uint8_t buttons) {
  if (!Bluefruit.connected()) {
    Serial.println("[hid] not connected");
    return;
  }
  blehid.mouseReport((uint8_t)buttons, (int8_t)0, (int8_t)0, (int8_t)0, (int8_t)0);
  delay(5);
  blehid.mouseReport((uint8_t)0, (int8_t)0, (int8_t)0, (int8_t)0, (int8_t)0);
}

void sendMouseMove(int8_t x, int8_t y) {
  if (!Bluefruit.connected()) {
    Serial.println("[hid] not connected");
    return;
  }
  blehid.mouseReport((uint8_t)0, x, y, (int8_t)0, (int8_t)0);
}

void handleSerial() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      line.trim();
      if (line.length() > 0) {
        processCommand(line);
      }
      line = "";
    } else if (c != '\r') {
      line += c;
    }
  }
}

void processCommand(const String& line) {
  if (line.equalsIgnoreCase("HELP")) {
    Serial.println("HELP");
    Serial.println("INFO");
    Serial.println("UART <text>");
    Serial.println("HID CLICK");
    Serial.println("HID MOVE x y");
    Serial.println("PAIRCLR");
    return;
  }

  if (line.equalsIgnoreCase("INFO")) {
    printStatus();
    char name[32] = {0};
    Bluefruit.getName(name, sizeof(name));
    Serial.print("[ble] name=");
    Serial.println(name);
    return;
  }

  if (line.startsWith("UART ")) {
    String msg = line.substring(5);
    if (!Bluefruit.connected()) {
      Serial.println("[uart] not connected");
      return;
    }
    bleuart.println(msg);
    Serial.println("[uart] sent");
    return;
  }

  if (line.equalsIgnoreCase("HID CLICK")) {
    sendMouseClick(0x01);
    return;
  }

  if (line.startsWith("HID MOVE ")) {
    int sep = line.indexOf(' ', 9);
    if (sep < 0) {
      Serial.println("[hid] format: HID MOVE x y");
      return;
    }
    int x = line.substring(9, sep).toInt();
    int y = line.substring(sep + 1).toInt();
    sendMouseMove((int8_t)constrain(x, -127, 127), (int8_t)constrain(y, -127, 127));
    return;
  }

  if (line.equalsIgnoreCase("PAIRCLR")) {
    Bluefruit.Periph.clearBonds();
    Serial.println("[ble] bonds cleared");
    return;
  }

  Serial.println("[cmd] unknown");
}
