# 触摸板底板（nice!nano 直焊）嘉立创EDA完整操作流程

适用范围：
- 半一体化底板
- `nice!nano v2` 直焊
- FPC `6P 1.0mm`
- 电池座 `2Pin`
- 复位微动开关 `3x6x4.3`

相关输入文件：
- `manufacturing/pcba/netlist_nicenano_baseboard.md`
- `manufacturing/pcba/pcba_minimal_height_nicenano.md`
- `manufacturing/pcba/component_selection.md`

---

## 1. 开始前准备（10分钟）
1. 确认网络连接表（Netlist）不再改动。
2. 确认 LCSC 编号：
   - FPC：`C51901510`
   - 电池座：`C53055318`
   - 复位开关：`C231711`
3. 准备好 `nice!nano v2` 实物，拍清晰正反面引脚图（用于核对 BAT/RST/3V3/D6/D7/D8/D9）。
4. 明确外壳约束：USB 方向、限高、板子大概长宽。

---

## 2. 新建EDA工程
1. 打开嘉立创EDA，创建新工程：`touchpad_baseboard`。
2. 先画原理图，不要直接画PCB。

---

## 3. 画原理图（按 Netlist 抄，不自行扩展）

### 3.1 放置器件
1. 放置 `U1`：nice!nano 对应符号（若无现成符号，先用自建连接器符号代替双排引脚）。
2. 放置 `J1`：FPC 6P 1.0mm（对应 `C51901510` 的符号/封装）。
3. 放置 `BT1`：电池座 2Pin（对应 `C53055318`）。
4. 放置 `SW1`：轻触开关 2Pin（对应 `C231711`）。

### 3.2 连线（必须与 netlist 一致）
1. `J1.1 -> GND -> U1.GND`
2. `J1.2 -> TP_EN -> U1.D9`
3. `J1.3 -> TP_INT -> U1.D8`
4. `J1.4 -> I2C_SDA -> U1.D7`
5. `J1.5 -> I2C_SCL -> U1.D6`
6. `J1.6 -> TP_3V3 -> U1.3V3`
7. `SW1.1 -> RST_N -> U1.RST`
8. `SW1.2 -> GND`
9. `BT1.1 -> GND`
10. `BT1.2 -> BAT_POS -> U1.BAT`

### 3.3 标注和检查
1. 添加网络标签：`GND/TP_3V3/I2C_SDA/I2C_SCL/TP_INT/TP_EN/RST_N/BAT_POS/BAT_NEG`。
2. 在 `J1` 旁加文字：`Pin1=GND ... Pin6=3V3`。
3. 运行 ERC，消除错误（警告可先记录）。

---

## 4. 转PCB并布局

### 4.1 PCB基础设置
1. 板厚目标：`0.8mm`（在下单参数里设置）。
2. 层数：2层。
3. 线宽建议起步：`0.2mm`；电源线可 `0.3~0.5mm`。
4. 地线尽量连续，最后铺铜 `GND`。

### 4.2 摆位顺序（先机械后电气）
1. 先放 `J1(FPC)`：按排线插入方向放在板边，确保可插拔。
2. 放 `BT1`：靠近外壳开口侧，预留插拔空间。
3. 放 `SW1`：与外壳按键孔对齐。
4. 最后放 `U1(nice!nano)`：保证 USB 方向与外壳开孔一致。

### 4.3 布线优先级
1. 先布 `I2C_SDA/I2C_SCL/TP_INT/TP_EN`。
2. 再布 `3V3/GND/BAT`。
3. 最后铺地铜并复查回流路径。

---

## 5. 必做检查（下单前）
1. `FPC方向`：上接/下接、同向/反向是否与触摸板排线一致。
2. `Pin1标识`：J1 丝印必须可见。
3. `电池极性`：BT1 `+/-` 丝印明确。
   - 当前定义：`BT1-1=GND`、`BT1-2=BAT`；若 BT1 引脚映射改动，电池插头线序（红/黑）必须同步调整。
4. `U1引脚`：`BAT/RST/3V3/D6/D7/D8/D9` 与实物丝印一致。
5. `机械干涉`：FPC锁扣、电池插头、USB 不顶壳。
6. 运行 DRC，无错误再导出。

---

## 6. 导出生产文件
1. 导出 `Gerber`。
2. 导出钻孔文件（Drill）。
3. 导出 `BOM`。
4. 导出 `CPL/坐标文件`。
5. 打包归档：
   - `touchpad_baseboard_gerber.zip`
   - `touchpad_baseboard_bom.csv`
   - `touchpad_baseboard_cpl.csv`

---

## 7. 下单建议（先小批）
1. 先做 `5~10` 套 EVT，不直接 50 套。
2. 首版建议先只让厂家贴 `J1/BT1/SW1`。
3. `nice!nano` 可先人工直焊验证，再决定是否交由工厂工艺处理。

---

## 8. 首板回板验收清单
1. 上电无短路（3V3-GND）。
2. I2C 能识别触摸板地址 `0x2C`。
3. 单指移动、双指滚动、点击正常。
4. 复位键功能正常。
5. 电池供电与充电路径正常（若装电池）。

---

## 9. 常见错误速查
1. 触摸板无响应：FPC 方向错（最常见）。
2. 识别不稳定：SDA/SCL 交叉或虚焊。
3. 无法复位：SW1 接错到非 RST。
4. 电池不工作：BAT 极性反接或接到错误引脚。
