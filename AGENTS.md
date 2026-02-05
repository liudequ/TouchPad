# 仓库指南

## 项目结构与模块组织
这是一个 Arduino 草图集合仓库。所有可编译草图放在 `sketches/` 下，并按 `common/`、`pico/`、`nrf/` 分组，每个草图一个独立文件夹，且 `.ino` 文件名与文件夹名一致：

- `sketches/common/blink/blink.ino`
- `sketches/pico/i2c_scan/i2c_scan.ino`
- `sketches/common/ps2_scan/ps2_scan.ino`
- `sketches/common/hid_report_dump/hid_report_dump.ino`
- `sketches/pico/touchpad_hid_pico/touchpad_hid_pico.ino`
- `sketches/nrf/touchpad_hid_nrf/touchpad_hid_nrf.ino`
- `sketches/common/two_finger_tuning/two_finger_tuning.ino`
- `sketches/common/try_read_data/try_read_data.ino`
- `sketches/pico/tinyusb_fake_input/tinyusb_fake_input.ino`

文档放在 `docs/`。每个草图自包含、互不依赖。

## 构建、测试与开发命令
这些草图通过 Arduino IDE 或 Arduino CLI 进行编译与上传。

- Arduino IDE：打开草图目录，点击 **Verify**（编译）或 **Upload**（上传）。
- Arduino CLI（若已安装）：
  - `arduino-cli compile --fqbn <board_fqbn> <sketch_path>`
  - `arduino-cli upload --fqbn <board_fqbn> -p <port> <sketch_path>`

将 `<board_fqbn>` 替换为实际板卡（如 `arduino:avr:uno`），将 `<sketch_path>` 替换为草图路径（如 `sketches/pico/i2c_scan`）。

## 代码风格与命名规范
- 采用标准 Arduino C++ 风格：2 空格缩进、K&R 花括号风格、清晰的函数命名。
- 变量命名应具描述性，尽量减少全局变量。
- 草图文件名与其所在目录保持一致（如 `sketches/common/ps2_scan/ps2_scan.ino`）。
- 通用逻辑改动需同步更新 `sketches/pico/touchpad_hid_pico` 与 `sketches/nrf/touchpad_hid_nrf`。

## 测试指南
当前无自动化测试。请在目标硬件上编译并上传，使用串口监视器或设备输出验证行为。

## 提交与合并请求规范
本仓库未包含 git 历史，无法推断提交规范。如需新增规范，建议使用简短、祈使语气的提交信息（例如：“Add I2C scan timeout”）。

提交变更时请包含：
- 受影响草图的简短描述
- 用于验证的板卡/固件环境
- 任何连线或硬件假设

补充：如果用户未指定提交信息，可由助手根据本次变更自行总结。

## 配置提示
部分草图涉及 USB HID 或 I2C/PS2 设备。请确认板卡支持相关接口，并在草图顶部按需调整引脚映射。

## 触摸板代码概览
这是一个触摸板项目。关键草图及职责如下：

- `sketches/pico/touchpad_hid_pico/touchpad_hid_pico.ino`：Pico 版主触摸板到鼠标实现（I2C-HID 读取 `0x2C`/`0x0109`，单指移动、双指滚动、平滑/加速、双击、INT 触发读取、冷启动使能时序）。
- `sketches/nrf/touchpad_hid_nrf/touchpad_hid_nrf.ino`：NRF 版占位草图（待接入 BLE HID 与 Pico 版逻辑）。
- `sketches/common/two_finger_tuning/two_finger_tuning.ino`：触摸到鼠标映射的实验版本，带报文打印，重点是单指/双指的平滑与加速。
- `sketches/common/hid_report_dump/hid_report_dump.ino`：I2C-HID 报文抓取与十六进制打印，便于理解原始输入帧。
- `sketches/common/try_read_data/try_read_data.ino`：Goodix 风格寄存器盲读工具（`0x8100`）及状态清理。
- `sketches/pico/i2c_scan/i2c_scan.ino`：I2C 地址扫描器（用于定位触摸控制器地址）。
