const int PS2_1 = 4;
const int PS2_2 = 5;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  // 1. 主机抑制阶段：拉低
  pinMode(PS2_1, OUTPUT);
  pinMode(PS2_2, OUTPUT);
  digitalWrite(PS2_1, LOW);
  digitalWrite(PS2_2, LOW);

  delay(100);  // 100ms 抑制

  // 2. 释放总线
  pinMode(PS2_1, INPUT);
  pinMode(PS2_2, INPUT);

  Serial.println("Released bus, listening...");
}

void loop() {
  static int last1 = HIGH;
  static int last2 = HIGH;

  int v1 = digitalRead(PS2_1);
  int v2 = digitalRead(PS2_2);

  if (v1 != last1 || v2 != last2) {
    Serial.print("P1=");
    Serial.print(v1);
    Serial.print("  P2=");
    Serial.println(v2);
    last1 = v1;
    last2 = v2;
  }
}
