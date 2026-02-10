# 触摸板使用说明（nRF 版）

本项目将触摸板输入转换为 HID 输入设备输出，支持 USB 与 BLE。

## 硬件连接（nice!nano v2 / SuperMini nRF52840）
- 触摸板 I2C：`SDA=D8`、`SCL=D9`
- 中断引脚：`INT=D7`（上拉输入）
- 使能引脚：`TP_EN=D6`
- 设备 I2C 地址：`0x2C`

如需修改引脚或地址，编辑 `sketches/nrf/touchpad_hid_nrf/touchpad_hid_nrf.ino` 顶部宏定义。
物料清单见 `docs/bom.md`。

## 固件烧录流程（推荐）
1. 在 Arduino IDE 中打开 `sketches/nrf/touchpad_hid_nrf/touchpad_hid_nrf.ino`。
2. 选择板卡与串口，执行“导出编译二进制文件”（Export Compiled Binary）。
3. 在仓库根目录运行：
   ```bash
   tools/make_uf2_latest_nicenano_v2.sh
   ```
4. 将生成的 `.uf2` 文件复制到开发板挂载出的 UF2/U 盘中完成刷写。

## 操作方式
- 单指滑动：移动鼠标指针。
- 双指上下滑动：滚轮滚动。
- 单指轻触抬起：单击。
- 双击：在时间窗口内连续两次轻触抬起。
- 左上角轻触：后退（默认 Alt+Left）。
- 右上角轻触：前进（默认 Alt+Right）。
- 右下角轻触：右键（默认）。
- 左下角轻触：默认未绑定，可在配置中自定义。
- 三指左右/上下滑动：触发配置绑定动作。
- 三指单击/双击：触发配置绑定动作。
- 四指左右/上下滑动：触发配置绑定动作。
- 四指单击/双击：触发配置绑定动作。

## 区域说明
当前区域划分为上边 `20%` 高度、左右各 `35%` 宽度。
- 左上角区域：后退
- 右上角区域：前进
- 右下角区域：右键
- 左下角区域：默认不启用

可通过参数 `topZonePercent`、`sideZonePercent`、`enableNavZones` 调整。

## 串口配置（当前固件）
固件支持串口命令配置并持久化到板载文件系统。

常用命令：
```text
HELP
GET
GET <key>
SET <key> <value>
SAVE
LOAD
RESET
PAIRCLR
```

常见参数：
- 移动/滚动：`sensitivity`、`smoothFactor`、`accelFactor`、`maxAccel`、`maxDelta`、`moveDeadband`、`scrollSensitivity`
- 区域：`topZonePercent`、`sideZonePercent`、`enableNavZones`
- 四角动作：`leftTop*`、`rightTop*`、`rightBottom*`、`leftBottom*`
- 三指动作：`threeLeft*`、`threeRight*`、`threeUp*`、`threeDown*`
- 三指点击：`threeTap*`、`threeDoubleTap*`
- 三指阈值：`threeSwipeThresholdX`、`threeSwipeThresholdY`、`threeSwipeTimeout`、`threeSwipeCooldown`
- 四指动作：`fourLeft*`、`fourRight*`、`fourUp*`、`fourDown*`、`fourTap*`、`fourDoubleTap*`
- 四指阈值：`fourSwipeThresholdX`、`fourSwipeThresholdY`、`fourSwipeTimeout`、`fourSwipeCooldown`
- 连接/省电：`useBleWhenUsb`、`bleIdleSleepEnabled`、`bleIdleLightMs`、`bleIdleMediumMs`、`bleIdleSleepMs`、`lightIdleRate`

## 三阶段省电策略
当 `bleIdleSleepEnabled=1` 且使用 BLE 传输时，固件会按空闲时长进入三阶段省电：

1. 第一阶段（轻度空闲）  
触发条件：空闲时长 >= `bleIdleLightMs`  
行为：进入轻度省电状态，降低活跃度并按 `lightIdleRate` 控制上报节奏。

2. 第二阶段（中度空闲）  
触发条件：空闲时长 >= `bleIdleMediumMs`  
行为：进入更深一层省电状态，断开当前 BLE 连接并停止广播。

3. 第三阶段（深度空闲）  
触发条件：空闲时长 >= `bleIdleSleepMs`  
行为：进入深度睡眠，进一步降低功耗，等待唤醒。

说明：
- 阈值需满足：`bleIdleLightMs < bleIdleMediumMs < bleIdleSleepMs`。固件会在内部做最小间隔校正。
- 任意新的触摸操作会更新活动时间并退出空闲阶段，恢复正常工作状态。

区域类型可选值：`NONE`、`MOUSE`、`KEYBOARD`。

常用修饰键位掩码（可组合）：`CTRL=1`、`SHIFT=2`、`ALT=4`、`GUI=8`。
常用按键码示例：`RIGHT=79`、`LEFT=80`、`DOWN=81`、`UP=82`。

示例：
```text
GET
SET scrollSensitivity 0.00002
SET leftBottomType MOUSE
SET leftBottomButtons 2
SET threeLeftType KEYBOARD
SET threeLeftModifier 5
SET threeLeftKey 80
SET sideZonePercent 35
SAVE
```

## 上位机 UI（Python）
可使用 `tools/ui/touchpad_config_ui.py` 图形化配置。

依赖：
- `pyserial`
- `PySide6`
- `bleak`（当前 nRF 固件仅保留 USB 串口调参，BLE 调参已移除）

运行：
```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r tools/ui/requirements.txt
python3 tools/ui/touchpad_config_ui.py
```

或直接：
```bash
tools/ui/run_ui.sh
```

## 串口权限（Linux）
首次在新机器连接时，若遇到 `/dev/ttyACM0` 权限不足，可将当前用户加入 `dialout` 组：

```bash
groups
sudo usermod -aG dialout $USER
```

执行后注销并重新登录。
