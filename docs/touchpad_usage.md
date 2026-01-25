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
- 右侧 15% 区域内上下滑动：音量加/减。

## 区域说明
当前区域划分为上边 20% 高度、左右各 20% 宽度：
- 左上角区域：后退
- 右上角区域：前进
- 右下角区域：右键
右侧 15% 区域用于音量手势（只要单指起始点位于该区域）。
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
   左 20%                 右 20%
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
- 音量相关：`VOLUME_ZONE_PERCENT`、`VOLUME_STEP`

## 依赖说明
音量手势在 RP2040 上使用 TinyUSB 的 Consumer 控制键。
- 需要可用的 `Adafruit_TinyUSB`（RP2040 常见核心已内置）
- 如果编译失败，请确认已启用 TinyUSB 支持

修改后重新编译上传即可生效。
