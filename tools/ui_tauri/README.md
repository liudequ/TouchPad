# Touchpad Config UI (Tauri)

这是 `tools/ui_tauri/` 下的 Tauri 版触摸板配置工具：

- USB 串口连接/断开
- 读取当前配置（`GET`）
- 应用配置（批量 `SET ...`）
- `SAVE` / `LOAD` / `RESET`
- 区域和三/四指手势绑定
- 快捷键录制（modifier + HID keycode）

## 目录结构

- `src/`：前端页面（原生 HTML/CSS/JS）
- `src-tauri/`：Rust 后端（串口桥接 + Tauri 壳）

## 本地开发

在仓库根目录执行：

```bash
cd tools/ui_tauri
npm install
npm run dev
```

说明：
- `npm run dev` 会启动 Tauri 开发窗口。
- 当前实现使用 USB 串口调参，不使用 BLE 调参。

## 打包

```bash
cd tools/ui_tauri
npm run build
```

打包产物会在 `tools/ui_tauri/src-tauri/target/release/bundle/` 下（按平台生成）。

## 平台注意事项

- macOS：建议签名 + notarization，否则用户首次运行会被 Gatekeeper 拦截。
- Linux：可能需要串口权限（例如把用户加入 `dialout` 组）。
- Windows：通常可直接安装使用。
