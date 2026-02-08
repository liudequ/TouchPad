# 仓库指南

## 项目结构与模块组织
这是一个 Arduino 草图集合仓库。所有可编译草图放在 `sketches/` 下，并按 `common/`、`nrf/` 分组，每个草图一个独立文件夹，且 `.ino` 文件名与文件夹名一致：

- `sketches/common/blink/blink.ino`
- `sketches/common/ps2_scan/ps2_scan.ino`
- `sketches/common/hid_report_dump/hid_report_dump.ino`
- `sketches/common/two_finger_tuning/two_finger_tuning.ino`
- `sketches/common/try_read_data/try_read_data.ino`
- `sketches/nrf/touchpad_hid_nrf/touchpad_hid_nrf.ino`
- `sketches/nrf/touchpad_i2c_probe_nrf/touchpad_i2c_probe_nrf.ino`
- `sketches/nrf/ble_conn_test/ble_conn_test.ino`
- `sketches/nrf/ble_hid_rate_test/ble_hid_rate_test.ino`
- `sketches/nrf/serial_alive_nicenano/serial_alive_nicenano.ino`

文档放在 `docs/`。每个草图自包含、互不依赖。
3D 模型放在 `models/`，用于外壳设计与打印参考。

## 文档说明（docs）
- `docs/touchpad_usage.md`：nRF 触摸板使用说明（接线、烧录、操作、串口命令与 UI 配置）。
- `docs/arduino_ide_supermini_setup.md`：Arduino IDE 本地板卡配置与 UF2 刷写流程说明。
- `docs/ble_conn_test.md`：`ble_conn_test` 草图的人工测试步骤与预期结果。
- `docs/bluetooth_migration_notes.md`：蓝牙迁移历史记录与现状说明（含 BLE 抓包分析方法）。
- `docs/bom.md`：触摸板项目物料清单（BOM）与接线速查。
- `docs/video_intro_flow.md`：触摸板介绍视频的分段流程与口播参考。
- `docs/stability_tuning.md`：手感稳定性与防抖调参思路清单。
- `docs/fpc_pinout.md`：触摸板 FPC 转接板引脚定义（nRF 方案）。
- `docs/schematic_nice_nano_v2.png`：nice!nano v2 原理图参考图。
- `docs/Promicro NRF52840开发板 兼容nice!nano V2.0 带蓝牙 充电管理-淘宝网.png`：开发板商品页截图参考。
- `docs/nRF52840_PS_v1.1.pdf`：nRF52840 官方规格书（参考资料）。

## 模型说明（models）
- `models/touchpad_case.scad`：触摸板外壳 OpenSCAD 源文件。
- `models/touchpad_case.stl`：触摸板外壳可打印 STL 文件。
- `models/case_notes.md`：外壳设计与打印参数说明。
- 备注：当前 3D 模型较粗糙，主要用于尺寸与装配验证，实际空间有很大的冗余。

## 构建、测试与开发命令
当前推荐流程（nice!nano v2）：

1. 在 Arduino IDE 打开目标草图，执行“导出编译二进制文件”（Export Compiled Binary）。
2. 在仓库根目录运行：
   - `tools/make_uf2_latest_nicenano_v2.sh`
3. 脚本会基于最新导出的二进制生成对应 `.uf2` 文件。
4. 将生成的 `.uf2` 文件复制到开发板暴露的 UF2/U 盘磁盘中完成刷写。

可选：若需要命令行编译，可使用 Arduino CLI：
- `arduino-cli compile --fqbn <board_fqbn> <sketch_path>`

将 `<board_fqbn>` 替换为实际板卡，将 `<sketch_path>` 替换为草图路径（如 `sketches/nrf/touchpad_hid_nrf`）。

## 代码风格与命名规范
- 采用标准 Arduino C++ 风格：2 空格缩进、K&R 花括号风格、清晰的函数命名。
- 变量命名应具描述性，尽量减少全局变量。
- 草图文件名与其所在目录保持一致（如 `sketches/common/ps2_scan/ps2_scan.ino`）。
- 触摸板主逻辑改动优先同步更新 `sketches/nrf/touchpad_hid_nrf` 与相关文档（如 `docs/touchpad_usage.md`）。
- 串口/日志输出尽量使用中文。

## 测试指南
当前无自动化测试。请在目标硬件上编译并上传，使用串口监视器或设备输出验证行为。

## 提交与合并请求规范
本仓库包含 git 历史。建议使用简短、祈使语气的提交信息（例如：“Add I2C scan timeout”）。

提交变更时请包含：
- 受影响草图的简短描述
- 用于验证的板卡/固件环境
- 任何连线或硬件假设
- 提交日志（commit message）使用中文

补充：如果用户未指定提交信息，可由助手根据本次变更自行总结。

## 配置提示
部分草图涉及 USB HID 或 I2C/PS2 设备。请确认板卡支持相关接口，并在草图顶部按需调整引脚映射。

## 触摸板代码概览
这是一个触摸板项目。关键草图及职责如下：

- `sketches/nrf/touchpad_hid_nrf/touchpad_hid_nrf.ino`：nRF 主触摸板实现（I2C-HID 读取 `0x2C`/`0x0109`，单指移动、双指滚动、三指手势、点击/双击、USB/BLE HID 输出、省电分级、配置持久化）。
- `sketches/common/two_finger_tuning/two_finger_tuning.ino`：触摸到鼠标映射的实验版本，带报文打印，重点是单指/双指的平滑与加速。
- `sketches/common/hid_report_dump/hid_report_dump.ino`：I2C-HID 报文抓取与十六进制打印，便于理解原始输入帧。
- `sketches/common/try_read_data/try_read_data.ino`：Goodix 风格寄存器盲读工具（`0x8100`）及状态清理。
- `sketches/nrf/touchpad_i2c_probe_nrf/touchpad_i2c_probe_nrf.ino`：nRF 侧 I2C 触摸板探测工具（用于定位触摸控制器地址与连线确认）。
