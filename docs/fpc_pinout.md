# FPC 转接板针脚定义

## nRF52840 SuperMini / nice!nano v2（当前方案）
- Pin6 VCC -> 3V3
- Pin5 SCL -> D8
- Pin4 SDA -> D9
- Pin3 INT -> D7
- Pin2 ENABLE -> D6
- Pin1 GND -> GND

## 已知问题记录
- 当前项目发现：FPC 转接模块在 PCB 底板设计时方向设计反了（序号镜像）。
- 影响：按丝印直连时可能出现引脚对应错误，导致触摸板异常（如设备发热、无法通信）。
- 建议：量产前修正底板设计；临时调试请先用万用表逐脚确认 `Pin1~Pin6` 实际对应关系后再上电。
