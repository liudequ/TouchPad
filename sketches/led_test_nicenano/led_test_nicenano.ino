#include <Arduino.h>

// Minimal LED control test for nice!nano (nRF52840)
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
#ifdef LED_STATE_ON
  digitalWrite(LED_BUILTIN, LED_STATE_ON);
#else
  digitalWrite(LED_BUILTIN, HIGH);
#endif
}

void loop() {
  delay(1000);
}
