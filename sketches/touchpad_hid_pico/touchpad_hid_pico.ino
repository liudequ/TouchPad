#include "touchpad_types.h"

#include <Wire.h>
#include <Adafruit_TinyUSB.h>
#include <LittleFS.h>
#include <pico/bootrom.h>

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

void rebootToBootsel() {
  Serial.println("[sys] reboot to BOOTSEL");
  delay(20);
  reset_usb_boot(0, 0);
}

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
const unsigned long TAP_GUARD_AFTER_SCROLL_MS = 150;

// 三指滑动
uint16_t threeSwipeThresholdX = 200;
uint16_t threeSwipeThresholdY = 200;
uint16_t threeSwipeTimeout = 350;
uint16_t threeSwipeCooldown = 400;

const char* CONFIG_PATH = "/config.txt";

/*===== 状态 =====*/
enum GestureMode { MODE_NONE,
                   MODE_SINGLE,
                   MODE_DOUBLE,
                   MODE_TRIPLE };
GestureMode mode = MODE_NONE;

int16_t lastX1 = 0, lastY1 = 0;
int16_t lastX2 = 0, lastY2 = 0;
unsigned long lastTouchTime = 0;
unsigned long lastSwipeTime = 0;
unsigned long lastScrollTime = 0;

int16_t tripleStartX = 0;
int16_t tripleStartY = 0;
int16_t tripleLastX = 0;
int16_t tripleLastY = 0;
unsigned long tripleStartTime = 0;

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

ZoneBinding leftTopZone = { ZONE_KEYBOARD, 0, KEYBOARD_MODIFIER_LEFTALT, HID_KEY_ARROW_LEFT };
ZoneBinding rightTopZone = { ZONE_KEYBOARD, 0, KEYBOARD_MODIFIER_LEFTALT, HID_KEY_ARROW_RIGHT };
ZoneBinding rightBottomZone = { ZONE_MOUSE, MOUSE_BUTTON_RIGHT, 0, 0 };
ZoneBinding leftBottomZone = { ZONE_NONE, 0, 0, 0 };

ZoneBinding threeLeftBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding threeRightBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding threeUpBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding threeDownBinding = { ZONE_NONE, 0, 0, 0 };

void applyDefaults() {
  scrollSensitivity = 0.00002f;
  TOP_ZONE_PERCENT = 20;
  SIDE_ZONE_PERCENT = 35;
  enableNavZones = true;
  leftTopZone = { ZONE_KEYBOARD, 0, KEYBOARD_MODIFIER_LEFTALT, HID_KEY_ARROW_LEFT };
  rightTopZone = { ZONE_KEYBOARD, 0, KEYBOARD_MODIFIER_LEFTALT, HID_KEY_ARROW_RIGHT };
  rightBottomZone = { ZONE_MOUSE, MOUSE_BUTTON_RIGHT, 0, 0 };
  leftBottomZone = { ZONE_NONE, 0, 0, 0 };
  threeSwipeThresholdX = 200;
  threeSwipeThresholdY = 200;
  threeSwipeTimeout = 350;
  threeSwipeCooldown = 400;
  threeLeftBinding = { ZONE_NONE, 0, 0, 0 };
  threeRightBinding = { ZONE_NONE, 0, 0, 0 };
  threeUpBinding = { ZONE_NONE, 0, 0, 0 };
  threeDownBinding = { ZONE_NONE, 0, 0, 0 };
}

/*===========================
   冷启动 Enable 时序
   ===========================*/
void touchColdBoot() {
  pinMode(TP_EN, OUTPUT);

  digitalWrrwudite(TP_EN, LOW);  // Disable
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

  applyDefaults();
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

const char* typeToString(ZoneType type) {
  switch (type) {
    case ZONE_MOUSE:
      return "MOUSE";
    case ZONE_KEYBOARD:
      return "KEYBOARD";
    case ZONE_NONE:
    default:
      return "NONE";
  }
}

bool parseType(const String& value, ZoneType* out) {
  if (value.equalsIgnoreCase("NONE") || value == "0") {
    *out = ZONE_NONE;
    return true;
  }
  if (value.equalsIgnoreCase("MOUSE") || value == "1") {
    *out = ZONE_MOUSE;
    return true;
  }
  if (value.equalsIgnoreCase("KEYBOARD") || value == "2") {
    *out = ZONE_KEYBOARD;
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
    lastScrollTime = now;
  }

  if (mode == MODE_TRIPLE && now - lastTouchTime > RELEASE_TIMEOUT) {
    mode = MODE_NONE;
  }

  /*连续输出*/
  if (mode == MODE_SINGLE && !tapCandidate) {
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
    Serial.println("CMD: RESET");
    Serial.println("CMD: BOOT");
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
    Serial.print("leftTopType=");
    Serial.println(typeToString(leftTopZone.type));
    Serial.print("leftTopButtons=");
    Serial.println(leftTopZone.mouseButtons);
    Serial.print("leftTopModifier=");
    Serial.println(leftTopZone.keyModifier);
    Serial.print("leftTopKey=");
    Serial.println(leftTopZone.keyCode);
    Serial.print("rightTopType=");
    Serial.println(typeToString(rightTopZone.type));
    Serial.print("rightTopButtons=");
    Serial.println(rightTopZone.mouseButtons);
    Serial.print("rightTopModifier=");
    Serial.println(rightTopZone.keyModifier);
    Serial.print("rightTopKey=");
    Serial.println(rightTopZone.keyCode);
    Serial.print("rightBottomType=");
    Serial.println(typeToString(rightBottomZone.type));
    Serial.print("rightBottomButtons=");
    Serial.println(rightBottomZone.mouseButtons);
    Serial.print("rightBottomModifier=");
    Serial.println(rightBottomZone.keyModifier);
    Serial.print("rightBottomKey=");
    Serial.println(rightBottomZone.keyCode);
    Serial.print("leftBottomType=");
    Serial.println(typeToString(leftBottomZone.type));
    Serial.print("leftBottomButtons=");
    Serial.println(leftBottomZone.mouseButtons);
    Serial.print("leftBottomModifier=");
    Serial.println(leftBottomZone.keyModifier);
    Serial.print("leftBottomKey=");
    Serial.println(leftBottomZone.keyCode);
    Serial.print("threeLeftType=");
    Serial.println(typeToString(threeLeftBinding.type));
    Serial.print("threeLeftButtons=");
    Serial.println(threeLeftBinding.mouseButtons);
    Serial.print("threeLeftModifier=");
    Serial.println(threeLeftBinding.keyModifier);
    Serial.print("threeLeftKey=");
    Serial.println(threeLeftBinding.keyCode);
    Serial.print("threeRightType=");
    Serial.println(typeToString(threeRightBinding.type));
    Serial.print("threeRightButtons=");
    Serial.println(threeRightBinding.mouseButtons);
    Serial.print("threeRightModifier=");
    Serial.println(threeRightBinding.keyModifier);
    Serial.print("threeRightKey=");
    Serial.println(threeRightBinding.keyCode);
    Serial.print("threeUpType=");
    Serial.println(typeToString(threeUpBinding.type));
    Serial.print("threeUpButtons=");
    Serial.println(threeUpBinding.mouseButtons);
    Serial.print("threeUpModifier=");
    Serial.println(threeUpBinding.keyModifier);
    Serial.print("threeUpKey=");
    Serial.println(threeUpBinding.keyCode);
    Serial.print("threeDownType=");
    Serial.println(typeToString(threeDownBinding.type));
    Serial.print("threeDownButtons=");
    Serial.println(threeDownBinding.mouseButtons);
    Serial.print("threeDownModifier=");
    Serial.println(threeDownBinding.keyModifier);
    Serial.print("threeDownKey=");
    Serial.println(threeDownBinding.keyCode);
    Serial.print("threeSwipeThresholdX=");
    Serial.println(threeSwipeThresholdX);
    Serial.print("threeSwipeThresholdY=");
    Serial.println(threeSwipeThresholdY);
    Serial.print("threeSwipeTimeout=");
    Serial.println(threeSwipeTimeout);
    Serial.print("threeSwipeCooldown=");
    Serial.println(threeSwipeCooldown);
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
    if (key.equalsIgnoreCase("leftTopType")) {
      Serial.print("leftTopType=");
      Serial.println(typeToString(leftTopZone.type));
      return;
    }
    if (key.equalsIgnoreCase("leftTopButtons")) {
      Serial.print("leftTopButtons=");
      Serial.println(leftTopZone.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("leftTopModifier")) {
      Serial.print("leftTopModifier=");
      Serial.println(leftTopZone.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("leftTopKey")) {
      Serial.print("leftTopKey=");
      Serial.println(leftTopZone.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("rightTopType")) {
      Serial.print("rightTopType=");
      Serial.println(typeToString(rightTopZone.type));
      return;
    }
    if (key.equalsIgnoreCase("rightTopButtons")) {
      Serial.print("rightTopButtons=");
      Serial.println(rightTopZone.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("rightTopModifier")) {
      Serial.print("rightTopModifier=");
      Serial.println(rightTopZone.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("rightTopKey")) {
      Serial.print("rightTopKey=");
      Serial.println(rightTopZone.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("rightBottomType")) {
      Serial.print("rightBottomType=");
      Serial.println(typeToString(rightBottomZone.type));
      return;
    }
    if (key.equalsIgnoreCase("rightBottomButtons")) {
      Serial.print("rightBottomButtons=");
      Serial.println(rightBottomZone.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("rightBottomModifier")) {
      Serial.print("rightBottomModifier=");
      Serial.println(rightBottomZone.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("rightBottomKey")) {
      Serial.print("rightBottomKey=");
      Serial.println(rightBottomZone.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("leftBottomType")) {
      Serial.print("leftBottomType=");
      Serial.println(typeToString(leftBottomZone.type));
      return;
    }
    if (key.equalsIgnoreCase("leftBottomButtons")) {
      Serial.print("leftBottomButtons=");
      Serial.println(leftBottomZone.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("leftBottomModifier")) {
      Serial.print("leftBottomModifier=");
      Serial.println(leftBottomZone.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("leftBottomKey")) {
      Serial.print("leftBottomKey=");
      Serial.println(leftBottomZone.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("threeLeftType")) {
      Serial.print("threeLeftType=");
      Serial.println(typeToString(threeLeftBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("threeLeftButtons")) {
      Serial.print("threeLeftButtons=");
      Serial.println(threeLeftBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("threeLeftModifier")) {
      Serial.print("threeLeftModifier=");
      Serial.println(threeLeftBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("threeLeftKey")) {
      Serial.print("threeLeftKey=");
      Serial.println(threeLeftBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("threeRightType")) {
      Serial.print("threeRightType=");
      Serial.println(typeToString(threeRightBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("threeRightButtons")) {
      Serial.print("threeRightButtons=");
      Serial.println(threeRightBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("threeRightModifier")) {
      Serial.print("threeRightModifier=");
      Serial.println(threeRightBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("threeRightKey")) {
      Serial.print("threeRightKey=");
      Serial.println(threeRightBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("threeUpType")) {
      Serial.print("threeUpType=");
      Serial.println(typeToString(threeUpBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("threeUpButtons")) {
      Serial.print("threeUpButtons=");
      Serial.println(threeUpBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("threeUpModifier")) {
      Serial.print("threeUpModifier=");
      Serial.println(threeUpBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("threeUpKey")) {
      Serial.print("threeUpKey=");
      Serial.println(threeUpBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("threeDownType")) {
      Serial.print("threeDownType=");
      Serial.println(typeToString(threeDownBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("threeDownButtons")) {
      Serial.print("threeDownButtons=");
      Serial.println(threeDownBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("threeDownModifier")) {
      Serial.print("threeDownModifier=");
      Serial.println(threeDownBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("threeDownKey")) {
      Serial.print("threeDownKey=");
      Serial.println(threeDownBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeThresholdX")) {
      Serial.print("threeSwipeThresholdX=");
      Serial.println(threeSwipeThresholdX);
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeThresholdY")) {
      Serial.print("threeSwipeThresholdY=");
      Serial.println(threeSwipeThresholdY);
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeTimeout")) {
      Serial.print("threeSwipeTimeout=");
      Serial.println(threeSwipeTimeout);
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeCooldown")) {
      Serial.print("threeSwipeCooldown=");
      Serial.println(threeSwipeCooldown);
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
    if (key.equalsIgnoreCase("leftTopType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        leftTopZone.type = type;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftTopButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        leftTopZone.mouseButtons = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftTopModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        leftTopZone.keyModifier = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftTopKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        leftTopZone.keyCode = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightTopType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        rightTopZone.type = type;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightTopButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        rightTopZone.mouseButtons = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightTopModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        rightTopZone.keyModifier = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightTopKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        rightTopZone.keyCode = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightBottomType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        rightBottomZone.type = type;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightBottomButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        rightBottomZone.mouseButtons = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightBottomModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        rightBottomZone.keyModifier = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightBottomKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        rightBottomZone.keyCode = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftBottomType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        leftBottomZone.type = type;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftBottomButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        leftBottomZone.mouseButtons = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftBottomModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        leftBottomZone.keyModifier = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftBottomKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        leftBottomZone.keyCode = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeLeftType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        threeLeftBinding.type = type;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeLeftButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        threeLeftBinding.mouseButtons = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeLeftModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeLeftBinding.keyModifier = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeLeftKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeLeftBinding.keyCode = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeRightType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        threeRightBinding.type = type;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeRightButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        threeRightBinding.mouseButtons = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeRightModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeRightBinding.keyModifier = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeRightKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeRightBinding.keyCode = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeUpType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        threeUpBinding.type = type;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeUpButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        threeUpBinding.mouseButtons = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeUpModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeUpBinding.keyModifier = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeUpKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeUpBinding.keyCode = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeDownType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        threeDownBinding.type = type;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeDownButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        threeDownBinding.mouseButtons = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeDownModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeDownBinding.keyModifier = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeDownKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeDownBinding.keyCode = (uint8_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeThresholdX")) {
      int v = valueStr.toInt();
      if (v >= 50 && v <= 800) {
        threeSwipeThresholdX = (uint16_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeThresholdY")) {
      int v = valueStr.toInt();
      if (v >= 50 && v <= 800) {
        threeSwipeThresholdY = (uint16_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeTimeout")) {
      int v = valueStr.toInt();
      if (v >= 50 && v <= 1000) {
        threeSwipeTimeout = (uint16_t)v;
        Serial.println("OK");
      } else {
        Serial.println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeCooldown")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 2000) {
        threeSwipeCooldown = (uint16_t)v;
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

  if (line.equalsIgnoreCase("RESET")) {
    applyDefaults();
    Serial.println("OK");
    return;
  }

  if (line.equalsIgnoreCase("BOOT")) {
    Serial.println("OK");
    rebootToBootsel();
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
    } else if (key.equalsIgnoreCase("leftTopType")) {
      ZoneType type;
      if (parseType(value, &type)) leftTopZone.type = type;
    } else if (key.equalsIgnoreCase("leftTopButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) leftTopZone.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("leftTopModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) leftTopZone.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("leftTopKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) leftTopZone.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("rightTopType")) {
      ZoneType type;
      if (parseType(value, &type)) rightTopZone.type = type;
    } else if (key.equalsIgnoreCase("rightTopButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) rightTopZone.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("rightTopModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) rightTopZone.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("rightTopKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) rightTopZone.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("rightBottomType")) {
      ZoneType type;
      if (parseType(value, &type)) rightBottomZone.type = type;
    } else if (key.equalsIgnoreCase("rightBottomButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) rightBottomZone.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("rightBottomModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) rightBottomZone.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("rightBottomKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) rightBottomZone.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("leftBottomType")) {
      ZoneType type;
      if (parseType(value, &type)) leftBottomZone.type = type;
    } else if (key.equalsIgnoreCase("leftBottomButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) leftBottomZone.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("leftBottomModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) leftBottomZone.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("leftBottomKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) leftBottomZone.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeLeftType")) {
      ZoneType type;
      if (parseType(value, &type)) threeLeftBinding.type = type;
    } else if (key.equalsIgnoreCase("threeLeftButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) threeLeftBinding.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeLeftModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) threeLeftBinding.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeLeftKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) threeLeftBinding.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeRightType")) {
      ZoneType type;
      if (parseType(value, &type)) threeRightBinding.type = type;
    } else if (key.equalsIgnoreCase("threeRightButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) threeRightBinding.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeRightModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) threeRightBinding.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeRightKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) threeRightBinding.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeUpType")) {
      ZoneType type;
      if (parseType(value, &type)) threeUpBinding.type = type;
    } else if (key.equalsIgnoreCase("threeUpButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) threeUpBinding.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeUpModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) threeUpBinding.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeUpKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) threeUpBinding.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeDownType")) {
      ZoneType type;
      if (parseType(value, &type)) threeDownBinding.type = type;
    } else if (key.equalsIgnoreCase("threeDownButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) threeDownBinding.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeDownModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) threeDownBinding.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeDownKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) threeDownBinding.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeSwipeThresholdX")) {
      int v = value.toInt();
      if (v >= 50 && v <= 800) threeSwipeThresholdX = (uint16_t)v;
    } else if (key.equalsIgnoreCase("threeSwipeThresholdY")) {
      int v = value.toInt();
      if (v >= 50 && v <= 800) threeSwipeThresholdY = (uint16_t)v;
    } else if (key.equalsIgnoreCase("threeSwipeTimeout")) {
      int v = value.toInt();
      if (v >= 50 && v <= 1000) threeSwipeTimeout = (uint16_t)v;
    } else if (key.equalsIgnoreCase("threeSwipeCooldown")) {
      int v = value.toInt();
      if (v >= 0 && v <= 2000) threeSwipeCooldown = (uint16_t)v;
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
  f.print("leftTopType=");
  f.println(typeToString(leftTopZone.type));
  f.print("leftTopButtons=");
  f.println(leftTopZone.mouseButtons);
  f.print("leftTopModifier=");
  f.println(leftTopZone.keyModifier);
  f.print("leftTopKey=");
  f.println(leftTopZone.keyCode);
  f.print("rightTopType=");
  f.println(typeToString(rightTopZone.type));
  f.print("rightTopButtons=");
  f.println(rightTopZone.mouseButtons);
  f.print("rightTopModifier=");
  f.println(rightTopZone.keyModifier);
  f.print("rightTopKey=");
  f.println(rightTopZone.keyCode);
  f.print("rightBottomType=");
  f.println(typeToString(rightBottomZone.type));
  f.print("rightBottomButtons=");
  f.println(rightBottomZone.mouseButtons);
  f.print("rightBottomModifier=");
  f.println(rightBottomZone.keyModifier);
  f.print("rightBottomKey=");
  f.println(rightBottomZone.keyCode);
  f.print("leftBottomType=");
  f.println(typeToString(leftBottomZone.type));
  f.print("leftBottomButtons=");
  f.println(leftBottomZone.mouseButtons);
  f.print("leftBottomModifier=");
  f.println(leftBottomZone.keyModifier);
  f.print("leftBottomKey=");
  f.println(leftBottomZone.keyCode);
  f.print("threeLeftType=");
  f.println(typeToString(threeLeftBinding.type));
  f.print("threeLeftButtons=");
  f.println(threeLeftBinding.mouseButtons);
  f.print("threeLeftModifier=");
  f.println(threeLeftBinding.keyModifier);
  f.print("threeLeftKey=");
  f.println(threeLeftBinding.keyCode);
  f.print("threeRightType=");
  f.println(typeToString(threeRightBinding.type));
  f.print("threeRightButtons=");
  f.println(threeRightBinding.mouseButtons);
  f.print("threeRightModifier=");
  f.println(threeRightBinding.keyModifier);
  f.print("threeRightKey=");
  f.println(threeRightBinding.keyCode);
  f.print("threeUpType=");
  f.println(typeToString(threeUpBinding.type));
  f.print("threeUpButtons=");
  f.println(threeUpBinding.mouseButtons);
  f.print("threeUpModifier=");
  f.println(threeUpBinding.keyModifier);
  f.print("threeUpKey=");
  f.println(threeUpBinding.keyCode);
  f.print("threeDownType=");
  f.println(typeToString(threeDownBinding.type));
  f.print("threeDownButtons=");
  f.println(threeDownBinding.mouseButtons);
  f.print("threeDownModifier=");
  f.println(threeDownBinding.keyModifier);
  f.print("threeDownKey=");
  f.println(threeDownBinding.keyCode);
  f.print("threeSwipeThresholdX=");
  f.println(threeSwipeThresholdX);
  f.print("threeSwipeThresholdY=");
  f.println(threeSwipeThresholdY);
  f.print("threeSwipeTimeout=");
  f.println(threeSwipeTimeout);
  f.print("threeSwipeCooldown=");
  f.println(threeSwipeCooldown);
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

void performZoneAction(const ZoneBinding& binding) {
  if (binding.type == ZONE_MOUSE) {
    if (binding.mouseButtons) {
      sendMouseClick(binding.mouseButtons);
    }
    return;
  }
  if (binding.type == ZONE_KEYBOARD) {
    if (binding.keyCode) {
      sendKeyboard(binding.keyModifier, binding.keyCode);
    }
    return;
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
  uint8_t s2 = (len >= 18) ? buf[13] : 0;

  bool f1 = s0 & 0x02;
  bool f2 = s1 & 0x02;
  bool f3 = (len >= 18) ? (s2 & 0x02) : false;

  int16_t x1 = buf[4] | (buf[5] << 8);
  int16_t y1 = buf[6] | (buf[7] << 8);
  int16_t x2 = buf[9] | (buf[10] << 8);
  int16_t y2 = buf[11] | (buf[12] << 8);
  int16_t x3 = (len >= 18) ? (buf[14] | (buf[15] << 8)) : 0;
  int16_t y3 = (len >= 18) ? (buf[16] | (buf[17] << 8)) : 0;

  unsigned long now = millis();

  /*===== 三指左右滑动 =====*/
  if (f1 && f2 && f3) {
    int16_t avgX = (x1 + x2 + x3) / 3;
    int16_t avgY = (y1 + y2 + y3) / 3;
    if (mode != MODE_TRIPLE) {
      tripleStartX = avgX;
      tripleStartY = avgY;
      tripleLastX = avgX;
      tripleLastY = avgY;
      tripleStartTime = now;
    } else {
      tripleLastX = avgX;
      tripleLastY = avgY;
    }
    mode = MODE_TRIPLE;
    tapCandidate = false;
    lastTouchTime = now;
    return;
  }

  if (mode == MODE_TRIPLE && !(f1 && f2 && f3)) {
    int16_t dx = tripleLastX - tripleStartX;
    int16_t dy = tripleLastY - tripleStartY;
    unsigned long dt = now - tripleStartTime;
    if (dt <= threeSwipeTimeout && now - lastSwipeTime >= threeSwipeCooldown) {
      if (abs(dx) >= (int16_t)threeSwipeThresholdX && abs(dx) >= abs(dy)) {
        if (dx > 0) {
          performZoneAction(threeRightBinding);
        } else {
          performZoneAction(threeLeftBinding);
        }
        lastSwipeTime = now;
      } else if (abs(dy) >= (int16_t)threeSwipeThresholdY && abs(dy) > abs(dx)) {
        if (dy > 0) {
          performZoneAction(threeDownBinding);
        } else {
          performZoneAction(threeUpBinding);
        }
        lastSwipeTime = now;
      }
    }
    mode = MODE_NONE;
    return;
  }

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
    lastScrollTime = now;
    return;
  }

  /*===== 单指移动 =====*/
  if (f1 && !f2) {
    if (now - lastScrollTime < TAP_GUARD_AFTER_SCROLL_MS) {
      lastX1 = x1;
      lastY1 = y1;
      mode = MODE_SINGLE;
      lastTouchTime = now;
      return;
    }
    if (mode == MODE_SINGLE) {
      int16_t dx = x1 - lastX1;
      int16_t dy = y1 - lastY1;
      dx = constrain(dx, -MAX_DELTA, MAX_DELTA);
      dy = constrain(dy, -MAX_DELTA, MAX_DELTA);
      if (abs(dx) <= MOVE_DEADBAND) dx = 0;
      if (abs(dy) <= MOVE_DEADBAND) dy = 0;

      if (tapCandidate) {
        uint16_t dist = abs(x1 - tapStartX) + abs(y1 - tapStartY);
        if (dist <= DOUBLE_TAP_MAX_MOVE) {
          velX = velY = 0;
          smoothDx = smoothDy = 0;
          accumX = accumY = 0;
          lastX1 = x1;
          lastY1 = y1;
          lastTouchTime = now;
          return;
        }
        tapCandidate = false;
      }

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
    if (now - lastScrollTime < TAP_GUARD_AFTER_SCROLL_MS) {
      tapCandidate = false;
      pendingClick = false;
      mode = MODE_NONE;
      return;
    }
    if (tapCandidate) {
      unsigned long dt = now - tapStartTime;
      if (dt <= TAP_MAX_MS) {
        if (enableNavZones && inLeftTopZone(tapStartX, tapStartY)) {
          performZoneAction(leftTopZone);
          pendingClick = false;
          tapCandidate = false;
          mode = MODE_NONE;
          return;
        }
        if (enableNavZones && inRightTopZone(tapStartX, tapStartY)) {
          performZoneAction(rightTopZone);
          pendingClick = false;
          tapCandidate = false;
          mode = MODE_NONE;
          return;
        }
        if (enableNavZones && inRightBottomZone(tapStartX, tapStartY)) {
          performZoneAction(rightBottomZone);
          pendingClick = false;
          tapCandidate = false;
          mode = MODE_NONE;
          return;
        }
        if (enableNavZones && inLeftBottomZone(tapStartX, tapStartY)) {
          performZoneAction(leftBottomZone);
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
