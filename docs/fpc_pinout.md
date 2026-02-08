# FPC 转接板针脚定义

## nRF52840 SuperMini / nice!nano v2（当前方案）
- Pin6 VCC -> 3V3
- Pin5 SCL -> D7（P0.11）
- Pin4 SDA -> D6（P1.00）
- Pin3 INT -> D8（P1.04）
- Pin2 ENABLE -> D9（P1.06）
- Pin1 GND -> GND

## RP2040（Pico）连接定义（历史方案）
- Pin6 VCC -> 3V3
- Pin5 SCL -> GP5
- Pin4 SDA -> GP4
- Pin3 INT -> GP10
- Pin2 ENABLE -> GP9（通过代码控制拉高/拉低）
  - High：TouchPad ENABLE
  - Low：TouchPad Disable
- Pin1 GND -> GND
