#include <Wire.h>
#include <Mouse.h>

/* ===== I2C-HID 固定参数 ===== */
#define I2C_ADDR 0x2C  // 你的触摸板地址
#define INPUT_REG_L 0x09
#define INPUT_REG_H 0x01

/* ===== INT 引脚 ===== */
#define INT_PIN 10  // GP10，根据你实际修改

/* ===== 缓冲区 ===== */
uint8_t reportBuf[180];

bool lastTouching = false;
int16_t lastX = 0;
int16_t lastY = 0;
unsigned long lastReportTime = 0;
const unsigned long TOUCH_RELEASE_TIMEOUT = 15;  // ms

/* ======= 优化移动卡顿 ========= */
// 灵敏度（0.1 ~ 1.0 之间调）
float sensitivity = 0.3f;

// 平滑系数（越小越平滑，0.2 ~ 0.4 推荐）
float smoothFactor = 0.2f;

// 最大单次移动，防止跳变
const int16_t MAX_DELTA = 30;

// 加速参数
float accelFactor = 0.015f;  // 速度相关加速
float maxAccel = 2.5f;

// 平滑后的速度
float smoothDx = 0;
float smoothDy = 0;

// 亚像素累积
float accumX = 0;
float accumY = 0;

float currentVelX = 0;
float currentVelY = 0;
unsigned long lastMoveTime = 0;

/* ===== 双指滚动参数 ===== */
const unsigned long TWO_FINGER_RELEASE_TIMEOUT = 15;  // ms

float scrollSensitivity = 0.25f;
float scrollSmoothFactor = 0.25f;
float scrollAccelFactor = 0.02f;
float maxScrollAccel = 2.0f;

float smoothScroll = 0;
float accumScroll = 0;
float currentScrollVel = 0;

int16_t lastY1 = 0;
int16_t lastY2 = 0;

bool lastTwoFinger = false;


void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Wire.begin();
  Wire.setClock(400000);

  pinMode(INT_PIN, INPUT_PULLUP);

  Mouse.begin();

  Serial.println("Ready");
}

void handleTouchToMouse(uint8_t* buf, uint16_t len) {
  if (len < 12) return;
  if (buf[2] != 0x04) return;

  bool finger1 = buf[3] & 0x01;
  bool finger2 = buf[3] & 0x02;

  Serial.print("buf[3] is ");
  Serial.println(buf[3]);

  unsigned long now = millis();

  /* =========================
     ===== 双指滚动模式 =====
     ========================= */
  if (finger1 && finger2) {
    lastReportTime = now;
    //Serial.println("TwoFinger Mode");
    int16_t y1 = buf[6] | (buf[7] << 8);
    int16_t y2 = buf[10] | (buf[11] << 8);

    if (lastTwoFinger) {
      int16_t rawDy1 = y1 - lastY1;
      int16_t rawDy2 = y2 - lastY2;
      int16_t rawDy = (rawDy1 + rawDy2) / 2;

      rawDy = constrain(rawDy, -MAX_DELTA, MAX_DELTA);

      float dy = rawDy * scrollSensitivity;

      float speed = abs(dy);
      float accel = 1.0f + min(speed * scrollAccelFactor, maxScrollAccel);
      dy *= accel;

      smoothScroll += (dy - smoothScroll) * scrollSmoothFactor;
      currentScrollVel = smoothScroll;
    } else {
      smoothScroll = 0;
      accumScroll = 0;
    }

    lastY1 = y1;
    lastY2 = y2;

    lastTwoFinger = true;
    lastTouching = false;  // 🔴 关键：禁止单指逻辑
    return;
  }

  /* =========================
     ===== 单指移动模式 =====
     ========================= */
  lastTwoFinger = false;

  if (finger1) {
    lastReportTime = now;
    //Serial.println("OneFinger Mode");
    int16_t x = buf[4] | (buf[5] << 8);
    int16_t y = buf[6] | (buf[7] << 8);

    if (lastTouching) {
      int16_t rawDx = x - lastX;
      int16_t rawDy = y - lastY;

      rawDx = constrain(rawDx, -MAX_DELTA, MAX_DELTA);
      rawDy = constrain(rawDy, -MAX_DELTA, MAX_DELTA);

      float dx = rawDx * sensitivity;
      float dy = rawDy * sensitivity;

      float speed = sqrt(dx * dx + dy * dy);
      float accel = 1.0f + min(speed * accelFactor, maxAccel);
      dx *= accel;
      dy *= accel;

      smoothDx += (dx - smoothDx) * smoothFactor;
      smoothDy += (dy - smoothDy) * smoothFactor;

      currentVelX = smoothDx;
      currentVelY = smoothDy;
    } else {
      smoothDx = smoothDy = 0;
      accumX = accumY = 0;
    }

    lastX = x;
    lastY = y;
    lastTouching = true;
  }
}




void loop() {
  if (digitalRead(INT_PIN) == LOW) {
    readInputReport();
  }

  /* ===== 单指释放 ===== */
  if (lastTouching && (millis() - lastReportTime > TOUCH_RELEASE_TIMEOUT)) {
    lastTouching = false;
    currentVelX = 0;
    currentVelY = 0;
  }

  /* ===== 双指释放 ===== */
  if (lastTwoFinger && (millis() - lastReportTime > TWO_FINGER_RELEASE_TIMEOUT)) {
    lastTwoFinger = false;
    currentScrollVel = 0;
    smoothScroll = 0;
    accumScroll = 0;
  }

  /* ===== 单指输出 ===== */
  if (lastTouching) {
    accumX += currentVelX;
    accumY += currentVelY;
    int8_t moveX = (int8_t)accumX;
    int8_t moveY = (int8_t)accumY;
    if (moveX || moveY) {
      Mouse.move(moveX, moveY);
      accumX -= moveX;
      accumY -= moveY;
    }
  }

  /* ===== 双指滚动输出 ===== */
  if (lastTwoFinger) {
    accumScroll += currentScrollVel;
    int8_t scroll = (int8_t)accumScroll;
    if (scroll) {
      Mouse.move(0, 0, -scroll);
      accumScroll -= scroll;
    }
  }
}


/* =============================
 *  严格的一次性 Input Report 读取
 * ============================= */
void readInputReport() {
  // 1. 指向 Input Register
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(INPUT_REG_L);
  Wire.write(INPUT_REG_H);
  if (Wire.endTransmission(false) != 0) {
    Serial.println("I2C set reg failed");
    return;
  }

  // 2. 先读前 2 字节（Report Length）
  if (Wire.requestFrom(I2C_ADDR, (uint8_t)2) != 2) {
    Serial.println("Length read failed");
    return;
  }

  uint16_t len = Wire.read();
  len |= (Wire.read() << 8);

  if (len == 0 || len > sizeof(reportBuf)) {
    Serial.print("Invalid report length: ");
    Serial.println(len);
    return;
  }

  // 3. 再读完整 Report
  if (Wire.requestFrom(I2C_ADDR, (uint8_t)len) != len) {
    Serial.println("Report read failed");
    return;
  }

  for (uint16_t i = 0; i < len; i++) {
    reportBuf[i] = Wire.read();
  }

  // 4. 打印（盲读）
  Serial.print("REPORT (");
  Serial.print(len);
  Serial.print("): ");

  for (uint16_t i = 0; i < len; i++) {
    if (reportBuf[i] < 0x10) Serial.print("0");
    Serial.print(reportBuf[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  handleTouchToMouse(reportBuf, len);

  // 5. 等待 INT 释放（非常重要，防止重复读）
  while (digitalRead(INT_PIN) == LOW) {
    delayMicroseconds(50);
  }
}
