# 蓝牙改造记录（初稿）

## 目标
将当前触摸板改为 BLE HID 连接，开发板选用 nRF52840。

## 关键结论
- 现有 RP2040/Pico 不支持 BLE，无法仅靠固件改成蓝牙，需要更换硬件或外接 BLE 模块。
- nRF52840 生态成熟、BLE HID 稳定，适合作为主控。

## 迁移思路
- 触摸板 I2C 读取与手势解析逻辑基本保留。
- 将 USB HID 输出替换为 BLE HID 报告发送。
- 串口配置可保留（USB 串口），也可改为 BLE GATT 配置。
- 增加 BLE 连接管理（配对、重连、低功耗）。

## NRF 版实现清单（逐步）
1. 触摸板读取：I2C 初始化、INT 触发读取、TP_EN 冷启动时序、引脚映射。
2. 手势/映射逻辑复用：单指移动、双指滚动、三指滑动、区域动作。
3. 输出层替换：USB HID 改为 BLE HID（鼠标/键盘报告）。
4. 连接管理：BLE 广播、配对/重连、连接状态提示。
5. 配置存储：替代 LittleFS 的持久化方案（或同类 FS）。
6. 配置通道：USB 串口保留或改为 BLE GATT 配置。

当前进展：
- 已完成：逻辑复用、BLE HID 输出、I2C 初始化/冷启动框架。
- 未完成：引脚映射、配置存储、连接管理细化、BLE 配置通道。

建议补充事项：
- 板卡依赖与 `sketches/nrf/ble_conn_test/ble_conn_test.ino` 相同：Adafruit nRF52 core + Adafruit Bluefruit nRF52 Libraries。
- BLE 连接参数调优（延迟与功耗）。
- 低功耗策略与唤醒。
- 电量检测/电池状态（后续上电池时）。

## 省电优化记录
- P0.13 / EXT_VCC 软开关：在 nice!nano 变体中，`PIN_EXT_VCC` / `EXT_VCC` / `P0_13` 映射到 P0.13，拉低可关闭外设 3.3V（EXT_VCC），用于断电触摸板等外设以降低功耗。
  ```cpp
  pinMode(PIN_EXT_VCC, OUTPUT);
  digitalWrite(PIN_EXT_VCC, LOW);
  ```
- 板载红灯可由 P0.15 控制（BLED），在 `nice_nano` 变体中对应 `LED_BUILTIN`。
 - 当前策略：先不做“完全断电”方案，保持触摸板 INT 作为唤醒源，后续再根据续航评估是否引入断电方案。

## 现成硬件清单（不自制载板）
必需：
- Pro Micro nRF52840 开发板（带电池接口/充电）
- 触摸板本体（I2C/INT/EN）
- FPC 转接板/转接线（排线转杜邦）
- 杜邦线/细导线
- 3.7V LiPo 电池
- USB 线（刷机/供电/充电）

建议：
- 电源开关（串电池正极或用板上 EN）
- I2C 上拉电阻（触摸板/板子未自带时）
- 绝缘胶带/双面胶（固定器件）

可选：
- 小外壳/支架
- ESD 保护（排线口）
- 指示灯（电源/连接）

## 延迟与功耗要点
- BLE 延迟主要受连接间隔、从机延迟、报告节流影响。
- 空闲时进入低功耗或深睡可显著节能，但会增加唤醒/重连延迟。

## BLE 抓包与上报频率分析
用于评估 BLE HID 实际上报频率（连接间隔、主机协商结果）。

### btmon + Wireshark（无需额外硬件）
1. 开始抓包：`sudo btmon -w /tmp/ble.btsnoop`
2. 连接触摸板并操作 10–20 秒。
3. 停止抓包（Ctrl+C）。
4. Wireshark 打开 `/tmp/ble.btsnoop`。
5. 过滤 HID 输入通知：`btatt.opcode == 0x1b`
6. 查看间隔/频率：
   - 视图 → 时间显示格式 → 与上一显示分组的时间差（秒）
   - 统计 → IO 图表（设置时间间隔如 0.01s 观察密度）

### 命令行直接算平均间隔
```bash
tshark -r /tmp/ble.btsnoop -Y 'btatt.opcode==0x1b' -T fields -e frame.time_delta_displayed \
| awk '{sum+=$1; n++} END {if(n>0) printf("avg=%.6f s, freq=%.2f Hz\n", sum/n, 1/(sum/n))}'
```

## 待确认事项
- 目标主机系统（Windows/macOS/Linux）。
- 目标电池容量与续航要求。
- 是否需要离线配置（无 USB 时的 BLE 配置通道）。
 - 电量检测/上报与低电量策略先不做，后续根据续航评估再决定。
