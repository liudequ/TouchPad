#include <Wire.h>
#include <Mouse.h>

/*===== I2C-HID =====*/
#define I2C_ADDR 0x2C
#define INPUT_REG_L 0x09
#define INPUT_REG_H 0x01

#define SDA_PIN 4
#define SCL_PIN 5
#define INT_PIN 10
#define TP_EN 9  // ★ 新增：TouchPad ENABLE

uint8_t reportBuf[64];

/*===== 单指移动参数 =====*/
float sensitivity = 0.3f;
float smoothFactor = 0.2f;
float accelFactor = 0.015f;
float maxAccel = 2.5f;
const int16_t MAX_DELTA = 30;

float smoothDx = 0, smoothDy = 0;
float accumX = 0, accumY = 0;
float velX = 0, velY = 0;

/*===== 双指滚动参数 =====*/
float scrollSensitivity = 0.25f;
float smoothScroll = 0;
float accumScroll = 0;
float scrollVel = 0;

/*===== 状态 =====*/
bool lastSingle = false;
bool lastDouble = false;

int16_t lastX1 = 0, lastY1 = 0;
int16_t lastX2 = 0, lastY2 = 0;

unsigned long lastTouchTime = 0;
unsigned long lastTapTime = 0;
const unsigned long TAP_INTERVAL = 250;
const unsigned long RELEASE_TIMEOUT = 20;

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

  Mouse.begin();
}

/*===========================
   主循环
   ===========================*/
void loop() {
  if (digitalRead(INT_PIN) == LOW) {
    readInputReport();
  }

  unsigned long now = millis();

  /*单指超时释放*/
  if (lastSingle && now - lastTouchTime > RELEASE_TIMEOUT) {
    lastSingle = false;
    velX = velY = 0;
  }

  /*双指超时释放*/
  if (lastDouble && now - lastTouchTime > RELEASE_TIMEOUT) {
    lastDouble = false;
    scrollVel = 0;
    smoothScroll = 0;
    accumScroll = 0;
  }

  /*连续输出*/
  if (lastSingle) {
    accumX += velX;
    accumY += velY;
    int8_t mx = (int8_t)accumX;
    int8_t my = (int8_t)accumY;
    if (mx || my) {
      Mouse.move(mx, my);
      accumX -= mx;
      accumY -= my;
    }
  }

  if (lastDouble) {
    accumScroll += scrollVel;
    int8_t s = (int8_t)accumScroll;
    if (s) {
      Mouse.move(0, 0, -s);
      accumScroll -= s;
    }
  }
}

/* ===========================
   Report 解析
   =========================== */
void handleReport(uint8_t* buf) {
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
    if (lastDouble) {
      int16_t dy = ((y1 - lastY1) + (y2 - lastY2)) / 2;
      dy = constrain(dy, -MAX_DELTA, MAX_DELTA);
      float v = dy * scrollSensitivity;
      smoothScroll += (v - smoothScroll) * 0.25f;
      scrollVel = smoothScroll;
    } else {
      smoothScroll = accumScroll = 0;
    }

    lastX1 = x1;
    lastY1 = y1;
    lastX2 = x2;
    lastY2 = y2;

    lastDouble = true;
    lastSingle = false;
    lastTouchTime = now;
    return;
  }

  /*===== 单指移动 =====*/
  if (f1 && !f2) {
    if (lastSingle) {
      int16_t dx = x1 - lastX1;
      int16_t dy = y1 - lastY1;
      dx = constrain(dx, -MAX_DELTA, MAX_DELTA);
      dy = constrain(dy, -MAX_DELTA, MAX_DELTA);

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
    }

    lastX1 = x1;
    lastY1 = y1;
    lastSingle = true;
    lastDouble = false;
    lastTouchTime = now;
    return;
  }

  /*===== 抬起：处理双击 =====*/
  if (!f1 && lastSingle) {
    if (now - lastTapTime < TAP_INTERVAL) {
      Mouse.click(MOUSE_LEFT);
      lastTapTime = 0;
    } else {
      lastTapTime = now;
    }
    lastSingle = false;
  }
}

/*===========================
   I2C 读取
   ===========================*/
void readInputReport() {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(INPUT_REG_L);
  Wire.write(INPUT_REG_H);
  if (Wire.endTransmission(false) != 0) return;

  if (Wire.requestFrom(I2C_ADDR, (uint8_t)2) != 2) return;
  uint16_t len = Wire.read() | (Wire.read() << 8);
  if (len == 0 || len > sizeof(reportBuf)) return;

  if (Wire.requestFrom(I2C_ADDR, (uint8_t)len) != len) return;
  for (uint16_t i = 0; i < len; i++) reportBuf[i] = Wire.read();

  handleReport(reportBuf);

  while (digitalRead(INT_PIN) == LOW) delayMicroseconds(50);
}