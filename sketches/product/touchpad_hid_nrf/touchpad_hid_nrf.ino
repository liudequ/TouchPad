#include "touchpad_types.h"

#include <Wire.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>
#include <stdio.h>
#include <string.h>
#if defined(ARDUINO_ARCH_NRF52)
#include <nrf.h>
#include <nrf_gpio.h>
#endif

/*===== I2C-HID =====*/
#define I2C_ADDR 0x2C
#define INPUT_REG_L 0x09
#define INPUT_REG_H 0x01

// nice!nano v2 pin mapping
#define SDA_PIN 9
#define SCL_PIN 8
#define INT_PIN 7
#define TP_EN 6  // TouchPad ENABLE

uint8_t reportBuf[128];
const unsigned long IDLE_SLEEP_MS = 60000;

enum {
  RID_MOUSE = 1,
  RID_KEYBOARD = 2,
};

Adafruit_USBD_HID usb_hid;

uint8_t const hid_report_descriptor[] = {
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(RID_MOUSE)),
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD))
};

// Minimal HID keycodes used by defaults, defined here to avoid core-specific headers.
#ifndef KEYBOARD_MODIFIER_LEFTALT
#define KEYBOARD_MODIFIER_LEFTALT 0x04
#endif
#ifndef HID_KEY_ARROW_LEFT
#define HID_KEY_ARROW_LEFT 0x50
#endif
#ifndef HID_KEY_ARROW_RIGHT
#define HID_KEY_ARROW_RIGHT 0x4F
#endif
#ifndef MOUSE_BUTTON_LEFT
#define MOUSE_BUTTON_LEFT 0x01
#define MOUSE_BUTTON_RIGHT 0x02
#endif

BLEDis bledis;
BLEHidAdafruit blehid;
BLEBas blebas;
using namespace Adafruit_LittleFS_Namespace;

// SoftDevice connection config (increase HVN queue to reduce notify blocking)
static const uint16_t kBleMtuMax = BLE_GATT_ATT_MTU_DEFAULT;
static const uint16_t kBleEventLen = BLE_GAP_EVENT_LENGTH_DEFAULT;
static const uint8_t kBleHvnQueueSize = 6;
static const uint8_t kBleWrCmdQueueSize = 4;

void onConnect(uint16_t conn_handle);
void onDisconnect(uint16_t conn_handle, uint8_t reason);
void startAdv();
void enterDeepSleep();
void initStatusLed();
void setLedPatternActive();
void setLedPatternIdleLight();
void setLedPatternIdleMedium();
void setLedPatternOff();
void updateStatusLed(unsigned long now);

#if defined(ARDUINO_ARCH_NRF52)
extern const uint32_t g_ADigitalPinMap[];
#endif

static Print* cfgOut = &Serial;
static bool lastUsbMounted = false;
static bool lastBleConnected = false;
static bool waitFingerReleaseAfterReconnect = false;
static bool useBleWhenUsb = false;
static bool bleIdleSleepEnabled = false;
static unsigned long bleIdleLightMs = IDLE_SLEEP_MS;
static unsigned long bleIdleMediumMs = IDLE_SLEEP_MS * 3;
static unsigned long bleIdleSleepMs = IDLE_SLEEP_MS * 10;
static uint32_t lightIdleReportIntervalMs = 33;
static uint16_t batteryMilliVolts = 0;
static uint8_t batteryPercent = 0;
static bool batteryReady = false;
static unsigned long lastBatterySampleMs = 0;
static const unsigned long kBatterySampleIntervalMs = 60000;

enum PowerStage {
  POWER_ACTIVE = 0,
  POWER_IDLE_LIGHT = 1,
  POWER_IDLE_MEDIUM = 2,
};

static PowerStage powerStage = POWER_ACTIVE;
static bool advSuppressedByIdle = false;

enum LedPattern {
  LED_PATTERN_OFF = 0,
  LED_PATTERN_ACTIVE_HOLD = 1,
  LED_PATTERN_IDLE_LIGHT = 2,
  LED_PATTERN_IDLE_MEDIUM = 3,
};

static LedPattern ledPattern = LED_PATTERN_OFF;
static unsigned long ledPatternStartMs = 0;
static bool ledStateOn = false;

#if defined(LED_STATE_ON)
static const uint8_t kLedOnLevel = LED_STATE_ON;
#else
static const uint8_t kLedOnLevel = HIGH;
#endif
static const uint8_t kLedOffLevel = (kLedOnLevel == HIGH) ? LOW : HIGH;

bool isUsbMounted() {
  return TinyUSBDevice.mounted();
}

bool useBleTransport() {
  return !isUsbMounted() || useBleWhenUsb;
}

void normalizeIdleThresholds();
uint32_t effectiveReportIntervalMs();
void leaveIdlePowerStage();
void resetGestureState();
void updateConnectionState();
void enterIdleLightStage();
void enterIdleMediumStage();
void handleIdlePower(unsigned long now);
void markActivity();
uint16_t readBatteryMilliVolts();
uint8_t batteryPercentFromMilliVolts(uint16_t mv);
void refreshBatteryStatus(bool forceNotify);
void handleBattery(unsigned long now);
void sendMouseMove(int8_t x, int8_t y);
void sendMouseMoveWithButtons(int8_t x, int8_t y, uint8_t buttons);
void sendMouseWheel(int8_t wheel);
void sendMouseButtons(uint8_t buttons);
void sendMouseClick(uint8_t buttons);
void sendKeyboard(uint8_t modifier, uint8_t keycode);
bool finalizeSingleTouch(unsigned long now, bool releasedByTimeout);
float applySingleFingerAxisResponse(int16_t delta, float axisScale);
float singleFingerAccelForSpeed(float speed);
float applyScrollAxisResponse(int16_t delta);
float scrollAccelForSpeed(float speed);
void handleReport(uint8_t* buf, uint16_t len);
bool waitIntRelease(unsigned long timeoutUs);
void readInputReport();

void writeStatusLed(bool on) {
  if (ledStateOn == on) return;
  ledStateOn = on;
  digitalWrite(LED_BUILTIN, on ? kLedOnLevel : kLedOffLevel);
}

void initStatusLed() {
  pinMode(LED_BUILTIN, OUTPUT);
  writeStatusLed(false);
  ledPattern = LED_PATTERN_OFF;
  ledPatternStartMs = millis();
}

void setLedPatternActive() {
  ledPattern = LED_PATTERN_ACTIVE_HOLD;
  ledPatternStartMs = millis();
  writeStatusLed(true);
}

void setLedPatternIdleLight() {
  ledPattern = LED_PATTERN_IDLE_LIGHT;
  ledPatternStartMs = millis();
}

void setLedPatternIdleMedium() {
  ledPattern = LED_PATTERN_IDLE_MEDIUM;
  ledPatternStartMs = millis();
}

void setLedPatternOff() {
  ledPattern = LED_PATTERN_OFF;
  ledPatternStartMs = millis();
  writeStatusLed(false);
}

void updateStatusLed(unsigned long now) {
  unsigned long phase;
  switch (ledPattern) {
    case LED_PATTERN_ACTIVE_HOLD:
      if (now - ledPatternStartMs < 1000) {
        writeStatusLed(true);
      } else {
        writeStatusLed(false);
      }
      break;
    case LED_PATTERN_IDLE_LIGHT:
      phase = (now - ledPatternStartMs) % 2000;
      writeStatusLed(phase < 60);
      break;
    case LED_PATTERN_IDLE_MEDIUM:
      phase = (now - ledPatternStartMs) % 4000;
      writeStatusLed((phase < 60) || (phase >= 240 && phase < 300));
      break;
    case LED_PATTERN_OFF:
    default:
      writeStatusLed(false);
      break;
  }
}

void rebootDevice() {
#if defined(ARDUINO_ARCH_NRF52)
  delay(20);
  NRF_POWER->GPREGRET = 0xB1;
  NVIC_SystemReset();
#endif
}
/*===== 参数配置区 =====*/
// 单指移动
float sensitivity = 0.3f;
float smoothFactor = 0.5f;
float accelFactor = 0.01f;
float maxAccel = 3.0f;
int16_t maxDelta = 60;
int16_t moveDeadband = 1;

// 双指滚动
float scrollSensitivity = 0.05f;
float scrollSmoothFactor = 0.2f;
bool naturalScroll = true;
const int16_t SCROLL_DEADBAND = 0;
float scrollLowSpeedBoostEnd = 0.35f;
float scrollMinActiveSpeed = 0.18f;
float scrollAccelStartSpeed = 0.45f;
float scrollAccelFactor = 0.90f;
float scrollMaxAccel = 2.50f;
const uint32_t SCROLL_REPORT_INTERVAL_MS = 8;
const float SCROLL_RELEASE_DECAY = 0.72f;
const float SCROLL_VELOCITY_EPSILON = 0.02f;
const float SCROLL_ACCUM_EPSILON = 0.02f;
const int8_t SCROLL_MAX_STEP_PER_REPORT = 1;

// 点击与释放
const unsigned long TAP_MAX_MS = 200;
const unsigned long LONG_PRESS_DRAG_MS = 220;
const unsigned long DOUBLE_TAP_WINDOW = 200;
const uint16_t DOUBLE_TAP_MAX_MOVE = 80;
const unsigned long RELEASE_TIMEOUT = 30;
const unsigned long INT_RELEASE_TIMEOUT_US = 5000;
const unsigned long TAP_GUARD_AFTER_SCROLL_MS = 150;
uint32_t reportIntervalMs = 16;
const float SINGLE_SOFT_DEADBAND_BLEND = 0.35f;
const float SINGLE_ACCEL_START_SPEED = 0.6f;
const float SINGLE_HOLD_DECAY = 0.65f;
const float SINGLE_VELOCITY_EPSILON = 0.01f;

// 三指滑动
uint16_t threeSwipeThresholdX = 200;
uint16_t threeSwipeThresholdY = 200;
uint16_t threeSwipeTimeout = 350;
uint16_t threeSwipeCooldown = 400;
// 四指滑动
uint16_t fourSwipeThresholdX = 200;
uint16_t fourSwipeThresholdY = 200;
uint16_t fourSwipeTimeout = 350;
uint16_t fourSwipeCooldown = 400;

const char* CONFIG_PATH = "/config.txt";

/*===== 状态 =====*/
enum GestureMode { MODE_NONE,
                   MODE_SINGLE,
                   MODE_DOUBLE,
                   MODE_TRIPLE,
                   MODE_QUAD };
GestureMode mode = MODE_NONE;

int16_t lastX1 = 0, lastY1 = 0;
int16_t lastX2 = 0, lastY2 = 0;
unsigned long lastTouchTime = 0;
unsigned long lastSwipeTime = 0;
unsigned long lastFourSwipeTime = 0;
unsigned long lastScrollTime = 0;
unsigned long lastScrollMotionTime = 0;
unsigned long lastActivityMs = 0;

int16_t tripleStartX = 0;
int16_t tripleStartY = 0;
int16_t tripleLastX = 0;
int16_t tripleLastY = 0;
unsigned long tripleStartTime = 0;
bool pendingThreeTap = false;
unsigned long lastThreeTapTime = 0;
int16_t lastThreeTapX = 0;
int16_t lastThreeTapY = 0;

int16_t quadStartX = 0;
int16_t quadStartY = 0;
int16_t quadLastX = 0;
int16_t quadLastY = 0;
unsigned long quadStartTime = 0;
bool pendingFourTap = false;
unsigned long lastFourTapTime = 0;
int16_t lastFourTapX = 0;
int16_t lastFourTapY = 0;

float smoothDx = 0, smoothDy = 0;
float accumX = 0, accumY = 0;
float velX = 0, velY = 0;
unsigned long lastReportMs = 0;

float smoothScroll = 0;
float accumScroll = 0;
float scrollVel = 0;
unsigned long lastScrollReportMs = 0;

bool tapCandidate = false;
bool dragActive = false;
unsigned long tapStartTime = 0;
int16_t tapStartX = 0;
int16_t tapStartY = 0;
bool pendingClick = false;
unsigned long lastTapTime = 0;
int16_t lastTapX = 0;
int16_t lastTapY = 0;

float applySingleFingerAxisResponse(int16_t delta, float axisScale) {
  float scaled = delta * sensitivity * axisScale;
  float magnitude = abs(scaled);
  if (magnitude <= 0.0f) return 0.0f;

  float softDeadband = moveDeadband * sensitivity * axisScale;
  float response = magnitude;
  if (softDeadband > 0.0f) {
    if (magnitude <= softDeadband) {
      response = magnitude * SINGLE_SOFT_DEADBAND_BLEND;
    } else {
      response = (softDeadband * SINGLE_SOFT_DEADBAND_BLEND) + (magnitude - softDeadband);
    }
  }

  return scaled < 0.0f ? -response : response;
}

float singleFingerAccelForSpeed(float speed) {
  float totalMaxAccel = max(1.0f, maxAccel);
  if (speed <= SINGLE_ACCEL_START_SPEED) return 1.0f;

  float extraSpeed = speed - SINGLE_ACCEL_START_SPEED;
  float extraAccel = min(extraSpeed * accelFactor, totalMaxAccel - 1.0f);
  return 1.0f + extraAccel;
}

float applyScrollAxisResponse(int16_t delta) {
  float scaled = delta * scrollSensitivity;
  float magnitude = abs(scaled);
  if (magnitude <= 0.0f) return 0.0f;

  float response = magnitude;
  if (magnitude < scrollLowSpeedBoostEnd) {
    float t = magnitude / scrollLowSpeedBoostEnd;
    response = scrollMinActiveSpeed + (magnitude - scrollMinActiveSpeed) * t;
  }

  return scaled < 0.0f ? -response : response;
}

float scrollAccelForSpeed(float speed) {
  float totalMaxAccel = max(1.0f, scrollMaxAccel);
  if (speed <= scrollAccelStartSpeed) return 1.0f;

  float extraSpeed = speed - scrollAccelStartSpeed;
  float extraAccel = min(extraSpeed * scrollAccelFactor, totalMaxAccel - 1.0f);
  return 1.0f + extraAccel;
}

// 触摸板区域（基于实测坐标范围）
const int16_t TOUCH_MAX_X = 2628;
const int16_t TOUCH_MAX_Y = 1332;
uint8_t TOP_ZONE_PERCENT = 20;
uint8_t SIDE_ZONE_PERCENT = 35;
const float AXIS_SCALE_X = 1.0f;
const float AXIS_SCALE_Y = 1.0f;

bool enableNavZones = true;
ZoneBinding leftTopZone = { ZONE_KEYBOARD, 0, KEYBOARD_MODIFIER_LEFTALT, HID_KEY_ARROW_LEFT };
ZoneBinding rightTopZone = { ZONE_KEYBOARD, 0, KEYBOARD_MODIFIER_LEFTALT, HID_KEY_ARROW_RIGHT };
ZoneBinding rightBottomZone = { ZONE_MOUSE, MOUSE_BUTTON_RIGHT, 0, 0 };
ZoneBinding leftBottomZone = { ZONE_NONE, 0, 0, 0 };

ZoneBinding threeLeftBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding threeRightBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding threeUpBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding threeDownBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding threeTapBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding threeDoubleTapBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding fourLeftBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding fourRightBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding fourUpBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding fourDownBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding fourTapBinding = { ZONE_NONE, 0, 0, 0 };
ZoneBinding fourDoubleTapBinding = { ZONE_NONE, 0, 0, 0 };

void resetGestureState() {
  unsigned long now = millis();
  mode = MODE_NONE;
  lastX1 = lastY1 = 0;
  lastX2 = lastY2 = 0;
  lastTouchTime = now;
  lastScrollTime = now;
  lastScrollMotionTime = 0;

  tripleStartX = tripleStartY = 0;
  tripleLastX = tripleLastY = 0;
  tripleStartTime = 0;
  pendingThreeTap = false;
  lastThreeTapTime = 0;
  lastThreeTapX = lastThreeTapY = 0;

  quadStartX = quadStartY = 0;
  quadLastX = quadLastY = 0;
  quadStartTime = 0;
  pendingFourTap = false;
  lastFourTapTime = 0;
  lastFourTapX = lastFourTapY = 0;

  smoothDx = smoothDy = 0;
  accumX = accumY = 0;
  velX = velY = 0;
  lastReportMs = now;

  smoothScroll = 0;
  accumScroll = 0;
  scrollVel = 0;
  lastScrollReportMs = now;

  tapCandidate = false;
  dragActive = false;
  tapStartTime = 0;
  tapStartX = tapStartY = 0;
  pendingClick = false;
  lastTapTime = 0;
  lastTapX = lastTapY = 0;
}

#include "modules/touchpad_slot_manager.h"
#include "modules/touchpad_power_battery.h"

void applyDefaults() {
  sensitivity = 0.3f;
  smoothFactor = 0.5f;
  accelFactor = 0.01f;
  maxAccel = 3.0f;
  maxDelta = 60;
  moveDeadband = 1;
  scrollSensitivity = 0.05f;
  scrollLowSpeedBoostEnd = 0.35f;
  scrollMinActiveSpeed = 0.18f;
  scrollAccelStartSpeed = 0.45f;
  scrollAccelFactor = 0.90f;
  scrollMaxAccel = 2.50f;
  TOP_ZONE_PERCENT = 20;
  SIDE_ZONE_PERCENT = 35;
  enableNavZones = true;
  useBleWhenUsb = false;
  bleIdleSleepEnabled = false;
  bleIdleLightMs = IDLE_SLEEP_MS;
  bleIdleMediumMs = IDLE_SLEEP_MS * 3;
  bleIdleSleepMs = IDLE_SLEEP_MS * 10;
  lightIdleReportIntervalMs = 33;
  leftTopZone = { ZONE_KEYBOARD, 0, KEYBOARD_MODIFIER_LEFTALT, HID_KEY_ARROW_LEFT };
  rightTopZone = { ZONE_KEYBOARD, 0, KEYBOARD_MODIFIER_LEFTALT, HID_KEY_ARROW_RIGHT };
  rightBottomZone = { ZONE_MOUSE, MOUSE_BUTTON_RIGHT, 0, 0 };
  leftBottomZone = { ZONE_NONE, 0, 0, 0 };
  threeSwipeThresholdX = 200;
  threeSwipeThresholdY = 200;
  threeSwipeTimeout = 350;
  threeSwipeCooldown = 400;
  fourSwipeThresholdX = 200;
  fourSwipeThresholdY = 200;
  fourSwipeTimeout = 350;
  fourSwipeCooldown = 400;
  threeLeftBinding = { ZONE_NONE, 0, 0, 0 };
  threeRightBinding = { ZONE_NONE, 0, 0, 0 };
  threeUpBinding = { ZONE_NONE, 0, 0, 0 };
  threeDownBinding = { ZONE_NONE, 0, 0, 0 };
  threeTapBinding = { ZONE_NONE, 0, 0, 0 };
  threeDoubleTapBinding = { ZONE_NONE, 0, 0, 0 };
  fourLeftBinding = { ZONE_NONE, 0, 0, 0 };
  fourRightBinding = { ZONE_NONE, 0, 0, 0 };
  fourUpBinding = { ZONE_NONE, 0, 0, 0 };
  fourDownBinding = { ZONE_NONE, 0, 0, 0 };
  fourTapBinding = { ZONE_NONE, 0, 0, 0 };
  fourDoubleTapBinding = { ZONE_NONE, 0, 0, 0 };
  pendingClick = false;
  pendingThreeTap = false;
  pendingFourTap = false;
  slotManagerEnabled = false;
  slotResetAll();
  normalizeIdleThresholds();
}

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

void initI2C() {
#if defined(ARDUINO_ARCH_NRF52)
  Wire.setPins(SDA_PIN, SCL_PIN);
#else
  Wire.setSDA(SDA_PIN);
  Wire.setSCL(SCL_PIN);
#endif
  Wire.begin();
  Wire.setClock(400000);
  delay(50);
}

void initUsbHid() {
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }
  usb_hid.setReportDescriptor(hid_report_descriptor,
                              sizeof(hid_report_descriptor));
  usb_hid.begin();
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

#include "modules/touchpad_ble_transport.h"

void enterDeepSleep() {
#if defined(ARDUINO_ARCH_NRF52)
  setLedPatternOff();
  for (uint8_t i = 0; i < 3; i++) {
    writeStatusLed(true);
    delay(80);
    writeStatusLed(false);
    delay(80);
  }
  if (Bluefruit.connected()) {
    Bluefruit.disconnect(0);
    delay(200);
  }
  Bluefruit.Advertising.stop();

  uint32_t wake_pin = g_ADigitalPinMap[INT_PIN];
  nrf_gpio_cfg_sense_input(wake_pin, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
  delay(5);
  NRF_POWER->SYSTEMOFF = 1;
  while (true) {
    __WFE();
  }
#endif
}

void onConnect(uint16_t conn_handle) {
  markActivity();
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  if (connection) {
    connection->requestConnectionParameter(12, 0, 400);
  }
  slotOnConnect(conn_handle);
}

void onDisconnect(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  (void)reason;
  slotOnDisconnect();
  if (advSuppressedByIdle) {
    Bluefruit.Advertising.stop();
  }
}

void setup() {
  Serial.begin(115200);
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart < 2000)) {
    delay(10);
  }

  // ★★★ 冷启动修复关键 ★★★
  touchColdBoot();

  // INT 必须提前配置，防止悬空
  pinMode(INT_PIN, INPUT_PULLUP);
  initStatusLed();

  // I2C 初始化（延后）
  initI2C();

  applyDefaults();
  bool fsMounted = InternalFS.begin();
  if (!fsMounted) {
    // Only format when mount fails to avoid wiping bond keys every boot.
    if (InternalFS.format()) {
      fsMounted = InternalFS.begin();
    }
  }
  if (fsMounted) {
    loadConfig();
  }

  initUsbHid();
  initBle();
  updateTransport();
  markActivity();
  setLedPatternActive();
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
  updateTransport();
  updateConnectionState();
  if (digitalRead(INT_PIN) == LOW) {
    readInputReport();
  }
  unsigned long now = millis();
  handleBattery(now);
  updateStatusLed(now);
  handleIdlePower(now);
  now = millis();
  updateStatusLed(now);

  /*单指超时释放*/
  if (mode == MODE_SINGLE && now - lastTouchTime > RELEASE_TIMEOUT) {
    finalizeSingleTouch(now, true);
  }

  /*双指超时释放*/
  if (mode == MODE_DOUBLE && now - lastTouchTime > RELEASE_TIMEOUT) {
    mode = MODE_NONE;
    lastScrollTime = now;
  }

  if (mode == MODE_TRIPLE && now - lastTouchTime > RELEASE_TIMEOUT) {
    mode = MODE_NONE;
  }
  if (mode == MODE_QUAD && now - lastTouchTime > RELEASE_TIMEOUT) {
    mode = MODE_NONE;
  }

  /*连续输出*/
  if (mode == MODE_SINGLE && (!tapCandidate || dragActive)) {
    uint32_t intervalMs = effectiveReportIntervalMs();
    if (now - lastReportMs >= intervalMs) {
      unsigned long dtMs = now - lastReportMs;
      lastReportMs = now;
      float dt = dtMs / (float)intervalMs;
      if (dt > 1.5f) dt = 1.0f;
      accumX += velX * dt;
      accumY += velY * dt;
      int16_t mx16 = (int16_t)accumX;
      int16_t my16 = (int16_t)accumY;
      mx16 = constrain(mx16, -127, 127);
      my16 = constrain(my16, -127, 127);
      int8_t mx = (int8_t)mx16;
      int8_t my = (int8_t)my16;
      if (mx || my) {
        if (dragActive) {
          sendMouseMoveWithButtons(mx, my, MOUSE_BUTTON_LEFT);
        } else {
          sendMouseMove(mx, my);
        }
        accumX -= mx;
        accumY -= my;
      }
    }
  }

  bool scrollActive = (mode == MODE_DOUBLE) || (abs(scrollVel) > SCROLL_VELOCITY_EPSILON) || (abs(accumScroll) > 1.0f);
  if (scrollActive && now - lastScrollReportMs >= SCROLL_REPORT_INTERVAL_MS) {
    unsigned long dtMs = now - lastScrollReportMs;
    lastScrollReportMs = now;
    float dt = dtMs / (float)SCROLL_REPORT_INTERVAL_MS;
    if (dt > 2.0f) dt = 2.0f;

    if (mode != MODE_DOUBLE) {
      scrollVel *= SCROLL_RELEASE_DECAY;
      smoothScroll *= SCROLL_RELEASE_DECAY;
      if (abs(scrollVel) < SCROLL_VELOCITY_EPSILON) scrollVel = 0;
      if (abs(smoothScroll) < SCROLL_VELOCITY_EPSILON) smoothScroll = 0;
    }

    accumScroll += scrollVel * dt;

    int8_t s = 0;
    if (accumScroll >= 1.0f) {
      s = SCROLL_MAX_STEP_PER_REPORT;
    } else if (accumScroll <= -1.0f) {
      s = -SCROLL_MAX_STEP_PER_REPORT;
    }

    if (s) {
      sendMouseWheel(naturalScroll ? s : -s);
      accumScroll -= s;
      lastScrollMotionTime = now;
    } else if (mode != MODE_DOUBLE && abs(accumScroll) < SCROLL_ACCUM_EPSILON) {
      accumScroll = 0;
    }
  }

  if (pendingClick && now - lastTapTime > DOUBLE_TAP_WINDOW) {
    sendMouseClick(MOUSE_BUTTON_LEFT);
    pendingClick = false;
  }
  if (pendingThreeTap && now - lastThreeTapTime > DOUBLE_TAP_WINDOW) {
    performZoneAction(threeTapBinding);
    pendingThreeTap = false;
  }
  if (pendingFourTap && now - lastFourTapTime > DOUBLE_TAP_WINDOW) {
    performZoneAction(fourTapBinding);
    pendingFourTap = false;
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
  markActivity();

  if (line.equalsIgnoreCase("HELP")) {
    cfgOut->println("CMD: GET scrollSensitivity");
    cfgOut->println("CMD: GET");
    cfgOut->println("CMD: SET <key> <value>");
    cfgOut->println("CMD: SAVE");
    cfgOut->println("CMD: LOAD");
    cfgOut->println("CMD: RESET");
    cfgOut->println("CMD: BOOT");
    cfgOut->println("CMD: INFO SLOT");
    cfgOut->println("CMD: PAIRCLR");
    cfgOut->println("CMD: SLOT <1|2|3>");
    cfgOut->println("CMD: PAIR SLOT <1|2|3>");
    cfgOut->println("CMD: PAIR SLOT <1|2|3> FORCE");
    cfgOut->println("CMD: UNPAIR SLOT <1|2|3>");
    cfgOut->println("CMD: GET useBleWhenUsb");
    cfgOut->println("CMD: GET bleIdleSleepEnabled");
    cfgOut->println("CMD: GET slotManagerEnabled");
    cfgOut->println("CMD: GET slot");
    cfgOut->println("CMD: GET bleIdleLightMs");
    cfgOut->println("CMD: GET bleIdleMediumMs");
    cfgOut->println("CMD: GET bleIdleSleepMs");
    cfgOut->println("CMD: GET lightIdleRate");
    cfgOut->println("CMD: GET scrollLowSpeedBoostEnd");
    cfgOut->println("CMD: GET scrollMinActiveSpeed");
    cfgOut->println("CMD: GET scrollAccelStartSpeed");
    cfgOut->println("CMD: GET scrollAccelFactor");
    cfgOut->println("CMD: GET scrollMaxAccel");
    cfgOut->println("CMD: GET battery");
    return;
  }

  if (line.startsWith("RATE ")) {
    int v = line.substring(5).toInt();
    if (v >= 10 && v <= 200) {
      reportIntervalMs = (uint32_t)(1000 / v);
      if (reportIntervalMs == 0) reportIntervalMs = 1;
      normalizeIdleThresholds();
    }
    return;
  }

  if (line.equalsIgnoreCase("GET")) {
    cfgOut->print("sensitivity=");
    cfgOut->println(sensitivity, 6);
    cfgOut->print("smoothFactor=");
    cfgOut->println(smoothFactor, 6);
    cfgOut->print("accelFactor=");
    cfgOut->println(accelFactor, 6);
    cfgOut->print("maxAccel=");
    cfgOut->println(maxAccel, 6);
    cfgOut->print("maxDelta=");
    cfgOut->println(maxDelta);
    cfgOut->print("moveDeadband=");
    cfgOut->println(moveDeadband);
    cfgOut->print("rate=");
    cfgOut->println(reportIntervalMs ? (1000 / reportIntervalMs) : 0);
    cfgOut->print("scrollSensitivity=");
    cfgOut->println(scrollSensitivity, 8);
    cfgOut->print("scrollLowSpeedBoostEnd=");
    cfgOut->println(scrollLowSpeedBoostEnd, 6);
    cfgOut->print("scrollMinActiveSpeed=");
    cfgOut->println(scrollMinActiveSpeed, 6);
    cfgOut->print("scrollAccelStartSpeed=");
    cfgOut->println(scrollAccelStartSpeed, 6);
    cfgOut->print("scrollAccelFactor=");
    cfgOut->println(scrollAccelFactor, 6);
    cfgOut->print("scrollMaxAccel=");
    cfgOut->println(scrollMaxAccel, 6);
    cfgOut->print("topZonePercent=");
    cfgOut->println(TOP_ZONE_PERCENT);
    cfgOut->print("sideZonePercent=");
    cfgOut->println(SIDE_ZONE_PERCENT);
    cfgOut->print("enableNavZones=");
    cfgOut->println(enableNavZones ? "1" : "0");
    cfgOut->print("useBleWhenUsb=");
    cfgOut->println(useBleWhenUsb ? "1" : "0");
    cfgOut->print("bleIdleSleepEnabled=");
    cfgOut->println(bleIdleSleepEnabled ? "1" : "0");
    cfgOut->print("bleIdleLightMs=");
    cfgOut->println(bleIdleLightMs);
    cfgOut->print("bleIdleMediumMs=");
    cfgOut->println(bleIdleMediumMs);
    cfgOut->print("bleIdleSleepMs=");
    cfgOut->println(bleIdleSleepMs);
    cfgOut->print("lightIdleRate=");
    cfgOut->println(lightIdleReportIntervalMs ? (1000 / lightIdleReportIntervalMs) : 0);
    if (!batteryReady) {
      refreshBatteryStatus(false);
    }
    cfgOut->print("batteryMv=");
    cfgOut->println(batteryMilliVolts);
    cfgOut->print("battery=");
    cfgOut->println(batteryPercent);
    slotPrintStatus();
    cfgOut->print("leftTopType=");
    cfgOut->println(typeToString(leftTopZone.type));
    cfgOut->print("leftTopButtons=");
    cfgOut->println(leftTopZone.mouseButtons);
    cfgOut->print("leftTopModifier=");
    cfgOut->println(leftTopZone.keyModifier);
    cfgOut->print("leftTopKey=");
    cfgOut->println(leftTopZone.keyCode);
    cfgOut->print("rightTopType=");
    cfgOut->println(typeToString(rightTopZone.type));
    cfgOut->print("rightTopButtons=");
    cfgOut->println(rightTopZone.mouseButtons);
    cfgOut->print("rightTopModifier=");
    cfgOut->println(rightTopZone.keyModifier);
    cfgOut->print("rightTopKey=");
    cfgOut->println(rightTopZone.keyCode);
    cfgOut->print("rightBottomType=");
    cfgOut->println(typeToString(rightBottomZone.type));
    cfgOut->print("rightBottomButtons=");
    cfgOut->println(rightBottomZone.mouseButtons);
    cfgOut->print("rightBottomModifier=");
    cfgOut->println(rightBottomZone.keyModifier);
    cfgOut->print("rightBottomKey=");
    cfgOut->println(rightBottomZone.keyCode);
    cfgOut->print("leftBottomType=");
    cfgOut->println(typeToString(leftBottomZone.type));
    cfgOut->print("leftBottomButtons=");
    cfgOut->println(leftBottomZone.mouseButtons);
    cfgOut->print("leftBottomModifier=");
    cfgOut->println(leftBottomZone.keyModifier);
    cfgOut->print("leftBottomKey=");
    cfgOut->println(leftBottomZone.keyCode);
    cfgOut->print("threeLeftType=");
    cfgOut->println(typeToString(threeLeftBinding.type));
    cfgOut->print("threeLeftButtons=");
    cfgOut->println(threeLeftBinding.mouseButtons);
    cfgOut->print("threeLeftModifier=");
    cfgOut->println(threeLeftBinding.keyModifier);
    cfgOut->print("threeLeftKey=");
    cfgOut->println(threeLeftBinding.keyCode);
    cfgOut->print("threeRightType=");
    cfgOut->println(typeToString(threeRightBinding.type));
    cfgOut->print("threeRightButtons=");
    cfgOut->println(threeRightBinding.mouseButtons);
    cfgOut->print("threeRightModifier=");
    cfgOut->println(threeRightBinding.keyModifier);
    cfgOut->print("threeRightKey=");
    cfgOut->println(threeRightBinding.keyCode);
    cfgOut->print("threeUpType=");
    cfgOut->println(typeToString(threeUpBinding.type));
    cfgOut->print("threeUpButtons=");
    cfgOut->println(threeUpBinding.mouseButtons);
    cfgOut->print("threeUpModifier=");
    cfgOut->println(threeUpBinding.keyModifier);
    cfgOut->print("threeUpKey=");
    cfgOut->println(threeUpBinding.keyCode);
    cfgOut->print("threeDownType=");
    cfgOut->println(typeToString(threeDownBinding.type));
    cfgOut->print("threeDownButtons=");
    cfgOut->println(threeDownBinding.mouseButtons);
    cfgOut->print("threeDownModifier=");
    cfgOut->println(threeDownBinding.keyModifier);
    cfgOut->print("threeDownKey=");
    cfgOut->println(threeDownBinding.keyCode);
    cfgOut->print("threeTapType=");
    cfgOut->println(typeToString(threeTapBinding.type));
    cfgOut->print("threeTapButtons=");
    cfgOut->println(threeTapBinding.mouseButtons);
    cfgOut->print("threeTapModifier=");
    cfgOut->println(threeTapBinding.keyModifier);
    cfgOut->print("threeTapKey=");
    cfgOut->println(threeTapBinding.keyCode);
    cfgOut->print("threeDoubleTapType=");
    cfgOut->println(typeToString(threeDoubleTapBinding.type));
    cfgOut->print("threeDoubleTapButtons=");
    cfgOut->println(threeDoubleTapBinding.mouseButtons);
    cfgOut->print("threeDoubleTapModifier=");
    cfgOut->println(threeDoubleTapBinding.keyModifier);
    cfgOut->print("threeDoubleTapKey=");
    cfgOut->println(threeDoubleTapBinding.keyCode);
    cfgOut->print("threeSwipeThresholdX=");
    cfgOut->println(threeSwipeThresholdX);
    cfgOut->print("threeSwipeThresholdY=");
    cfgOut->println(threeSwipeThresholdY);
    cfgOut->print("threeSwipeTimeout=");
    cfgOut->println(threeSwipeTimeout);
    cfgOut->print("threeSwipeCooldown=");
    cfgOut->println(threeSwipeCooldown);
    cfgOut->print("fourLeftType=");
    cfgOut->println(typeToString(fourLeftBinding.type));
    cfgOut->print("fourLeftButtons=");
    cfgOut->println(fourLeftBinding.mouseButtons);
    cfgOut->print("fourLeftModifier=");
    cfgOut->println(fourLeftBinding.keyModifier);
    cfgOut->print("fourLeftKey=");
    cfgOut->println(fourLeftBinding.keyCode);
    cfgOut->print("fourRightType=");
    cfgOut->println(typeToString(fourRightBinding.type));
    cfgOut->print("fourRightButtons=");
    cfgOut->println(fourRightBinding.mouseButtons);
    cfgOut->print("fourRightModifier=");
    cfgOut->println(fourRightBinding.keyModifier);
    cfgOut->print("fourRightKey=");
    cfgOut->println(fourRightBinding.keyCode);
    cfgOut->print("fourUpType=");
    cfgOut->println(typeToString(fourUpBinding.type));
    cfgOut->print("fourUpButtons=");
    cfgOut->println(fourUpBinding.mouseButtons);
    cfgOut->print("fourUpModifier=");
    cfgOut->println(fourUpBinding.keyModifier);
    cfgOut->print("fourUpKey=");
    cfgOut->println(fourUpBinding.keyCode);
    cfgOut->print("fourDownType=");
    cfgOut->println(typeToString(fourDownBinding.type));
    cfgOut->print("fourDownButtons=");
    cfgOut->println(fourDownBinding.mouseButtons);
    cfgOut->print("fourDownModifier=");
    cfgOut->println(fourDownBinding.keyModifier);
    cfgOut->print("fourDownKey=");
    cfgOut->println(fourDownBinding.keyCode);
    cfgOut->print("fourTapType=");
    cfgOut->println(typeToString(fourTapBinding.type));
    cfgOut->print("fourTapButtons=");
    cfgOut->println(fourTapBinding.mouseButtons);
    cfgOut->print("fourTapModifier=");
    cfgOut->println(fourTapBinding.keyModifier);
    cfgOut->print("fourTapKey=");
    cfgOut->println(fourTapBinding.keyCode);
    cfgOut->print("fourDoubleTapType=");
    cfgOut->println(typeToString(fourDoubleTapBinding.type));
    cfgOut->print("fourDoubleTapButtons=");
    cfgOut->println(fourDoubleTapBinding.mouseButtons);
    cfgOut->print("fourDoubleTapModifier=");
    cfgOut->println(fourDoubleTapBinding.keyModifier);
    cfgOut->print("fourDoubleTapKey=");
    cfgOut->println(fourDoubleTapBinding.keyCode);
    cfgOut->print("fourSwipeThresholdX=");
    cfgOut->println(fourSwipeThresholdX);
    cfgOut->print("fourSwipeThresholdY=");
    cfgOut->println(fourSwipeThresholdY);
    cfgOut->print("fourSwipeTimeout=");
    cfgOut->println(fourSwipeTimeout);
    cfgOut->print("fourSwipeCooldown=");
    cfgOut->println(fourSwipeCooldown);
    return;
  }

  if (line.startsWith("GET ")) {
    String key = line.substring(4);
    key.trim();
    if (key.equalsIgnoreCase("scrollSensitivity")) {
      cfgOut->print("scrollSensitivity=");
      cfgOut->println(scrollSensitivity, 8);
      return;
    }
    if (key.equalsIgnoreCase("scrollLowSpeedBoostEnd")) {
      cfgOut->print("scrollLowSpeedBoostEnd=");
      cfgOut->println(scrollLowSpeedBoostEnd, 6);
      return;
    }
    if (key.equalsIgnoreCase("scrollMinActiveSpeed")) {
      cfgOut->print("scrollMinActiveSpeed=");
      cfgOut->println(scrollMinActiveSpeed, 6);
      return;
    }
    if (key.equalsIgnoreCase("scrollAccelStartSpeed")) {
      cfgOut->print("scrollAccelStartSpeed=");
      cfgOut->println(scrollAccelStartSpeed, 6);
      return;
    }
    if (key.equalsIgnoreCase("scrollAccelFactor")) {
      cfgOut->print("scrollAccelFactor=");
      cfgOut->println(scrollAccelFactor, 6);
      return;
    }
    if (key.equalsIgnoreCase("scrollMaxAccel")) {
      cfgOut->print("scrollMaxAccel=");
      cfgOut->println(scrollMaxAccel, 6);
      return;
    }
    if (key.equalsIgnoreCase("sensitivity")) {
      cfgOut->print("sensitivity=");
      cfgOut->println(sensitivity, 6);
      return;
    }
    if (key.equalsIgnoreCase("smoothFactor")) {
      cfgOut->print("smoothFactor=");
      cfgOut->println(smoothFactor, 6);
      return;
    }
    if (key.equalsIgnoreCase("accelFactor")) {
      cfgOut->print("accelFactor=");
      cfgOut->println(accelFactor, 6);
      return;
    }
    if (key.equalsIgnoreCase("maxAccel")) {
      cfgOut->print("maxAccel=");
      cfgOut->println(maxAccel, 6);
      return;
    }
    if (key.equalsIgnoreCase("maxDelta")) {
      cfgOut->print("maxDelta=");
      cfgOut->println(maxDelta);
      return;
    }
    if (key.equalsIgnoreCase("moveDeadband")) {
      cfgOut->print("moveDeadband=");
      cfgOut->println(moveDeadband);
      return;
    }
    if (key.equalsIgnoreCase("rate")) {
      cfgOut->print("rate=");
      cfgOut->println(reportIntervalMs ? (1000 / reportIntervalMs) : 0);
      return;
    }
    if (key.equalsIgnoreCase("topZonePercent")) {
      cfgOut->print("topZonePercent=");
      cfgOut->println(TOP_ZONE_PERCENT);
      return;
    }
    if (key.equalsIgnoreCase("sideZonePercent")) {
      cfgOut->print("sideZonePercent=");
      cfgOut->println(SIDE_ZONE_PERCENT);
      return;
    }
    if (key.equalsIgnoreCase("enableNavZones")) {
      cfgOut->print("enableNavZones=");
      cfgOut->println(enableNavZones ? "1" : "0");
      return;
    }
    if (key.equalsIgnoreCase("useBleWhenUsb")) {
      cfgOut->print("useBleWhenUsb=");
      cfgOut->println(useBleWhenUsb ? "1" : "0");
      return;
    }
    if (key.equalsIgnoreCase("slotManagerEnabled")) {
      cfgOut->print("slotManagerEnabled=");
      cfgOut->println(slotManagerEnabled ? "1" : "0");
      return;
    }
    if (key.equalsIgnoreCase("slot")) {
      slotPrintStatus();
      return;
    }
    if (key.equalsIgnoreCase("bleIdleSleepEnabled")) {
      cfgOut->print("bleIdleSleepEnabled=");
      cfgOut->println(bleIdleSleepEnabled ? "1" : "0");
      return;
    }
    if (key.equalsIgnoreCase("bleIdleLightMs")) {
      cfgOut->print("bleIdleLightMs=");
      cfgOut->println(bleIdleLightMs);
      return;
    }
    if (key.equalsIgnoreCase("bleIdleMediumMs")) {
      cfgOut->print("bleIdleMediumMs=");
      cfgOut->println(bleIdleMediumMs);
      return;
    }
    if (key.equalsIgnoreCase("bleIdleSleepMs")) {
      cfgOut->print("bleIdleSleepMs=");
      cfgOut->println(bleIdleSleepMs);
      return;
    }
    if (key.equalsIgnoreCase("lightIdleRate")) {
      cfgOut->print("lightIdleRate=");
      cfgOut->println(lightIdleReportIntervalMs ? (1000 / lightIdleReportIntervalMs) : 0);
      return;
    }
    if (key.equalsIgnoreCase("battery")) {
      refreshBatteryStatus(false);
      cfgOut->print("batteryMv=");
      cfgOut->println(batteryMilliVolts);
      cfgOut->print("battery=");
      cfgOut->println(batteryPercent);
      return;
    }
    if (key.equalsIgnoreCase("leftTopType")) {
      cfgOut->print("leftTopType=");
      cfgOut->println(typeToString(leftTopZone.type));
      return;
    }
    if (key.equalsIgnoreCase("leftTopButtons")) {
      cfgOut->print("leftTopButtons=");
      cfgOut->println(leftTopZone.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("leftTopModifier")) {
      cfgOut->print("leftTopModifier=");
      cfgOut->println(leftTopZone.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("leftTopKey")) {
      cfgOut->print("leftTopKey=");
      cfgOut->println(leftTopZone.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("rightTopType")) {
      cfgOut->print("rightTopType=");
      cfgOut->println(typeToString(rightTopZone.type));
      return;
    }
    if (key.equalsIgnoreCase("rightTopButtons")) {
      cfgOut->print("rightTopButtons=");
      cfgOut->println(rightTopZone.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("rightTopModifier")) {
      cfgOut->print("rightTopModifier=");
      cfgOut->println(rightTopZone.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("rightTopKey")) {
      cfgOut->print("rightTopKey=");
      cfgOut->println(rightTopZone.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("rightBottomType")) {
      cfgOut->print("rightBottomType=");
      cfgOut->println(typeToString(rightBottomZone.type));
      return;
    }
    if (key.equalsIgnoreCase("rightBottomButtons")) {
      cfgOut->print("rightBottomButtons=");
      cfgOut->println(rightBottomZone.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("rightBottomModifier")) {
      cfgOut->print("rightBottomModifier=");
      cfgOut->println(rightBottomZone.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("rightBottomKey")) {
      cfgOut->print("rightBottomKey=");
      cfgOut->println(rightBottomZone.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("leftBottomType")) {
      cfgOut->print("leftBottomType=");
      cfgOut->println(typeToString(leftBottomZone.type));
      return;
    }
    if (key.equalsIgnoreCase("leftBottomButtons")) {
      cfgOut->print("leftBottomButtons=");
      cfgOut->println(leftBottomZone.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("leftBottomModifier")) {
      cfgOut->print("leftBottomModifier=");
      cfgOut->println(leftBottomZone.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("leftBottomKey")) {
      cfgOut->print("leftBottomKey=");
      cfgOut->println(leftBottomZone.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("threeLeftType")) {
      cfgOut->print("threeLeftType=");
      cfgOut->println(typeToString(threeLeftBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("threeLeftButtons")) {
      cfgOut->print("threeLeftButtons=");
      cfgOut->println(threeLeftBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("threeLeftModifier")) {
      cfgOut->print("threeLeftModifier=");
      cfgOut->println(threeLeftBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("threeLeftKey")) {
      cfgOut->print("threeLeftKey=");
      cfgOut->println(threeLeftBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("threeRightType")) {
      cfgOut->print("threeRightType=");
      cfgOut->println(typeToString(threeRightBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("threeRightButtons")) {
      cfgOut->print("threeRightButtons=");
      cfgOut->println(threeRightBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("threeRightModifier")) {
      cfgOut->print("threeRightModifier=");
      cfgOut->println(threeRightBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("threeRightKey")) {
      cfgOut->print("threeRightKey=");
      cfgOut->println(threeRightBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("threeUpType")) {
      cfgOut->print("threeUpType=");
      cfgOut->println(typeToString(threeUpBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("threeUpButtons")) {
      cfgOut->print("threeUpButtons=");
      cfgOut->println(threeUpBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("threeUpModifier")) {
      cfgOut->print("threeUpModifier=");
      cfgOut->println(threeUpBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("threeUpKey")) {
      cfgOut->print("threeUpKey=");
      cfgOut->println(threeUpBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("threeDownType")) {
      cfgOut->print("threeDownType=");
      cfgOut->println(typeToString(threeDownBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("threeDownButtons")) {
      cfgOut->print("threeDownButtons=");
      cfgOut->println(threeDownBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("threeDownModifier")) {
      cfgOut->print("threeDownModifier=");
      cfgOut->println(threeDownBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("threeDownKey")) {
      cfgOut->print("threeDownKey=");
      cfgOut->println(threeDownBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("threeTapType")) {
      cfgOut->print("threeTapType=");
      cfgOut->println(typeToString(threeTapBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("threeTapButtons")) {
      cfgOut->print("threeTapButtons=");
      cfgOut->println(threeTapBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("threeTapModifier")) {
      cfgOut->print("threeTapModifier=");
      cfgOut->println(threeTapBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("threeTapKey")) {
      cfgOut->print("threeTapKey=");
      cfgOut->println(threeTapBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("threeDoubleTapType")) {
      cfgOut->print("threeDoubleTapType=");
      cfgOut->println(typeToString(threeDoubleTapBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("threeDoubleTapButtons")) {
      cfgOut->print("threeDoubleTapButtons=");
      cfgOut->println(threeDoubleTapBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("threeDoubleTapModifier")) {
      cfgOut->print("threeDoubleTapModifier=");
      cfgOut->println(threeDoubleTapBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("threeDoubleTapKey")) {
      cfgOut->print("threeDoubleTapKey=");
      cfgOut->println(threeDoubleTapBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeThresholdX")) {
      cfgOut->print("threeSwipeThresholdX=");
      cfgOut->println(threeSwipeThresholdX);
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeThresholdY")) {
      cfgOut->print("threeSwipeThresholdY=");
      cfgOut->println(threeSwipeThresholdY);
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeTimeout")) {
      cfgOut->print("threeSwipeTimeout=");
      cfgOut->println(threeSwipeTimeout);
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeCooldown")) {
      cfgOut->print("threeSwipeCooldown=");
      cfgOut->println(threeSwipeCooldown);
      return;
    }
    if (key.equalsIgnoreCase("fourLeftType")) {
      cfgOut->print("fourLeftType=");
      cfgOut->println(typeToString(fourLeftBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("fourLeftButtons")) {
      cfgOut->print("fourLeftButtons=");
      cfgOut->println(fourLeftBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("fourLeftModifier")) {
      cfgOut->print("fourLeftModifier=");
      cfgOut->println(fourLeftBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("fourLeftKey")) {
      cfgOut->print("fourLeftKey=");
      cfgOut->println(fourLeftBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("fourRightType")) {
      cfgOut->print("fourRightType=");
      cfgOut->println(typeToString(fourRightBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("fourRightButtons")) {
      cfgOut->print("fourRightButtons=");
      cfgOut->println(fourRightBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("fourRightModifier")) {
      cfgOut->print("fourRightModifier=");
      cfgOut->println(fourRightBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("fourRightKey")) {
      cfgOut->print("fourRightKey=");
      cfgOut->println(fourRightBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("fourUpType")) {
      cfgOut->print("fourUpType=");
      cfgOut->println(typeToString(fourUpBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("fourUpButtons")) {
      cfgOut->print("fourUpButtons=");
      cfgOut->println(fourUpBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("fourUpModifier")) {
      cfgOut->print("fourUpModifier=");
      cfgOut->println(fourUpBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("fourUpKey")) {
      cfgOut->print("fourUpKey=");
      cfgOut->println(fourUpBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("fourDownType")) {
      cfgOut->print("fourDownType=");
      cfgOut->println(typeToString(fourDownBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("fourDownButtons")) {
      cfgOut->print("fourDownButtons=");
      cfgOut->println(fourDownBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("fourDownModifier")) {
      cfgOut->print("fourDownModifier=");
      cfgOut->println(fourDownBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("fourDownKey")) {
      cfgOut->print("fourDownKey=");
      cfgOut->println(fourDownBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("fourTapType")) {
      cfgOut->print("fourTapType=");
      cfgOut->println(typeToString(fourTapBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("fourTapButtons")) {
      cfgOut->print("fourTapButtons=");
      cfgOut->println(fourTapBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("fourTapModifier")) {
      cfgOut->print("fourTapModifier=");
      cfgOut->println(fourTapBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("fourTapKey")) {
      cfgOut->print("fourTapKey=");
      cfgOut->println(fourTapBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("fourDoubleTapType")) {
      cfgOut->print("fourDoubleTapType=");
      cfgOut->println(typeToString(fourDoubleTapBinding.type));
      return;
    }
    if (key.equalsIgnoreCase("fourDoubleTapButtons")) {
      cfgOut->print("fourDoubleTapButtons=");
      cfgOut->println(fourDoubleTapBinding.mouseButtons);
      return;
    }
    if (key.equalsIgnoreCase("fourDoubleTapModifier")) {
      cfgOut->print("fourDoubleTapModifier=");
      cfgOut->println(fourDoubleTapBinding.keyModifier);
      return;
    }
    if (key.equalsIgnoreCase("fourDoubleTapKey")) {
      cfgOut->print("fourDoubleTapKey=");
      cfgOut->println(fourDoubleTapBinding.keyCode);
      return;
    }
    if (key.equalsIgnoreCase("fourSwipeThresholdX")) {
      cfgOut->print("fourSwipeThresholdX=");
      cfgOut->println(fourSwipeThresholdX);
      return;
    }
    if (key.equalsIgnoreCase("fourSwipeThresholdY")) {
      cfgOut->print("fourSwipeThresholdY=");
      cfgOut->println(fourSwipeThresholdY);
      return;
    }
    if (key.equalsIgnoreCase("fourSwipeTimeout")) {
      cfgOut->print("fourSwipeTimeout=");
      cfgOut->println(fourSwipeTimeout);
      return;
    }
    if (key.equalsIgnoreCase("fourSwipeCooldown")) {
      cfgOut->print("fourSwipeCooldown=");
      cfgOut->println(fourSwipeCooldown);
      return;
    }
    cfgOut->print("ERR: key ");
    cfgOut->println(key);
    return;
  }

  if (line.startsWith("SET ")) {
    int keyEnd = line.indexOf(' ', 4);
    if (keyEnd < 0) {
      cfgOut->println("ERR: SET format");
      return;
    }
    String key = line.substring(4, keyEnd);
    String valueStr = line.substring(keyEnd + 1);
    valueStr.trim();
    if (key.equalsIgnoreCase("scrollSensitivity")) {
      float v = valueStr.toFloat();
      if (v > 0.0f) {
        scrollSensitivity = v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("scrollLowSpeedBoostEnd")) {
      float v = valueStr.toFloat();
      if (v > 0.0f && v <= 2.0f) {
        scrollLowSpeedBoostEnd = v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("scrollMinActiveSpeed")) {
      float v = valueStr.toFloat();
      if (v >= 0.0f && v <= 2.0f) {
        scrollMinActiveSpeed = v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("scrollAccelStartSpeed")) {
      float v = valueStr.toFloat();
      if (v >= 0.0f && v <= 2.0f) {
        scrollAccelStartSpeed = v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("scrollAccelFactor")) {
      float v = valueStr.toFloat();
      if (v >= 0.0f && v <= 5.0f) {
        scrollAccelFactor = v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("scrollMaxAccel")) {
      float v = valueStr.toFloat();
      if (v >= 1.0f && v <= 10.0f) {
        scrollMaxAccel = v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("sensitivity")) {
      float v = valueStr.toFloat();
      if (v > 0.0f && v <= 5.0f) {
        sensitivity = v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("smoothFactor")) {
      float v = valueStr.toFloat();
      if (v >= 0.0f && v <= 1.0f) {
        smoothFactor = v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("accelFactor")) {
      float v = valueStr.toFloat();
      if (v >= 0.0f && v <= 1.0f) {
        accelFactor = v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("maxAccel")) {
      float v = valueStr.toFloat();
      if (v >= 0.0f && v <= 10.0f) {
        maxAccel = v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("maxDelta")) {
      int v = valueStr.toInt();
      if (v >= 1 && v <= 200) {
        maxDelta = (int16_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("moveDeadband")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 20) {
        moveDeadband = (int16_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("topZonePercent")) {
      int v = valueStr.toInt();
      if (v >= 5 && v <= 50) {
        TOP_ZONE_PERCENT = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("sideZonePercent")) {
      int v = valueStr.toInt();
      if (v >= 5 && v <= 50) {
        SIDE_ZONE_PERCENT = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("enableNavZones")) {
      if (valueStr.equalsIgnoreCase("1") || valueStr.equalsIgnoreCase("true")) {
        enableNavZones = true;
        cfgOut->println("OK");
      } else if (valueStr.equalsIgnoreCase("0") || valueStr.equalsIgnoreCase("false")) {
        enableNavZones = false;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rate")) {
      int v = valueStr.toInt();
      if (v >= 10 && v <= 200) {
        reportIntervalMs = (uint32_t)(1000 / v);
        if (reportIntervalMs == 0) reportIntervalMs = 1;
        normalizeIdleThresholds();
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("useBleWhenUsb")) {
      if (valueStr.equalsIgnoreCase("1") || valueStr.equalsIgnoreCase("true")) {
        useBleWhenUsb = true;
        cfgOut->println("OK");
      } else if (valueStr.equalsIgnoreCase("0") || valueStr.equalsIgnoreCase("false")) {
        useBleWhenUsb = false;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("slotManagerEnabled")) {
      if (valueStr.equalsIgnoreCase("1") || valueStr.equalsIgnoreCase("true")) {
        slotManagerEnabled = true;
        cfgOut->println("OK");
      } else if (valueStr.equalsIgnoreCase("0") || valueStr.equalsIgnoreCase("false")) {
        slotManagerEnabled = false;
        slotPairing = 0;
        slotReplaceArmed = 0;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("bleIdleSleepEnabled")) {
      if (valueStr.equalsIgnoreCase("1") || valueStr.equalsIgnoreCase("true")) {
        bleIdleSleepEnabled = true;
        cfgOut->println("OK");
      } else if (valueStr.equalsIgnoreCase("0") || valueStr.equalsIgnoreCase("false")) {
        bleIdleSleepEnabled = false;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("bleIdleSleepMs")) {
      long v = valueStr.toInt();
      if (v >= 1000 && v <= 3600000) {
        bleIdleSleepMs = (unsigned long)v;
        normalizeIdleThresholds();
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("bleIdleLightMs")) {
      long v = valueStr.toInt();
      if (v >= 1000 && v <= 3600000) {
        bleIdleLightMs = (unsigned long)v;
        normalizeIdleThresholds();
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("bleIdleMediumMs")) {
      long v = valueStr.toInt();
      if (v >= 1000 && v <= 3600000) {
        bleIdleMediumMs = (unsigned long)v;
        normalizeIdleThresholds();
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("lightIdleRate")) {
      int v = valueStr.toInt();
      if (v >= 5 && v <= 120) {
        lightIdleReportIntervalMs = (uint32_t)(1000 / v);
        if (lightIdleReportIntervalMs == 0) lightIdleReportIntervalMs = 1;
        normalizeIdleThresholds();
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftTopType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        leftTopZone.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftTopButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        leftTopZone.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftTopModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        leftTopZone.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftTopKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        leftTopZone.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightTopType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        rightTopZone.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightTopButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        rightTopZone.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightTopModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        rightTopZone.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightTopKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        rightTopZone.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightBottomType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        rightBottomZone.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightBottomButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        rightBottomZone.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightBottomModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        rightBottomZone.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("rightBottomKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        rightBottomZone.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftBottomType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        leftBottomZone.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftBottomButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        leftBottomZone.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftBottomModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        leftBottomZone.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("leftBottomKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        leftBottomZone.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeLeftType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        threeLeftBinding.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeLeftButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        threeLeftBinding.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeLeftModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeLeftBinding.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeLeftKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeLeftBinding.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeRightType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        threeRightBinding.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeRightButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        threeRightBinding.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeRightModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeRightBinding.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeRightKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeRightBinding.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeUpType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        threeUpBinding.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeUpButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        threeUpBinding.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeUpModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeUpBinding.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeUpKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeUpBinding.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeDownType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        threeDownBinding.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeDownButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        threeDownBinding.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeDownModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeDownBinding.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeDownKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeDownBinding.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeTapType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        threeTapBinding.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeTapButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        threeTapBinding.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeTapModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeTapBinding.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeTapKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeTapBinding.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeDoubleTapType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        threeDoubleTapBinding.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeDoubleTapButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        threeDoubleTapBinding.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeDoubleTapModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeDoubleTapBinding.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeDoubleTapKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        threeDoubleTapBinding.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeThresholdX")) {
      int v = valueStr.toInt();
      if (v >= 50 && v <= 800) {
        threeSwipeThresholdX = (uint16_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeThresholdY")) {
      int v = valueStr.toInt();
      if (v >= 50 && v <= 800) {
        threeSwipeThresholdY = (uint16_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeTimeout")) {
      int v = valueStr.toInt();
      if (v >= 50 && v <= 1000) {
        threeSwipeTimeout = (uint16_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("threeSwipeCooldown")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 2000) {
        threeSwipeCooldown = (uint16_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourLeftType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        fourLeftBinding.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourLeftButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        fourLeftBinding.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourLeftModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        fourLeftBinding.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourLeftKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        fourLeftBinding.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourRightType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        fourRightBinding.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourRightButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        fourRightBinding.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourRightModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        fourRightBinding.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourRightKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        fourRightBinding.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourUpType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        fourUpBinding.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourUpButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        fourUpBinding.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourUpModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        fourUpBinding.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourUpKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        fourUpBinding.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourDownType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        fourDownBinding.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourDownButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        fourDownBinding.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourDownModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        fourDownBinding.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourDownKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        fourDownBinding.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourTapType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        fourTapBinding.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourTapButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        fourTapBinding.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourTapModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        fourTapBinding.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourTapKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        fourTapBinding.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourDoubleTapType")) {
      ZoneType type;
      if (parseType(valueStr, &type)) {
        fourDoubleTapBinding.type = type;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourDoubleTapButtons")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 7) {
        fourDoubleTapBinding.mouseButtons = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourDoubleTapModifier")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        fourDoubleTapBinding.keyModifier = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourDoubleTapKey")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 255) {
        fourDoubleTapBinding.keyCode = (uint8_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourSwipeThresholdX")) {
      int v = valueStr.toInt();
      if (v >= 50 && v <= 800) {
        fourSwipeThresholdX = (uint16_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourSwipeThresholdY")) {
      int v = valueStr.toInt();
      if (v >= 50 && v <= 800) {
        fourSwipeThresholdY = (uint16_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourSwipeTimeout")) {
      int v = valueStr.toInt();
      if (v >= 50 && v <= 1000) {
        fourSwipeTimeout = (uint16_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    if (key.equalsIgnoreCase("fourSwipeCooldown")) {
      int v = valueStr.toInt();
      if (v >= 0 && v <= 2000) {
        fourSwipeCooldown = (uint16_t)v;
        cfgOut->println("OK");
      } else {
        cfgOut->println("ERR: value");
      }
      return;
    }
    cfgOut->print("ERR: key ");
    cfgOut->println(key);
    return;
  }

  if (line.equalsIgnoreCase("SAVE")) {
    if (saveConfig()) {
      cfgOut->println("OK");
    } else {
      cfgOut->println("ERR: save");
    }
    return;
  }

  if (line.equalsIgnoreCase("LOAD")) {
    if (loadConfig()) {
      cfgOut->println("OK");
    } else {
      cfgOut->println("ERR: load");
    }
    return;
  }

  if (line.equalsIgnoreCase("RESET")) {
    applyDefaults();
    cfgOut->println("OK");
    return;
  }

  if (line.equalsIgnoreCase("BOOT")) {
    cfgOut->println("OK");
    rebootDevice();
    return;
  }

  if (line.equalsIgnoreCase("INFO SLOT")) {
    slotPrintCompactStatus();
    return;
  }

  if (line.startsWith("SLOT ")) {
    if (!slotManagerEnabled) {
      cfgOut->println("ERR: slot manager disabled");
      return;
    }
    int v = line.substring(5).toInt();
    if (!slotSelect((uint8_t)v)) return;
    return;
  }

  if (line.startsWith("PAIR SLOT ")) {
    String args = line.substring(10);
    args.trim();
    bool forceReplace = false;
    int sp = args.indexOf(' ');
    String slotStr = args;
    if (sp >= 0) {
      slotStr = args.substring(0, sp);
      String opt = args.substring(sp + 1);
      opt.trim();
      forceReplace = opt.equalsIgnoreCase("FORCE");
      if (!forceReplace) {
        cfgOut->println("ERR: option");
        return;
      }
    }
    int v = slotStr.toInt();
    if (!slotStartPair((uint8_t)v, forceReplace)) return;
    return;
  }

  if (line.startsWith("UNPAIR SLOT ")) {
    int v = line.substring(12).toInt();
    if (!slotUnpair((uint8_t)v)) return;
    return;
  }

  if (line.equalsIgnoreCase("PAIRCLR")) {
    if (Bluefruit.connected()) {
      Bluefruit.disconnect(0);
      delay(50);
    }
    Bluefruit.Periph.clearBonds();
    slotResetAll();
    cfgOut->println("OK");
    return;
  }

  cfgOut->println("ERR: unknown");
}

bool loadConfig() {
  File f(InternalFS);
  if (!f.open(CONFIG_PATH, FILE_O_READ)) {
    cfgOut->println("[cfg] no config");
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
    } else if (key.equalsIgnoreCase("scrollLowSpeedBoostEnd")) {
      float val = value.toFloat();
      if (val > 0.0f && val <= 2.0f) scrollLowSpeedBoostEnd = val;
    } else if (key.equalsIgnoreCase("scrollMinActiveSpeed")) {
      float val = value.toFloat();
      if (val >= 0.0f && val <= 2.0f) scrollMinActiveSpeed = val;
    } else if (key.equalsIgnoreCase("scrollAccelStartSpeed")) {
      float val = value.toFloat();
      if (val >= 0.0f && val <= 2.0f) scrollAccelStartSpeed = val;
    } else if (key.equalsIgnoreCase("scrollAccelFactor")) {
      float val = value.toFloat();
      if (val >= 0.0f && val <= 5.0f) scrollAccelFactor = val;
    } else if (key.equalsIgnoreCase("scrollMaxAccel")) {
      float val = value.toFloat();
      if (val >= 1.0f && val <= 10.0f) scrollMaxAccel = val;
    } else if (key.equalsIgnoreCase("sensitivity")) {
      float val = value.toFloat();
      if (val > 0.0f) sensitivity = val;
    } else if (key.equalsIgnoreCase("smoothFactor")) {
      float val = value.toFloat();
      if (val >= 0.0f && val <= 1.0f) smoothFactor = val;
    } else if (key.equalsIgnoreCase("accelFactor")) {
      float val = value.toFloat();
      if (val >= 0.0f && val <= 1.0f) accelFactor = val;
    } else if (key.equalsIgnoreCase("maxAccel")) {
      float val = value.toFloat();
      if (val >= 0.0f && val <= 10.0f) maxAccel = val;
    } else if (key.equalsIgnoreCase("maxDelta")) {
      int v = value.toInt();
      if (v >= 1 && v <= 200) maxDelta = (int16_t)v;
    } else if (key.equalsIgnoreCase("moveDeadband")) {
      int v = value.toInt();
      if (v >= 0 && v <= 20) moveDeadband = (int16_t)v;
    } else if (key.equalsIgnoreCase("topZonePercent")) {
      int v = value.toInt();
      if (v >= 5 && v <= 50) TOP_ZONE_PERCENT = (uint8_t)v;
    } else if (key.equalsIgnoreCase("sideZonePercent")) {
      int v = value.toInt();
      if (v >= 5 && v <= 50) SIDE_ZONE_PERCENT = (uint8_t)v;
    } else if (key.equalsIgnoreCase("enableNavZones")) {
      enableNavZones = (value == "1" || value.equalsIgnoreCase("true"));
    } else if (key.equalsIgnoreCase("useBleWhenUsb")) {
      useBleWhenUsb = (value == "1" || value.equalsIgnoreCase("true"));
    } else if (key.equalsIgnoreCase("bleIdleSleepEnabled")) {
      bleIdleSleepEnabled = (value == "1" || value.equalsIgnoreCase("true"));
    } else if (key.equalsIgnoreCase("bleIdleLightMs")) {
      long v = value.toInt();
      if (v >= 1000 && v <= 3600000) bleIdleLightMs = (unsigned long)v;
    } else if (key.equalsIgnoreCase("bleIdleMediumMs")) {
      long v = value.toInt();
      if (v >= 1000 && v <= 3600000) bleIdleMediumMs = (unsigned long)v;
    } else if (key.equalsIgnoreCase("bleIdleSleepMs")) {
      long v = value.toInt();
      if (v >= 1000 && v <= 3600000) bleIdleSleepMs = (unsigned long)v;
    } else if (key.equalsIgnoreCase("lightIdleRate")) {
      int v = value.toInt();
      if (v >= 5 && v <= 120) {
        lightIdleReportIntervalMs = (uint32_t)(1000 / v);
        if (lightIdleReportIntervalMs == 0) lightIdleReportIntervalMs = 1;
      }
    } else if (slotLoadConfigEntry(key, value)) {
      // handled by slot manager
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
    } else if (key.equalsIgnoreCase("threeTapType")) {
      ZoneType type;
      if (parseType(value, &type)) threeTapBinding.type = type;
    } else if (key.equalsIgnoreCase("threeTapButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) threeTapBinding.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeTapModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) threeTapBinding.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeTapKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) threeTapBinding.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeDoubleTapType")) {
      ZoneType type;
      if (parseType(value, &type)) threeDoubleTapBinding.type = type;
    } else if (key.equalsIgnoreCase("threeDoubleTapButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) threeDoubleTapBinding.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeDoubleTapModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) threeDoubleTapBinding.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("threeDoubleTapKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) threeDoubleTapBinding.keyCode = (uint8_t)v;
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
    } else if (key.equalsIgnoreCase("fourLeftType")) {
      ZoneType type;
      if (parseType(value, &type)) fourLeftBinding.type = type;
    } else if (key.equalsIgnoreCase("fourLeftButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) fourLeftBinding.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourLeftModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) fourLeftBinding.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourLeftKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) fourLeftBinding.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourRightType")) {
      ZoneType type;
      if (parseType(value, &type)) fourRightBinding.type = type;
    } else if (key.equalsIgnoreCase("fourRightButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) fourRightBinding.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourRightModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) fourRightBinding.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourRightKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) fourRightBinding.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourUpType")) {
      ZoneType type;
      if (parseType(value, &type)) fourUpBinding.type = type;
    } else if (key.equalsIgnoreCase("fourUpButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) fourUpBinding.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourUpModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) fourUpBinding.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourUpKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) fourUpBinding.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourDownType")) {
      ZoneType type;
      if (parseType(value, &type)) fourDownBinding.type = type;
    } else if (key.equalsIgnoreCase("fourDownButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) fourDownBinding.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourDownModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) fourDownBinding.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourDownKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) fourDownBinding.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourTapType")) {
      ZoneType type;
      if (parseType(value, &type)) fourTapBinding.type = type;
    } else if (key.equalsIgnoreCase("fourTapButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) fourTapBinding.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourTapModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) fourTapBinding.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourTapKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) fourTapBinding.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourDoubleTapType")) {
      ZoneType type;
      if (parseType(value, &type)) fourDoubleTapBinding.type = type;
    } else if (key.equalsIgnoreCase("fourDoubleTapButtons")) {
      int v = value.toInt();
      if (v >= 0 && v <= 7) fourDoubleTapBinding.mouseButtons = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourDoubleTapModifier")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) fourDoubleTapBinding.keyModifier = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourDoubleTapKey")) {
      int v = value.toInt();
      if (v >= 0 && v <= 255) fourDoubleTapBinding.keyCode = (uint8_t)v;
    } else if (key.equalsIgnoreCase("fourSwipeThresholdX")) {
      int v = value.toInt();
      if (v >= 50 && v <= 800) fourSwipeThresholdX = (uint16_t)v;
    } else if (key.equalsIgnoreCase("fourSwipeThresholdY")) {
      int v = value.toInt();
      if (v >= 50 && v <= 800) fourSwipeThresholdY = (uint16_t)v;
    } else if (key.equalsIgnoreCase("fourSwipeTimeout")) {
      int v = value.toInt();
      if (v >= 50 && v <= 1000) fourSwipeTimeout = (uint16_t)v;
    } else if (key.equalsIgnoreCase("fourSwipeCooldown")) {
      int v = value.toInt();
      if (v >= 0 && v <= 2000) fourSwipeCooldown = (uint16_t)v;
    } else if (key.equalsIgnoreCase("rate")) {
      int v = value.toInt();
      if (v >= 10 && v <= 200) {
        reportIntervalMs = (uint32_t)(1000 / v);
        if (reportIntervalMs == 0) reportIntervalMs = 1;
      }
    }
  }
  normalizeIdleThresholds();
  f.close();
  return true;
}

bool saveConfig() {
  File f(InternalFS);
  if (InternalFS.exists(CONFIG_PATH)) {
    InternalFS.remove(CONFIG_PATH);
  }
  if (!f.open(CONFIG_PATH, FILE_O_WRITE)) {
    cfgOut->println("[cfg] open failed");
    return false;
  }
  f.print("scrollSensitivity=");
  f.println(scrollSensitivity, 8);
  f.print("scrollLowSpeedBoostEnd=");
  f.println(scrollLowSpeedBoostEnd, 6);
  f.print("scrollMinActiveSpeed=");
  f.println(scrollMinActiveSpeed, 6);
  f.print("scrollAccelStartSpeed=");
  f.println(scrollAccelStartSpeed, 6);
  f.print("scrollAccelFactor=");
  f.println(scrollAccelFactor, 6);
  f.print("scrollMaxAccel=");
  f.println(scrollMaxAccel, 6);
  f.print("sensitivity=");
  f.println(sensitivity, 6);
  f.print("smoothFactor=");
  f.println(smoothFactor, 6);
  f.print("accelFactor=");
  f.println(accelFactor, 6);
  f.print("maxAccel=");
  f.println(maxAccel, 6);
  f.print("maxDelta=");
  f.println(maxDelta);
  f.print("moveDeadband=");
  f.println(moveDeadband);
  f.print("topZonePercent=");
  f.println(TOP_ZONE_PERCENT);
  f.print("sideZonePercent=");
  f.println(SIDE_ZONE_PERCENT);
  f.print("enableNavZones=");
  f.println(enableNavZones ? "1" : "0");
  f.print("useBleWhenUsb=");
  f.println(useBleWhenUsb ? "1" : "0");
  f.print("bleIdleSleepEnabled=");
  f.println(bleIdleSleepEnabled ? "1" : "0");
  f.print("bleIdleLightMs=");
  f.println(bleIdleLightMs);
  f.print("bleIdleMediumMs=");
  f.println(bleIdleMediumMs);
  f.print("bleIdleSleepMs=");
  f.println(bleIdleSleepMs);
  f.print("lightIdleRate=");
  f.println(lightIdleReportIntervalMs ? (1000 / lightIdleReportIntervalMs) : 0);
  slotSaveConfig(f);
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
  f.print("threeTapType=");
  f.println(typeToString(threeTapBinding.type));
  f.print("threeTapButtons=");
  f.println(threeTapBinding.mouseButtons);
  f.print("threeTapModifier=");
  f.println(threeTapBinding.keyModifier);
  f.print("threeTapKey=");
  f.println(threeTapBinding.keyCode);
  f.print("threeDoubleTapType=");
  f.println(typeToString(threeDoubleTapBinding.type));
  f.print("threeDoubleTapButtons=");
  f.println(threeDoubleTapBinding.mouseButtons);
  f.print("threeDoubleTapModifier=");
  f.println(threeDoubleTapBinding.keyModifier);
  f.print("threeDoubleTapKey=");
  f.println(threeDoubleTapBinding.keyCode);
  f.print("threeSwipeThresholdX=");
  f.println(threeSwipeThresholdX);
  f.print("threeSwipeThresholdY=");
  f.println(threeSwipeThresholdY);
  f.print("threeSwipeTimeout=");
  f.println(threeSwipeTimeout);
  f.print("threeSwipeCooldown=");
  f.println(threeSwipeCooldown);
  f.print("fourLeftType=");
  f.println(typeToString(fourLeftBinding.type));
  f.print("fourLeftButtons=");
  f.println(fourLeftBinding.mouseButtons);
  f.print("fourLeftModifier=");
  f.println(fourLeftBinding.keyModifier);
  f.print("fourLeftKey=");
  f.println(fourLeftBinding.keyCode);
  f.print("fourRightType=");
  f.println(typeToString(fourRightBinding.type));
  f.print("fourRightButtons=");
  f.println(fourRightBinding.mouseButtons);
  f.print("fourRightModifier=");
  f.println(fourRightBinding.keyModifier);
  f.print("fourRightKey=");
  f.println(fourRightBinding.keyCode);
  f.print("fourUpType=");
  f.println(typeToString(fourUpBinding.type));
  f.print("fourUpButtons=");
  f.println(fourUpBinding.mouseButtons);
  f.print("fourUpModifier=");
  f.println(fourUpBinding.keyModifier);
  f.print("fourUpKey=");
  f.println(fourUpBinding.keyCode);
  f.print("fourDownType=");
  f.println(typeToString(fourDownBinding.type));
  f.print("fourDownButtons=");
  f.println(fourDownBinding.mouseButtons);
  f.print("fourDownModifier=");
  f.println(fourDownBinding.keyModifier);
  f.print("fourDownKey=");
  f.println(fourDownBinding.keyCode);
  f.print("fourTapType=");
  f.println(typeToString(fourTapBinding.type));
  f.print("fourTapButtons=");
  f.println(fourTapBinding.mouseButtons);
  f.print("fourTapModifier=");
  f.println(fourTapBinding.keyModifier);
  f.print("fourTapKey=");
  f.println(fourTapBinding.keyCode);
  f.print("fourDoubleTapType=");
  f.println(typeToString(fourDoubleTapBinding.type));
  f.print("fourDoubleTapButtons=");
  f.println(fourDoubleTapBinding.mouseButtons);
  f.print("fourDoubleTapModifier=");
  f.println(fourDoubleTapBinding.keyModifier);
  f.print("fourDoubleTapKey=");
  f.println(fourDoubleTapBinding.keyCode);
  f.print("fourSwipeThresholdX=");
  f.println(fourSwipeThresholdX);
  f.print("fourSwipeThresholdY=");
  f.println(fourSwipeThresholdY);
  f.print("fourSwipeTimeout=");
  f.println(fourSwipeTimeout);
  f.print("fourSwipeCooldown=");
  f.println(fourSwipeCooldown);
  f.print("rate=");
  f.println(reportIntervalMs ? (1000 / reportIntervalMs) : 0);
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

void performTapBinding(bool* pendingTap,
                       unsigned long* lastTapTimeMs,
                       int16_t* lastTapPosX,
                       int16_t* lastTapPosY,
                       int16_t tapX,
                       int16_t tapY,
                       unsigned long now,
                       const ZoneBinding& singleTapBinding,
                       const ZoneBinding& doubleTapBinding) {
  if (*pendingTap && now - *lastTapTimeMs <= DOUBLE_TAP_WINDOW) {
    uint16_t dist = abs(tapX - *lastTapPosX) + abs(tapY - *lastTapPosY);
    if (dist <= DOUBLE_TAP_MAX_MOVE) {
      performZoneAction(doubleTapBinding);
      *pendingTap = false;
      return;
    }
    performZoneAction(singleTapBinding);
  }
  *pendingTap = true;
  *lastTapTimeMs = now;
  *lastTapPosX = tapX;
  *lastTapPosY = tapY;
}

bool finalizeSingleTouch(unsigned long now, bool releasedByTimeout) {
  if (dragActive) {
    sendMouseButtons((uint8_t)0);
    dragActive = false;
    tapCandidate = false;
    pendingClick = false;
    mode = MODE_NONE;
    velX = velY = 0;
    smoothDx = smoothDy = 0;
    accumX = accumY = 0;
    return true;
  }
  if (now - lastScrollMotionTime < TAP_GUARD_AFTER_SCROLL_MS) {
    tapCandidate = false;
    pendingClick = false;
    mode = MODE_NONE;
    velX = velY = 0;
    smoothDx = smoothDy = 0;
    accumX = accumY = 0;
    return true;
  }
  if (tapCandidate) {
    unsigned long dt = now - tapStartTime;
    if (dt <= TAP_MAX_MS) {
      if (enableNavZones && inLeftTopZone(tapStartX, tapStartY)) {
        performZoneAction(leftTopZone);
      } else if (enableNavZones && inRightTopZone(tapStartX, tapStartY)) {
        performZoneAction(rightTopZone);
      } else if (enableNavZones && inRightBottomZone(tapStartX, tapStartY)) {
        performZoneAction(rightBottomZone);
      } else if (enableNavZones && inLeftBottomZone(tapStartX, tapStartY)) {
        performZoneAction(leftBottomZone);
      } else if (!releasedByTimeout && pendingClick && now - lastTapTime <= DOUBLE_TAP_WINDOW) {
        uint16_t dist = abs(tapStartX - lastTapX) + abs(tapStartY - lastTapY);
        if (dist <= DOUBLE_TAP_MAX_MOVE) {
          sendMouseClick(MOUSE_BUTTON_LEFT);
          delay(30);
          sendMouseClick(MOUSE_BUTTON_LEFT);
          pendingClick = false;
        } else {
          sendMouseClick(MOUSE_BUTTON_LEFT);
          pendingClick = true;
          lastTapTime = now;
          lastTapX = tapStartX;
          lastTapY = tapStartY;
        }
      } else {
        if (releasedByTimeout && pendingClick && now - lastTapTime <= DOUBLE_TAP_WINDOW) {
          sendMouseClick(MOUSE_BUTTON_LEFT);
          pendingClick = false;
        } else {
          pendingClick = true;
          lastTapTime = now;
          lastTapX = tapStartX;
          lastTapY = tapStartY;
        }
      }
    }
  }
  tapCandidate = false;
  mode = MODE_NONE;
  velX = velY = 0;
  smoothDx = smoothDy = 0;
  accumX = accumY = 0;
  return true;
}

#include "modules/touchpad_hid_output.h"

/* ===========================
   Report 解析
   =========================== */
void handleReport(uint8_t* buf, uint16_t len) {
  if (len < 13) return;
  uint8_t s0 = buf[3];
  uint8_t s1 = buf[8];
  uint8_t s2 = (len >= 18) ? buf[13] : 0;
  uint8_t s3 = (len >= 23) ? buf[18] : 0;

  bool f1 = s0 & 0x02;
  bool f2 = s1 & 0x02;
  bool f3 = (len >= 18) ? (s2 & 0x02) : false;
  bool f4 = (len >= 23) ? (s3 & 0x02) : false;
  bool anyFinger = f1 || f2 || f3 || f4;

  if (waitFingerReleaseAfterReconnect) {
    if (anyFinger) {
      lastTouchTime = millis();
      return;
    }
    waitFingerReleaseAfterReconnect = false;
    return;
  }

  int16_t x1 = buf[4] | (buf[5] << 8);
  int16_t y1 = buf[6] | (buf[7] << 8);
  int16_t x2 = buf[9] | (buf[10] << 8);
  int16_t y2 = buf[11] | (buf[12] << 8);
  int16_t x3 = (len >= 18) ? (buf[14] | (buf[15] << 8)) : 0;
  int16_t y3 = (len >= 18) ? (buf[16] | (buf[17] << 8)) : 0;
  int16_t x4 = (len >= 23) ? (buf[19] | (buf[20] << 8)) : 0;
  int16_t y4 = (len >= 23) ? (buf[21] | (buf[22] << 8)) : 0;

  unsigned long now = millis();

  /*===== 四指手势 =====*/
  if (f1 && f2 && f3 && f4) {
    if (dragActive) {
      sendMouseButtons((uint8_t)0);
      dragActive = false;
    }
    int16_t avgX = (x1 + x2 + x3 + x4) / 4;
    int16_t avgY = (y1 + y2 + y3 + y4) / 4;
    if (mode != MODE_QUAD) {
      quadStartX = avgX;
      quadStartY = avgY;
      quadLastX = avgX;
      quadLastY = avgY;
      quadStartTime = now;
    } else {
      quadLastX = avgX;
      quadLastY = avgY;
    }
    mode = MODE_QUAD;
    tapCandidate = false;
    lastTouchTime = now;
    return;
  }

  if (mode == MODE_QUAD && !(f1 && f2 && f3 && f4)) {
    int16_t dx = quadLastX - quadStartX;
    int16_t dy = quadLastY - quadStartY;
    uint16_t dist = abs(dx) + abs(dy);
    unsigned long dt = now - quadStartTime;
    bool triggered = false;
    if (dt <= fourSwipeTimeout && now - lastFourSwipeTime >= fourSwipeCooldown) {
      if (abs(dx) >= (int16_t)fourSwipeThresholdX && abs(dx) >= abs(dy)) {
        if (dx > 0) {
          performZoneAction(fourRightBinding);
        } else {
          performZoneAction(fourLeftBinding);
        }
        lastFourSwipeTime = now;
        triggered = true;
      } else if (abs(dy) >= (int16_t)fourSwipeThresholdY && abs(dy) > abs(dx)) {
        if (dy > 0) {
          performZoneAction(fourDownBinding);
        } else {
          performZoneAction(fourUpBinding);
        }
        lastFourSwipeTime = now;
        triggered = true;
      }
    }
    if (!triggered && dt <= TAP_MAX_MS && dist <= DOUBLE_TAP_MAX_MOVE) {
      performTapBinding(&pendingFourTap, &lastFourTapTime, &lastFourTapX, &lastFourTapY,
                        quadStartX, quadStartY, now, fourTapBinding, fourDoubleTapBinding);
      triggered = true;
    }
    if (triggered) {
      pendingClick = false;
      tapCandidate = false;
    }
    mode = MODE_NONE;
    return;
  }

  /*===== 三指手势 =====*/
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
    uint16_t dist = abs(dx) + abs(dy);
    unsigned long dt = now - tripleStartTime;
    bool triggered = false;
    if (dt <= threeSwipeTimeout && now - lastSwipeTime >= threeSwipeCooldown) {
      if (abs(dx) >= (int16_t)threeSwipeThresholdX && abs(dx) >= abs(dy)) {
        if (dx > 0) {
          performZoneAction(threeRightBinding);
        } else {
          performZoneAction(threeLeftBinding);
        }
        lastSwipeTime = now;
        triggered = true;
      } else if (abs(dy) >= (int16_t)threeSwipeThresholdY && abs(dy) > abs(dx)) {
        if (dy > 0) {
          performZoneAction(threeDownBinding);
        } else {
          performZoneAction(threeUpBinding);
        }
        lastSwipeTime = now;
        triggered = true;
      }
    }
    if (!triggered && dt <= TAP_MAX_MS && dist <= DOUBLE_TAP_MAX_MOVE) {
      performTapBinding(&pendingThreeTap, &lastThreeTapTime, &lastThreeTapX, &lastThreeTapY,
                        tripleStartX, tripleStartY, now, threeTapBinding, threeDoubleTapBinding);
      triggered = true;
    }
    if (triggered) {
      pendingClick = false;
      tapCandidate = false;
    }
    mode = MODE_NONE;
    return;
  }

  /*===== 双指滚动 =====*/
  if (f1 && f2) {
    if (dragActive) {
      sendMouseButtons((uint8_t)0);
      dragActive = false;
    }
    if (mode == MODE_DOUBLE) {
      int16_t dy = ((y1 - lastY1) + (y2 - lastY2)) / 2;
      dy = constrain(dy, -maxDelta, maxDelta);
      if (abs(dy) <= SCROLL_DEADBAND) dy = 0;
      float v = applyScrollAxisResponse(dy);
      float accel = scrollAccelForSpeed(abs(v));
      v *= accel;
      smoothScroll += (v - smoothScroll) * scrollSmoothFactor;
      scrollVel = smoothScroll;
      if (abs(scrollVel) > SCROLL_VELOCITY_EPSILON) {
        lastScrollMotionTime = now;
      }
    } else {
      scrollVel = 0;
      smoothScroll = 0;
      if (abs(accumScroll) < 1.0f) accumScroll = 0;
      lastScrollReportMs = now;
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
    if (now - lastScrollMotionTime < TAP_GUARD_AFTER_SCROLL_MS) {
      lastX1 = x1;
      lastY1 = y1;
      mode = MODE_SINGLE;
      lastTouchTime = now;
      return;
    }
    if (mode == MODE_SINGLE) {
      int16_t dx = x1 - lastX1;
      int16_t dy = y1 - lastY1;
      dx = constrain(dx, -maxDelta, maxDelta);
      dy = constrain(dy, -maxDelta, maxDelta);

      if (tapCandidate) {
        uint16_t dist = abs(x1 - tapStartX) + abs(y1 - tapStartY);
        if (dist <= DOUBLE_TAP_MAX_MOVE && now - tapStartTime >= LONG_PRESS_DRAG_MS) {
          dragActive = true;
          tapCandidate = false;
          pendingClick = false;
          velX = velY = 0;
          smoothDx = smoothDy = 0;
          accumX = accumY = 0;
          lastReportMs = now;
          lastX1 = x1;
          lastY1 = y1;
          sendMouseButtons(MOUSE_BUTTON_LEFT);
          lastTouchTime = now;
          return;
        }
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
        smoothDx *= SINGLE_HOLD_DECAY;
        smoothDy *= SINGLE_HOLD_DECAY;
        if (abs(smoothDx) < SINGLE_VELOCITY_EPSILON) smoothDx = 0;
        if (abs(smoothDy) < SINGLE_VELOCITY_EPSILON) smoothDy = 0;
        velX = smoothDx;
        velY = smoothDy;
        if (abs(accumX) < SINGLE_VELOCITY_EPSILON) accumX = 0;
        if (abs(accumY) < SINGLE_VELOCITY_EPSILON) accumY = 0;
        lastX1 = x1;
        lastY1 = y1;
        lastTouchTime = now;
        return;
      }

      float fx = applySingleFingerAxisResponse(dx, AXIS_SCALE_X);
      float fy = applySingleFingerAxisResponse(dy, AXIS_SCALE_Y);

      float speed = sqrt(fx * fx + fy * fy);
      float accel = singleFingerAccelForSpeed(speed);
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
      lastReportMs = now;
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
    finalizeSingleTouch(now, false);
    return;
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
  markActivity();

  waitIntRelease(INT_RELEASE_TIMEOUT_US);
}
