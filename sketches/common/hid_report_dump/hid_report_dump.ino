#include <Wire.h>

/* ===== I2C-HID 固定参数 ===== */
#define I2C_ADDR        0x2C
#define INPUT_REG_L     0x09
#define INPUT_REG_H     0x01

/* ===== 引脚配置 ===== */
#if defined(ARDUINO_ARCH_NRF52)
#define SDA_PIN         8
#define SCL_PIN         9
#define INT_PIN         7
#define TP_EN           6
#else
#define INT_PIN         10   // 按你的实际连线
#endif

/* ===== 缓冲区 ===== */
uint8_t reportBuf[256];

void touchColdBoot() {
#if defined(ARDUINO_ARCH_NRF52)
  pinMode(TP_EN, OUTPUT);
  digitalWrite(TP_EN, LOW);
  delay(20);
  digitalWrite(TP_EN, HIGH);
  delay(150);
#endif
}

void setup() {
  Serial.begin(115200);
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart < 2000)) {
    delay(10);
  }

  touchColdBoot();

#if defined(ARDUINO_ARCH_NRF52)
  Wire.setPins(SDA_PIN, SCL_PIN);
#endif
  Wire.begin();
  Wire.setClock(400000);

  pinMode(INT_PIN, INPUT_PULLUP);

  Serial.println("=== I2C-HID Report Dump Tool nice!nano v2 ===");
  Serial.println("Waiting for touch...");
}

/* =============================
 * 主循环：只在 INT 拉低时读
 * ============================= */
void loop() {
  if (digitalRead(INT_PIN) == LOW) {
    readAndDumpReport();
  }
}

/* =============================
 * 读取并打印完整 Report
 * ============================= */
void readAndDumpReport() {
  /* 1. 指向 Input Register */
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(INPUT_REG_L);
  Wire.write(INPUT_REG_H);
  if (Wire.endTransmission(false) != 0) {
    Serial.println("I2C set reg failed");
    waitIntRelease();
    return;
  }

  /* 2. 读 Report Length（2 字节） */
  if (Wire.requestFrom(I2C_ADDR, (uint8_t)2) != 2) {
    Serial.println("Length read failed");
    waitIntRelease();
    return;
  }

  uint16_t len = Wire.read();
  len |= (Wire.read() << 8);

  if (len == 0 || len > sizeof(reportBuf)) {
    Serial.print("Invalid report length: ");
    Serial.println(len);
    waitIntRelease();
    return;
  }

  /* 3. 读完整 Report */
  if (Wire.requestFrom(I2C_ADDR, (uint8_t)len) != len) {
    Serial.println("Report read failed");
    waitIntRelease();
    return;
  }

  for (uint16_t i = 0; i < len; i++) {
    reportBuf[i] = Wire.read();
  }

  /* 4. 打印 Report */
  dumpReport(reportBuf, len);

  /* 5. 等待 INT 释放 */
  waitIntRelease();
}

/* =============================
 * 打印格式化 HEX Dump
 * ============================= */
void dumpReport(uint8_t* buf, uint16_t len) {
  Serial.println("--------------------------------");
  Serial.print("LEN = ");
  Serial.println(len);

  for (uint16_t i = 0; i < len; i++) {
    if (i % 16 == 0) {
      Serial.printf("%02X: ", i);
    }

    if (buf[i] < 0x10) Serial.print("0");
    Serial.print(buf[i], HEX);
    Serial.print(" ");

    if (i % 16 == 15 || i == len - 1) {
      Serial.println();
    }
  }
}

/* =============================
 * 等待 INT 拉高，防止重复读
 * ============================= */
void waitIntRelease() {
  while (digitalRead(INT_PIN) == LOW) {
    delayMicroseconds(50);
  }
}
