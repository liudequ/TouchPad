#include <Wire.h>

// ================================
// I2C 基本信息
// ================================

#define TOUCH_ADDR 0x2C     // 你已经扫到的地址
#define READ_REG   0x8100   // 先从这个地址盲读（Goodix 常见）
#define READ_LEN   64       // 先读 32 字节

// ================================
// I2C 读函数（16 位寄存器地址）
// ================================

void i2c_read(uint8_t addr, uint16_t reg, uint8_t* buf, uint16_t len)
{
  Wire.beginTransmission(addr);
  Wire.write(reg >> 8);        // 寄存器高字节
  Wire.write(reg & 0xFF);      // 寄存器低字节
  Wire.endTransmission(false); // repeated start

  Wire.requestFrom(addr, len);
  uint16_t i = 0;
  while (Wire.available() && i < len) {
    buf[i++] = Wire.read();
  }
}

// ================================
// 清触摸状态（Goodix 类常见要求）
// ================================

void clear_touch_status()
{
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(READ_REG >> 8);
  Wire.write(READ_REG & 0xFF);
  Wire.write(0x00);
  Wire.endTransmission();
}

// ================================
// Arduino 生命周期
// ================================

uint8_t buffer[READ_LEN];

void setup()
{
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Wire.begin();        // 默认 I2C 总线
  Wire.setClock(400000); // 400kHz，触摸 IC 常用

  Serial.println("Touch blind read start...");
}

void loop()
{
  // 读一帧数据
  i2c_read(TOUCH_ADDR, READ_REG, buffer, READ_LEN);

  // 打印
  Serial.print("DATA: ");
  for (int i = 0; i < READ_LEN; i++) {
    if (buffer[i] < 0x10) Serial.print("0");
    Serial.print(buffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // 清状态（如果是 Goodix 类，这是必须的）
  clear_touch_status();

  delay(100);
}
