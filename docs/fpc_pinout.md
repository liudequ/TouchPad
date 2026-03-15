# FPC 转接板针脚定义

## nRF52840 SuperMini / nice!nano v2（当前方案）
- Pin1 VCC -> 3V3
- Pin2 SCL -> D8
- Pin3 SDA -> D9
- Pin4 INT -> D7
- Pin5 ENABLE -> D6
- Pin6 GND -> GND

## 已知问题记录
- 当前文档已按镜像后的实际顺序更新。
- 若继续参考旧版本资料或旧丝印，可能会把 `Pin1~Pin6` 顺序看反，导致触摸板异常（如设备发热、无法通信）。
- 建议：上电前先用万用表逐脚确认 `Pin1~Pin6` 实际对应关系。
