#include <Wire.h>

/*
  NRF52840 版占位草图：用于后续接入 BLE HID。
  TODO:
  - 选择 nRF52840 的 SDA/SCL/INT/TP_EN 引脚
  - 初始化 BLE HID（鼠标/键盘）
  - 迁移 Pico 版触摸板读取与手势逻辑
*/

#define I2C_ADDR 0x2C
#define INPUT_REG_L 0x09
#define INPUT_REG_H 0x01

// TODO: 按所用板卡修正引脚
#define SDA_PIN 0
#define SCL_PIN 1
#define INT_PIN 2
#define TP_EN 3

void setup() {
  Serial.begin(115200);

  pinMode(TP_EN, OUTPUT);
  digitalWrite(TP_EN, HIGH);

  pinMode(INT_PIN, INPUT_PULLUP);

  Wire.setSDA(SDA_PIN);
  Wire.setSCL(SCL_PIN);
  Wire.begin();
  Wire.setClock(400000);
}

void loop() {
  // TODO: 读取触摸板 I2C-HID 报文，转换为 BLE HID 报告并发送
}
