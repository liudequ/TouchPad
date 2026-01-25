#include <Wire.h>
#include <Adafruit_TinyUSB.h>
#include <LittleFS.h>

/*===== I2C-HID =====*/
#define I2C_ADDR 0x2C
#define INPUT_REG_L 0x09
#define INPUT_REG_H 0x01

#define SDA_PIN 4
#define SCL_PIN 5
#define INT_PIN 10
#define TP_EN 9  // ★ 新增：TouchPad ENABLE

uint8_t reportBuf[128];

enum {
  RID_MOUSE = 1,
  RID_KEYBOARD = 2,
};

Adafruit_USBD_HID usb_hid;

uint8_t const hid_report_descriptor[] = {
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(RID_MOUSE)),
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD))
};

/*===== 参数配置区 =====*/
// 单指移动
float sensitivity = 0.35f;
float smoothFactor = 0.2f;
float accelFactor = 0.015f;
float maxAccel = 2.5f;
const int16_t MAX_DELTA = 30;
const int16_t MOVE_DEADBAND = 1;

// 双指滚动
float scrollSensitivity = 0.00002f;
float scrollSmoothFactor = 0.2f;
bool naturalScroll = true;
const int16_t SCROLL_DEADBAND = 0;

// 点击与释放
const unsigned long TAP_MAX_MS = 200;
const unsigned long DOUBLE_TAP_WINDOW = 200;
const uint16_t DOUBLE_TAP_MAX_MOVE = 80;
const unsigned long RELEASE_TIMEOUT = 30;
const unsigned long INT_RELEASE_TIMEOUT_US = 5000;

const char* CONFIG_PATH = "/config.txt";

/*===== 状态 =====*/
enum GestureMode { MODE_NONE,
                   MODE_SINGLE,
                   MODE_DOUBLE };
GestureMode mode = MODE_NONE;

int16_t lastX1 = 0, lastY1 = 0;
int16_t lastX2 = 0, lastY2 = 0;
unsigned long lastTouchTime = 0;

float smoothDx = 0, smoothDy = 0;
float accumX = 0, accumY = 0;
float velX = 0, velY = 0;

float smoothScroll = 0;
float accumScroll = 0;
float scrollVel = 0;

bool tapCandidate = false;
unsigned long tapStartTime = 0;
int16_t tapStartX = 0;
int16_t tapStartY = 0;
bool pendingClick = false;
unsigned long lastTapTime = 0;
int16_t lastTapX = 0;
int16_t lastTapY = 0;

// 触摸板区域（基于实测坐标范围）
const int16_t TOUCH_MAX_X = 2628;
const int16_t TOUCH_MAX_Y = 1332;
uint8_t TOP_ZONE_PERCENT = 20;
uint8_t SIDE_ZONE_PERCENT = 35;

bool enableNavZones = true;

enum ZoneAction {
  ACTION_NONE = 0,
  ACTION_BACK = 1,
  ACTION_FORWARD = 2,
  ACTION_RIGHT_CLICK = 3,
  ACTION_LEFT_CLICK = 4
};

ZoneAction leftTopAction = ACTION_BACK;
ZoneAction rightTopAction = ACTION_FORWARD;
ZoneAction rightBottomAction = ACTION_RIGHT_CLICK;
ZoneAction leftBottomAction = ACTION_NONE;

/*===========================
   冷启动 Enable 时序
   ===========================*/
void touchColdBoot() {
  pinMode(TP_EN, OUTPUT);

  digitalWrite(TP_EN, LOW);  // Disable
  delay(20);

  digitalWrite(TP_EN, HIGH);  // Enable（关键上升沿）
  delay(150);                 // 等触摸 IC 完全启动
}

void setup() {
  Serial.begin(115200);

  // ★★★ 冷启动修复关键 ★★★
  touchColdBoot();

  // INT 必须提前配置，防止悬空
  pinMode(INT_PIN, INPUT_PULLUP);

  // I2C 初始化（延后）
  Wire.setSDA(SDA_PIN);
  Wire.setSCL(SCL_PIN);
  Wire.begin();
  Wire.setClock(400000);
  delay(50);

  if (LittleFS.begin()) {
    loadConfig();
  } else {
    Serial.println("[cfg] LittleFS mount failed");
  }

  usb_hid.setReportDescriptor(hid_report_descriptor,
                              sizeof(hid_report_descriptor));
  usb_hid.begin();
  while (!TinyUSBDevice.mounted()) {
    delay(10);
  }
}

const char* actionToString(ZoneAction action) {
  switch (action) {
    case ACTION_BACK:
      return "BACK";
    case ACTION_FORWARD:
      return "FORWARD";
    case ACTION_RIGHT_CLICK:
      return "RIGHT_CLICK";
    case ACTION_LEFT_CLICK:
      return "LEFT_CLICK";
    case ACTION_NONE:
    default:
      return "NONE";
  }
}

bool parseAction(const String& value, ZoneAction* out) {
  if (value.equalsIgnoreCase("NONE") || value == "0") {
    *out = ACTION_NONE;
    return true;
  }
  if (value.equalsIgnoreCase("BACK") || value == "1") {
    *out = ACTION_BACK;
    return true;
  }
  if (value.equalsIgnoreCase("FORWARD") || value == "2") {
    *out = ACTION_FORWARD;
    return true;
  }
  if (value.equalsIgnoreCase("RIGHT_CLICK") || value == "3") {
    *out = ACTION_RIGHT_CLICK;
    return true;
  }
  if (value.equalsIgnoreCase("LEFT_CLICK") || value == "4") {
    *out = ACTION_LEFT_CLICK;
    return true;
  }
  return false;
}

/*===========================
   主循环
   ===========================*/
void loop() {
  handleSerial();
  if (digitalRead(INT_PIN) == LOW) {
    readInputReport();
  }

  unsigned long now = millis();

  /*单指超时释放*/
  if (mode == MODE_SINGLE && now - lastTouchTime > RELEASE_TIMEOUT) {
    mode = MODE_NONE;
    velX = velY = 0;
    smoothDx = smoothDy = 0;
    accumX = accumY = 0;
    tapCandidate = false;
  }

  /*双指超时释放*/
  if (mode == MODE_DOUBLE && now - lastTouchTime > RELEASE_TIMEOUT) {
    mode = MODE_NONE;
    scrollVel = 0;
    smoothScroll = 0;
    accumScroll = 0;
  }

  /*连续输出*/
  if (mode == MODE_SINGLE) {
    accumX += velX;
    accumY += velY;
    int8_t mx = (int8_t)accumX;
    int8_t my = (int8_t)accumY;
    if (mx || my) {
      sendMouseMove(mx, my);
      accumX -= mx;
      accumY -= my;
    }
  }

  if (mode == MODE_DOUBLE) {
    accumScroll += scrollVel;
    int8_t s = (int8_t)accumScroll;
    if (s) {
      sendMouseWheel(naturalScroll ? s : -s);
      accumScroll -= s;
    }
  }

  if (pendingClick && now - lastTapTime > DOUBLE_TAP_WINDOW) {
    Serial.println("[tap] single click (double window expired)");
    sendMouseClick(MOUSE_BUTTON_LEFT);
    pendingClick = false;
  }
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
    Serial.println("CMD: GET scrollSensitivity");
    Serial.println("CMD: GET");
    Serial.println("CMD: SET <key> <value>");
    Serial.println("CMD: SAVE");
    Serial.println("CMD: LOAD");
    return;
  }

  if (line.equalsIgnoreCase("GET")) {
    Serial.print("scrollSensitivity=");
    Serial.println(scrollSensitivity, 8);
    Serial.print("topZonePercent=");
    Serial.println(TOP_ZONE_PERCENT);
    Serial.print("sideZonePercent=");
    Serial.println(SIDE_ZONE_PERCENT);
    Serial.print("enableNavZones=");
    Serial.println(enableNavZones ? "1" : "0");
    Serial.print("leftTopAction=");
    Serial.println(actionToString(leftTopAction));
    Serial.print("rightTopAction=");
    Serial.println(actionToString(rightTopAction));
    Serial.print("rightBottomAction=");
    Serial.println(actionToString(rightBottomAction));
    Serial.print("leftBottomAction=");
    Serial.println(actionToString(leftBottomAction));
    return;
  }

  if (line.startsWith("GET ")) {
    String key = line.substring(4);
    key.trim();
    if (key.equalsIgnoreCase("scrollSensitivity")) {
      Serial.print("scrollSensitivity=");
      Serial.println(scrollSensitivity, 8);
      return;
    }
    if (key.equalsIgnoreCase("topZonePercent")) {
      Serial.print("topZonePercent=");
      Serial.println(TOP_ZONE_PERCENT);
      return;
    }
    if (key.equalsIgnoreCase("sideZonePercent")) {
      Serial.print("sideZonePercent=");
      Serial.println(SIDE_ZONE_PERCENT);
      return;
    }
    if (key.equalsIgnoreCase("enableNavZones")) {
      Serial.print("enableNavZones=");
      Serial.println(enableNavZones ? "1" : "0");
      return;
    }
    if (key.equalsIgnoreCase("leftTopAction")) {
      Serial.print("leftTopAction=");
      Serial.println(actionToString(leftTopAction));
      return;
    }
    if (key.equalsIgnoreCase("rightTopAction")) {
      Serial.print("rightTopAction=");
      Serial.println(actionToString(rightTopAction));
      return;
    }
    if (key.equalsIgnoreCase("rightBottomAction")) {
      Serial.print("rightBottomAction=");
      Serial.println(actionToString(rightBottomAction));
      return;
    }
    if (key.equalsIgnoreCase("leftBottomAction")) {
      Serial.print("leftBottomAction=");
      Serial.println(actionToString(leftBottomAction));
      return;
    }
    Serial.println("ERR: key");
    return;
  }

  if (line.startsWith("SET ")) {
    int keyEnd = line.indexOf(' ', 4);
    if (keyEnd < 0) {
      Serial.println("ERR: SET format");
      return;
    }
    String key = line.substring(4, keyEnd);
    String valueStr = line.substring(keyEnd + 1);
    valueStr.trim();
    if (key.equalsIgnoreCase("scrollSensitivity")) {
      float v = valueStr.toFloat();
      if (v > 0.0f) {
        scrollSensitivity = v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("topZonePercent")) {
      int v = valueStr.toInt();
      if (v >= 5 && v <= 50) {
        TOP_ZONE_PERCENT = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("sideZonePercent")) {
      int v = valueStr.toInt();
      if (v >= 5 && v <= 50) {
        SIDE_ZONE_PERCENT = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("enableNavZones")) {
      if (valueStr.equalsIgnoreCase("1") || valueStr.equalsIgnoreCase("true")) {
        enableNavZones = true;
        Serial.println("OK");
      } else if (valueStr.equalsIgnoreCase("0") || valueStr.equalsIgnoreCase("false")) {
        enableNavZones = false;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftTopAction")) {
      ZoneAction action;
      if (parseAction(valueStr, &action)) {
        leftTopAction = action;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightTopAction")) {
      ZoneAction action;
      if (parseAction(valueStr, &action)) {
        rightTopAction = action;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightBottomAction")) {
      ZoneAction action;
      if (parseAction(valueStr, &action)) {
        rightBottomAction = action;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftBottomAction")) {
      ZoneAction action;
      if (parseAction(valueStr, &action)) {
        leftBottomAction = action;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    Serial.println("ERR: key");
    return;
  }

  if (line.equalsIgnoreCase("SAVE")) {
    if (saveConfig()) {
      Serial.println("OK");
    } else {
      Serial.println("ERR: save");
    }
    return;
  }

  if (line.equalsIgnoreCase("LOAD")) {
    if (loadConfig()) {
      Serial.println("OK");
    } else {
      Serial.println("ERR: load");
    }
    return;
  }

  Serial.println("ERR: unknown");
}

bool loadConfig() {
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) {
    Serial.println("[cfg] no config");
    return false;
  }
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    int eq = line.indexOf('=');
    if (eq < 0) continue;
    String key = line.substring(0, eq);
    String value = line.substring(eq + 1);
    key.trim();
    value.trim();
    if (key.equalsIgnoreCase("scrollSensitivity")) {
      float val = value.toFloat();
      if (val > 0.0f) scrollSensitivity = val;
    } else if (key.equalsIgnoreCase("topZonePercent")) {
      int v = value.toInt();
      if (v >= 5 && v <= 50) TOP_ZONE_PERCENT = (uint8_t)v;
    } else if (key.equalsIgnoreCase("sideZonePercent")) {
      int v = value.toInt();
      if (v >= 5 && v <= 50) SIDE_ZONE_PERCENT = (uint8_t)v;
    } else if (key.equalsIgnoreCase("enableNavZones")) {
      enableNavZones = (value == "1" || value.equalsIgnoreCase("true"));
    } else if (key.equalsIgnoreCase("leftTopAction")) {
      ZoneAction action;
      if (parseAction(value, &action)) leftTopAction = action;
    } else if (key.equalsIgnoreCase("rightTopAction")) {
      ZoneAction action;
      if (parseAction(value, &action)) rightTopAction = action;
    } else if (key.equalsIgnoreCase("rightBottomAction")) {
      ZoneAction action;
      if (parseAction(value, &action)) rightBottomAction = action;
    } else if (key.equalsIgnoreCase("leftBottomAction")) {
      ZoneAction action;
      if (parseAction(value, &action)) leftBottomAction = action;
    }
  }
  f.close();
  return true;
}

bool saveConfig() {
  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) return false;
  f.print("scrollSensitivity=");
  f.println(scrollSensitivity, 8);
  f.print("topZonePercent=");
  f.println(TOP_ZONE_PERCENT);
  f.print("sideZonePercent=");
  f.println(SIDE_ZONE_PERCENT);
  f.print("enableNavZones=");
  f.println(enableNavZones ? "1" : "0");
  f.print("leftTopAction=");
  f.println(actionToString(leftTopAction));
  f.print("rightTopAction=");
  f.println(actionToString(rightTopAction));
  f.print("rightBottomAction=");
  f.println(actionToString(rightBottomAction));
  f.print("leftBottomAction=");
  f.println(actionToString(leftBottomAction));
  f.close();
  return true;
}

bool inLeftTopZone(int16_t x, int16_t y) {
  int16_t maxX = (TOUCH_MAX_X * SIDE_ZONE_PERCENT) / 100;
  int16_t maxY = (TOUCH_MAX_Y * TOP_ZONE_PERCENT) / 100;
  return x <= maxX && y <= maxY;
}

bool inRightTopZone(int16_t x, int16_t y) {
  int16_t minX = TOUCH_MAX_X - (TOUCH_MAX_X * SIDE_ZONE_PERCENT) / 100;
  int16_t maxY = (TOUCH_MAX_Y * TOP_ZONE_PERCENT) / 100;
  return x >= minX && y <= maxY;
}

bool inRightBottomZone(int16_t x, int16_t y) {
  int16_t minX = TOUCH_MAX_X - (TOUCH_MAX_X * SIDE_ZONE_PERCENT) / 100;
  int16_t minY = TOUCH_MAX_Y - (TOUCH_MAX_Y * TOP_ZONE_PERCENT) / 100;
  return x >= minX && y >= minY;
}

bool inLeftBottomZone(int16_t x, int16_t y) {
  int16_t maxX = (TOUCH_MAX_X * SIDE_ZONE_PERCENT) / 100;
  int16_t minY = TOUCH_MAX_Y - (TOUCH_MAX_Y * TOP_ZONE_PERCENT) / 100;
  return x <= maxX && y >= minY;
}

void sendBack() {
  sendKeyboard(KEYBOARD_MODIFIER_LEFTALT, HID_KEY_ARROW_LEFT);
}

void sendForward() {
  sendKeyboard(KEYBOARD_MODIFIER_LEFTALT, HID_KEY_ARROW_RIGHT);
}

void performZoneAction(ZoneAction action) {
  switch (action) {
    case ACTION_BACK:
      Serial.println("[tap] back");
      sendBack();
      break;
    case ACTION_FORWARD:
      Serial.println("[tap] forward");
      sendForward();
      break;
    case ACTION_RIGHT_CLICK:
      Serial.println("[tap] right click");
      sendMouseClick(MOUSE_BUTTON_RIGHT);
      break;
    case ACTION_LEFT_CLICK:
      Serial.println("[tap] left click");
      sendMouseClick(MOUSE_BUTTON_LEFT);
      break;
    case ACTION_NONE:
    default:
      break;
  }
}

void sendMouseMove(int8_t x, int8_t y) {
  uint8_t report[5] = { 0, (uint8_t)x, (uint8_t)y, 0, 0 };
  usb_hid.sendReport(RID_MOUSE, report, sizeof(report));
}

void sendMouseWheel(int8_t wheel) {
  uint8_t report[5] = { 0, 0, 0, (uint8_t)wheel, 0 };
  usb_hid.sendReport(RID_MOUSE, report, sizeof(report));
}

void sendMouseClick(uint8_t buttons) {
  uint8_t report[5] = { buttons, 0, 0, 0, 0 };
  usb_hid.sendReport(RID_MOUSE, report, sizeof(report));
  delay(5);
  report[0] = 0;
  usb_hid.sendReport(RID_MOUSE, report, sizeof(report));
}

void sendKeyboard(uint8_t modifier, uint8_t keycode) {
  uint8_t report[8] = { modifier, 0, keycode, 0, 0, 0, 0, 0 };
  usb_hid.sendReport(RID_KEYBOARD, report, sizeof(report));
  delay(5);
  for (uint8_t i = 0; i < sizeof(report); i++) report[i] = 0;
  usb_hid.sendReport(RID_KEYBOARD, report, sizeof(report));
}

/* ===========================
   Report 解析
   =========================== */
void handleReport(uint8_t* buf, uint16_t len) {
  if (len < 13) return;
  uint8_t s0 = buf[3];
  uint8_t s1 = buf[8];

  bool f1 = s0 & 0x02;
  bool f2 = s1 & 0x02;

  int16_t x1 = buf[4] | (buf[5] << 8);
  int16_t y1 = buf[6] | (buf[7] << 8);
  int16_t x2 = buf[9] | (buf[10] << 8);
  int16_t y2 = buf[11] | (buf[12] << 8);

  unsigned long now = millis();

  /*===== 双指滚动 =====*/
  if (f1 && f2) {
    if (mode == MODE_DOUBLE) {
      int16_t dy = ((y1 - lastY1) + (y2 - lastY2)) / 2;
      dy = constrain(dy, -MAX_DELTA, MAX_DELTA);
      if (abs(dy) <= SCROLL_DEADBAND) dy = 0;
      float v = dy * scrollSensitivity;
      smoothScroll += (v - smoothScroll) * scrollSmoothFactor;
      scrollVel = smoothScroll;
    } else {
      smoothScroll = accumScroll = 0;
    }

    lastX1 = x1;
    lastY1 = y1;
    lastX2 = x2;
    lastY2 = y2;

    mode = MODE_DOUBLE;
    tapCandidate = false;
    lastTouchTime = now;
    return;
  }

  /*===== 单指移动 =====*/
  if (f1 && !f2) {
    if (mode == MODE_SINGLE) {
      int16_t dx = x1 - lastX1;
      int16_t dy = y1 - lastY1;
      dx = constrain(dx, -MAX_DELTA, MAX_DELTA);
      dy = constrain(dy, -MAX_DELTA, MAX_DELTA);
      if (abs(dx) <= MOVE_DEADBAND) dx = 0;
      if (abs(dy) <= MOVE_DEADBAND) dy = 0;

      if (dx == 0 && dy == 0) {
        velX = velY = 0;
        smoothDx = smoothDy = 0;
        accumX = accumY = 0;
        lastTouchTime = now;
        return;
      }

      float fx = dx * sensitivity;
      float fy = dy * sensitivity;

      float speed = sqrt(fx * fx + fy * fy);
      float accel = 1.0f + min(speed * accelFactor, maxAccel);
      fx *= accel;
      fy *= accel;

      smoothDx += (fx - smoothDx) * smoothFactor;
      smoothDy += (fy - smoothDy) * smoothFactor;

      velX = smoothDx;
      velY = smoothDy;
    } else {
      smoothDx = smoothDy = 0;
      accumX = accumY = 0;
      velX = velY = 0;
      tapCandidate = true;
      tapStartTime = now;
      tapStartX = x1;
      tapStartY = y1;
    }

    lastX1 = x1;
    lastY1 = y1;
    mode = MODE_SINGLE;
    lastTouchTime = now;
    return;
  }

  /*===== 抬起：处理单击 =====*/
  if (!f1 && mode == MODE_SINGLE) {
    if (tapCandidate) {
      unsigned long dt = now - tapStartTime;
      if (dt <= TAP_MAX_MS) {
        if (enableNavZones && inLeftTopZone(tapStartX, tapStartY)) {
          performZoneAction(leftTopAction);
          pendingClick = false;
          tapCandidate = false;
          mode = MODE_NONE;
          return;
        }
        if (enableNavZones && inRightTopZone(tapStartX, tapStartY)) {
          performZoneAction(rightTopAction);
          pendingClick = false;
          tapCandidate = false;
          mode = MODE_NONE;
          return;
        }
        if (enableNavZones && inRightBottomZone(tapStartX, tapStartY)) {
          performZoneAction(rightBottomAction);
          pendingClick = false;
          tapCandidate = false;
          mode = MODE_NONE;
          return;
        }
        if (enableNavZones && inLeftBottomZone(tapStartX, tapStartY)) {
          performZoneAction(leftBottomAction);
          pendingClick = false;
          tapCandidate = false;
          mode = MODE_NONE;
          return;
        }
        if (pendingClick && now - lastTapTime <= DOUBLE_TAP_WINDOW) {
          uint16_t dist = abs(tapStartX - lastTapX) + abs(tapStartY - lastTapY);
          if (dist <= DOUBLE_TAP_MAX_MOVE) {
            Serial.println("[tap] double click");
            sendMouseClick(MOUSE_BUTTON_LEFT);
            delay(30);
            sendMouseClick(MOUSE_BUTTON_LEFT);
            pendingClick = false;
          } else {
            Serial.println("[tap] double click rejected, distance too large");
            sendMouseClick(MOUSE_BUTTON_LEFT);
            pendingClick = true;
            lastTapTime = now;
            lastTapX = tapStartX;
            lastTapY = tapStartY;
          }
        } else {
          Serial.println("[tap] pending single click");
          pendingClick = true;
          lastTapTime = now;
          lastTapX = tapStartX;
          lastTapY = tapStartY;
        }
      } else {
        Serial.print("[tap] ignore, dt=");
        Serial.println(dt);
      }
    }
    tapCandidate = false;
    mode = MODE_NONE;
  }
}

bool waitIntRelease(unsigned long timeoutUs) {
  unsigned long start = micros();
  while (digitalRead(INT_PIN) == LOW) {
    if (micros() - start > timeoutUs) return false;
    delayMicroseconds(50);
  }
  return true;
}

/*===========================
   I2C 读取
   ===========================*/
void readInputReport() {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(INPUT_REG_L);
  Wire.write(INPUT_REG_H);
  if (Wire.endTransmission(false) != 0) {
    waitIntRelease(INT_RELEASE_TIMEOUT_US);
    return;
  }

  if (Wire.requestFrom(I2C_ADDR, (uint8_t)2) != 2) {
    waitIntRelease(INT_RELEASE_TIMEOUT_US);
    return;
  }
  uint16_t len = Wire.read() | (Wire.read() << 8);
  if (len == 0 || len > sizeof(reportBuf)) {
    waitIntRelease(INT_RELEASE_TIMEOUT_US);
    return;
  }

  if (Wire.requestFrom(I2C_ADDR, (uint8_t)len) != len) {
    waitIntRelease(INT_RELEASE_TIMEOUT_US);
    return;
  }
  for (uint16_t i = 0; i < len; i++) reportBuf[i] = Wire.read();

  handleReport(reportBuf, len);

  waitIntRelease(INT_RELEASE_TIMEOUT_US);
}
