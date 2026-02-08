# FPC 转接板针脚定义

## nRF52840 SuperMini / nice!nano v2（当前方案）
- Pin6 VCC -> 3V3
- Pin5 SCL -> D7（P0.11）
- Pin4 SDA -> D6（P1.00）
- Pin3 INT -> D8（P1.04）
- Pin2 ENABLE -> D9（P1.06）
- Pin1 GND -> GND

## 微动复位开关（可选）
- 微动开关一端 -> `RST`
- 微动开关另一端 -> `GND`
- 按下时短接 `RST` 到 `GND`，触发硬件复位
