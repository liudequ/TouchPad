# Windows UI 打包与发布

本文档用于把 `tools/ui/touchpad_config_ui.py` 打包成适合普通 Windows 用户使用的图形工具。

## 目标

- 用户不需要安装 Python。
- 用户双击安装包即可完成安装。
- 安装后可从开始菜单或桌面启动 UI。

## 当前方案

推荐采用两步发布：

1. 使用 `PyInstaller` 生成 `exe` 发布目录
2. 使用 `Inno Setup` 生成安装包

这样比单文件 `onefile` 更稳定，启动更快，用户遇到问题时也更容易排查。

## 构建环境

建议在 Windows 10 或 Windows 11 上执行以下操作：

- 安装 Python 3
- 安装 Inno Setup
- 使用仓库根目录作为工作目录

## 生成 exe

仓库已提供脚本：

- [build_windows_exe.bat](/home/liudq/Cloud/TouchPad/tools/ui/build_windows_exe.bat)

在 Windows 命令行执行：

```bat
tools\ui\build_windows_exe.bat
```

脚本会自动：

- 创建 `.venv`
- 安装 `tools/ui/requirements.txt`
- 安装 `pyinstaller`
- 生成 `dist\TouchpadConfigUI\TouchpadConfigUI.exe`

## 生成安装包

仓库已提供 Inno Setup 脚本模板：

- [windows_installer.iss](/home/liudq/Cloud/TouchPad/tools/ui/windows_installer.iss)

使用方式：

1. 先执行 `tools\ui\build_windows_exe.bat`
2. 用 Inno Setup 打开 `tools\ui\windows_installer.iss`
3. 点击 Compile

生成的安装包默认输出到：

```text
dist\installer\TouchpadConfigUI-Setup.exe
```

## 发布给普通用户

建议把以下内容一并提供给用户：

- `TouchpadConfigUI-Setup.exe`
- 简短使用说明

说明里至少写清楚：

1. 先把触摸板通过 USB 接到电脑
2. 打开软件后选择对应 `COM` 端口
3. 点击“连接”
4. 如未出现串口，重新插拔设备后再点“刷新”

## 驱动与权限说明

Windows 下通常不需要像 Linux 那样处理 `dialout` 权限。

如果用户看不到串口，优先检查：

1. 设备管理器里是否出现新的串口设备
2. 数据线是否支持数据传输
3. 开发板当前固件是否正常枚举为串口
4. 是否缺少开发板对应 USB 串口驱动

## 体积预期

由于 GUI 使用 `PySide6`，打包后的目录通常不会太小，这是正常现象。

经验上：

- `onedir` 发布目录通常会明显大于源码
- 但它比 `onefile` 更适合分发给普通用户

## 备注

- 当前 UI 实际使用的是 USB 串口配置。
- 仓库里虽保留 `bleak` 和 BLE 相关代码，但当前 nRF 固件已移除 BLE 调参入口。
