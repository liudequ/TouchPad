# TouchPad 半一体化底板 Netlist

适用方案：
- `nice!nano v2` 直焊
- FPC 底座 `6P, 1.0mm`
- 电池座 `2Pin`
- 复位微动开关 `3x6x4.3`

## 网络连接表
| 网络名 | 连接点A | 连接点B |
|---|---|---|
| `GND` | `J1-1 (FPC Pin1)` | `U1-GND` |
| `TP_VCC_3V3` | `J1-6 (FPC Pin6)` | `U1-3V3` |
| `I2C_SDA` | `J1-4 (FPC Pin4)` | `U1-D7` |
| `I2C_SCL` | `J1-5 (FPC Pin5)` | `U1-D6` |
| `TP_INT` | `J1-3 (FPC Pin3)` | `U1-D8` |
| `TP_EN` | `J1-2 (FPC Pin2)` | `U1-D9` |
| `RST_BTN` | `SW1-1` | `U1-RST` |
| `RST_BTN_GND` | `SW1-2` | `U1-GND` |
| `BAT_NEG` | `BT1-1` | `U1-GND` |
| `BAT_POS` | `BT1-2` | `U1-BAT` |

## 器件位号建议
- `U1`: nice!nano v2
- `J1`: FPC 底座（6P, 1.0mm）
- `BT1`: 电池座（2Pin）
- `SW1`: 复位微动开关（3x6x4.3）

## FPC 引脚定义（按当前文档）
- Pin1 = GND
- Pin2 = EN
- Pin3 = INT
- Pin4 = SDA
- Pin5 = SCL
- Pin6 = VCC(3V3)

## 注意事项
- `U1-BAT` 请按你手上 `nice!nano v2` 实物丝印确认（可能标注为 `BAT`/`VBAT`/`B+`）。
- 原理图与PCB中必须标清 `J1 Pin1` 方向，避免 FPC 方向装反。
- 当前电池座定义为：`BT1-1=GND`、`BT1-2=BAT`。若更改 BT1 引脚映射，电池插头线序（红/黑）也必须同步调整，避免反接。
