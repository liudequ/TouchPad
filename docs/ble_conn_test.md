# BLE 连接测试

这是 `sketches/ble_conn_test/ble_conn_test.ino` 在 nRF52840 上的人工测试用例
（兼容 nice!nano v2.0）。

## 前置条件
- 开发板：Pro Micro nRF52840（兼容 nice!nano v2.0）
- 已安装 Arduino core + Adafruit Bluefruit nRF52
- 可用串口监视器（115200 波特率）
- 一台可连接 BLE HID + BLE UART 的手机或电脑

## 测试用例：BLE 连接 + HID/UART
1) 编译并上传 `sketches/ble_conn_test` 到开发板。
2) 打开串口监视器，波特率 115200。
3) 确认日志出现：`[ble] nRF52 BLE connection test`。
4) 在手机/电脑上扫描 BLE 设备并连接 `TouchPadTest`。
5) 确认串口输出 `[ble] connected`。
6) 在串口监视器发送 `INFO`，确认状态显示已连接且带 RSSI。
7) 发送 `HID CLICK`，确认主机收到一次左键点击。
8) 发送 `HID MOVE 20 -10`，确认鼠标光标移动。
9) 若有 BLE UART App，发送 `UART hello`，确认 App 接收到 `hello`。
10) 从主机断开，确认串口输出 `[ble] disconnected`。

## 预期结果
- 设备以 `TouchPadTest` 名称广播。
- 串口能看到连接/断开日志。
- HID 点击/移动能被主机接收。
- BLE UART 在连接状态下能发送文本。

## 备注
- 若配对异常，可在串口监视器发送 `PAIRCLR` 清除配对记录。
- 若主机未显示 HID，请删除配对并重新连接。
