fn main() {
    #[cfg(target_os = "linux")]
    {
        // Improve WebKitGTK compatibility on some Linux + NVIDIA setups.
        std::env::set_var("WEBKIT_DISABLE_DMABUF_RENDERER", "1");
    }
    touchpad_config_ui_tauri_lib::run();
}
