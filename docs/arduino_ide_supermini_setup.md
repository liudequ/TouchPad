# Arduino IDE 使用 SuperMini nRF52840 的本地板卡配置

为解决 Arduino IDE 2.3.7 在选择 `nRFMicro-like Boards` 时无法生效的问题，
改用本地 `hardware/` 目录加载板卡核心。

## 步骤
1. 将板卡核心放到仓库内的本地硬件目录：
   ```text
   /home/liudq/Work/TouchPad/hardware/pdcook/nrf52
   ```
2. 打开 Arduino IDE -> Preferences -> Sketchbook Location，设置为：
   ```text
   /home/liudq/Work/TouchPad
   ```
3. 重启 Arduino IDE。
4. 在 Tools -> Board 中选择：
   ```text
   pdcook -> SuperMini nRF52840
   ```

## 说明
- 使用本地 `hardware/` 后，IDE 不再依赖 Boards Manager 中的 `nRFMicro-like Boards` 包。
- 该方式适用于包名包含空格导致 FQBN 解析异常的情况。

## UF2 刷写流程（推荐）
1. 在 Arduino IDE 中执行“导出编译二进制文件”（Export Compiled Binary）。
2. 在仓库根目录执行：
   ```bash
   tools/make_uf2_latest_nicenano_v2.sh
   ```
3. 将生成的 `.uf2` 文件拷贝到开发板挂载出的 `NICENANO` 盘。

## 备用：手动 uf2conv
如需手动转换，可执行：

```bash
python3 hardware/pdcook/nrf52/tools/uf2conv/uf2conv.py -c -f 0xADA52840 -o your_firmware.uf2 your_firmware.hex
cp your_firmware.uf2 /media/$USER/NICENANO/
```
