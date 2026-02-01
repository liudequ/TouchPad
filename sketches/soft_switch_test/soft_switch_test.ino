#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#if defined(PIN_EXT_VCC)
#define SWITCH_PIN PIN_EXT_VCC
#elif defined(EXT_VCC)
#define SWITCH_PIN EXT_VCC
#elif defined(P0_13)
#define SWITCH_PIN P0_13
#else
#define SWITCH_PIN 255
#endif

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && (millis() - start < 2000)) {
    delay(10);
  }

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_STATE_ON);

  if (SWITCH_PIN == 255) {
    Serial.println("[switch] no EXT_VCC/P0_13 pin definition");
    return;
  }

  pinMode(SWITCH_PIN, OUTPUT);
  digitalWrite(SWITCH_PIN, HIGH);
  Serial.println("[switch] EXT_VCC HIGH (power on)");
}

void loop() {
  if (SWITCH_PIN == 255) {
    delay(1000);
    return;
  }

  digitalWrite(SWITCH_PIN, LOW);
  digitalWrite(LED_BUILTIN, !LED_STATE_ON);
  Serial.println("[switch] EXT_VCC LOW (power off)");
  delay(2000);

  digitalWrite(SWITCH_PIN, HIGH);
  digitalWrite(LED_BUILTIN, LED_STATE_ON);
  Serial.println("[switch] EXT_VCC HIGH (power on)");
  delay(2000);
}
