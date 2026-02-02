#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start < 2000)) {
    delay(10);
  }
  Serial.println("[alive] serial ready");
}

void loop() {
  Serial.print("[alive] millis=");
  Serial.println(millis());
  delay(1000);
}
