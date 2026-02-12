# 蓝牙改造记录（历史）

> 说明：本文件主要保留迁移过程记录；当前实现以 `sketches/product/touchpad_hid_nrf/touchpad_hid_nrf.ino` 为准。

## 目标
将触摸板迁移到 nRF52840，实现 BLE HID，并保留 USB 使用路径。

## 已落地现状（当前）
- 触摸板 I2C 读取已接入（含 INT 触发读取与 TP_EN 时序）。
- 手势映射已接入：单指移动、双指滚动、三指滑动、四角区域动作。
- 输出层已支持 USB HID + BLE HID。
- 连接管理已包含广播、连接/断开回调、清配对（`PAIRCLR`）。
- 配置存储已接入（板载文件系统）。
- 配置通道当前以 USB 串口为主（UI 侧 BLE 调参已移除）。
- 省电策略已包含空闲分级与相关配置项（`bleIdle*`、`lightIdleRate`）。

## 迁移期间关键结论（保留）
- nRF52840 生态成熟，BLE HID 实用性较好。

## 硬件与功耗备忘
- 可利用 `PIN_EXT_VCC` / `EXT_VCC`（不同变体命名可能不同）控制外设供电。
- 板载 LED 可用于连接/省电状态指示。
- 完全断电策略会影响唤醒与连接时延，需按续航目标取舍。

## BLE 抓包与上报频率分析（保留）
用于评估 BLE HID 实际上报频率。

### btmon + Wireshark
1. `sudo btmon -w /tmp/ble.btsnoop`
2. 连接触摸板并操作 10-20 秒。
3. 停止抓包。
4. 用 Wireshark 打开 `/tmp/ble.btsnoop`。
5. 过滤：`btatt.opcode == 0x1b`

### 命令行计算平均间隔
```bash
tshark -r /tmp/ble.btsnoop -Y 'btatt.opcode==0x1b' -T fields -e frame.time_delta_displayed \
| awk '{sum+=$1; n++} END {if(n>0) printf("avg=%.6f s, freq=%.2f Hz\\n", sum/n, 1/(sum/n))}'
```
