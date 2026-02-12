# 嘉立创EDA专业版导入（nice!nano v2 2x13）

已生成可导入压缩包：

- `exports/easyeda_pro_nice_nano_v2_import.zip`

包内内容：

- `easyeda_pro_nice_nano_v2/nice_nano_v2.kicad_sym`
- `easyeda_pro_nice_nano_v2/nice_nano_v2.pretty/NICE_NANO_V2_2X13_P2.54MM.kicad_mod`

## 在嘉立创EDA专业版导入

1. 打开专业版工程。  
2. 进入“库管理/我的库”里的“导入”功能。  
3. 导入符号：选择 `nice_nano_v2.kicad_sym`。  
4. 导入封装：选择 `NICE_NANO_V2_2X13_P2.54MM.kicad_mod`（也可直接选整个 `nice_nano_v2.pretty` 目录）。  
5. 在原理图放置符号后，检查封装关联为 `NICE_NANO_V2_2X13_P2.54MM`。  
6. 同步到PCB后确认：
   - USB 朝向（顶部）
   - Pin1 方向（左上角）
   - 实物孔距一致

## 说明

- 这是按侧边排针 `2x13` 建的模块封装。  
- `D18/D19/D20`（底部额外焊盘）不在这个 `2x13` 封装里。  
- 建议首板前先 1:1 打印对孔，避免镜像错误。
