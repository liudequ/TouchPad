# 触摸板使用说明

本项目将触摸板输入转换为 USB 鼠标输出。按以下步骤连接与操作。

## 硬件连接
- 触摸板 I2C：`SDA=GP4`、`SCL=GP5`
- 中断引脚：`INT=GP10`（上拉）
- 使能引脚：`TP_EN=GP9`
- 设备 I2C 地址：`0x2C`

如需修改引脚或地址，编辑 `sketches/touchpad_hid/touchpad_hid.ino` 顶部的宏定义。

## 烧录步骤
1. 在 Arduino IDE 中打开 `sketches/touchpad_hid/touchpad_hid.ino`。
2. 选择板卡与串口。
3. 点击 **Upload** 上传固件。

## 操作方式
- 单指滑动：移动鼠标指针。
- 双指上下滑动：滚轮滚动。
- 单指轻触抬起：单击。
- 双击：在短时间内连续两次轻触抬起。
- 左上角轻触：后退（Alt+Left）。
- 右上角轻触：前进（Alt+Right）。
- 右下角轻触：右键。

## 区域说明
当前区域划分为上边 20% 高度、左右各 35% 宽度：
- 左上角区域：后退
- 右上角区域：前进
- 右下角区域：右键
如需调整区域大小，修改 `TOP_ZONE_PERCENT` 和 `SIDE_ZONE_PERCENT`。

示意图（非比例）：
```
┌──────────────────────────────┐
│  后退            前进         │  上边 20%
│  (左上)         (右上)        │
│                              │
│                              │
│                              │
│                 右键         │  下边 20%
│                (右下)        │
└──────────────────────────────┘
   左 35%                 右 35%
```

## 坐标范围记录
实测坐标范围如下（用于区域划分与调试）：
- 左上角：`(0, 0)`
- 右下角：`(2628, 1332)`

## 调参建议
如需调整手感，可修改以下参数：
- 移动相关：`sensitivity`、`smoothFactor`、`accelFactor`
- 滚轮相关：`scrollSensitivity`、`scrollSmoothFactor`
- 点击相关：`TAP_MAX_MS`、`DOUBLE_TAP_WINDOW`
- 双击距离：`DOUBLE_TAP_MAX_MOVE`（两次点击位置的距离阈值）
- 方向相关：`naturalScroll`（`true` 为自然滚动，`false` 为传统滚动）
- 区域相关：`TOP_ZONE_PERCENT`、`SIDE_ZONE_PERCENT`、`enableNavZones`

## 串口配置（区域与滚动）
当前支持通过串口修改并保存以下参数：
- `scrollSensitivity`
- `topZonePercent`（上边高度百分比，5~50）
- `sideZonePercent`（左右宽度百分比，5~50）
- `enableNavZones`（0/1）
- `leftTopType` / `rightTopType` / `rightBottomType` / `leftBottomType`
- `leftTopButtons` / `rightTopButtons` / `rightBottomButtons` / `leftBottomButtons`
- `leftTopModifier` / `rightTopModifier` / `rightBottomModifier` / `leftBottomModifier`
- `leftTopKey` / `rightTopKey` / `rightBottomKey` / `leftBottomKey`

区域类型可选值：`NONE`、`MOUSE`、`KEYBOARD`。

常用修饰键位掩码（可组合）：`CTRL=1`、`SHIFT=2`、`ALT=4`、`GUI=8`。

常用按键码示例：`RIGHT=79`、`LEFT=80`、`DOWN=81`、`UP=82`。

示例：
```
HELP
GET
GET leftTopType
SET scrollSensitivity 0.00002
SET leftBottomType MOUSE
SET leftBottomButtons 2
SET sideZonePercent 35
SAVE
LOAD
RESET
```
说明：
- `SET` 修改内存中的参数，`SAVE` 写入 Flash，`LOAD` 从 Flash 读取。
- 串口波特率固定为 `115200`。

## 依赖说明
RP2040 使用 TinyUSB 复合 HID（鼠标/键盘）。请确保可用的 `Adafruit_TinyUSB` 支持。

## 上位机 UI（Python）
可以使用 `tools/ui/touchpad_config_ui.py` 打开配置界面。依赖：
- `pyserial`
- `PySide6`

运行示例：
```
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
python3 tools/ui/touchpad_config_ui.py
```

也可以直接运行脚本：
```
tools/ui/run_ui.sh
```

如果需要双击启动，可使用：
- `tools/ui/touchpad_config.desktop`

修改后重新编译上传即可生效。
