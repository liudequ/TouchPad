#include <Wire.h>
#include <Adafruit_TinyUSB.h>

// nRF52840 SuperMini pin mapping (方案 A)
#define SDA_PIN 8
#define SCL_PIN 9
#define INT_PIN 7
#define TP_EN 6

#define I2C_ADDR 0x2C

void touchColdBoot() {
  pinMode(TP_EN, OUTPUT);
  digitalWrite(TP_EN, LOW);
  delay(20);
  digitalWrite(TP_EN, HIGH);
  delay(150);
}

void scanI2C() {
  uint8_t found = 0;
  Serial.println("[i2c] scanning...");
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("[i2c] found 0x");
      if (addr < 0x10) Serial.print('0');
      Serial.println(addr, HEX);
      found++;
    }
    delay(2);
  }
  if (found == 0) {
    Serial.println("[i2c] no devices found");
  }
}

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start < 2000)) {
    delay(10);
  }
  delay(100);
  Serial.println("[probe] touchpad i2c probe (nRF52840)");

  pinMode(INT_PIN, INPUT_PULLUP);
  touchColdBoot();

#if defined(ARDUINO_ARCH_NRF52)
  Wire.setPins(SDA_PIN, SCL_PIN);
#else
  Wire.setSDA(SDA_PIN);
  Wire.setSCL(SCL_PIN);
#endif
  Wire.begin();
  Wire.setClock(400000);

  scanI2C();
  Serial.print("[probe] INT=");
  Serial.println(digitalRead(INT_PIN));
  Serial.println("[probe] done");
}

void loop() {
  delay(2000);
}
