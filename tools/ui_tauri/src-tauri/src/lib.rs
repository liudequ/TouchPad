use anyhow::{anyhow, Result};
use once_cell::sync::Lazy;
use serde::Serialize;
use serialport::SerialPort;
use std::collections::HashMap;
use std::io::{Read, Write};
use std::sync::Mutex;
use std::time::{Duration, Instant};

static SERIAL: Lazy<Mutex<Option<Box<dyn SerialPort>>>> = Lazy::new(|| Mutex::new(None));

#[derive(Serialize)]
struct ConfigData {
    values: HashMap<String, String>,
    raw_lines: Vec<String>,
}

#[tauri::command]
fn list_ports() -> Result<Vec<String>, String> {
    let ports = serialport::available_ports().map_err(|e| e.to_string())?;
    Ok(ports.into_iter().map(|p| p.port_name).collect())
}

#[tauri::command]
fn connect_serial(port: String, baud: Option<u32>) -> Result<(), String> {
    let baud_rate = baud.unwrap_or(115200);
    let serial = serialport::new(port, baud_rate)
        .timeout(Duration::from_millis(300))
        .open()
        .map_err(|e| e.to_string())?;
    let mut guard = SERIAL.lock().map_err(|_| "串口锁失败".to_string())?;
    *guard = Some(serial);
    Ok(())
}

#[tauri::command]
fn disconnect_serial() -> Result<(), String> {
    let mut guard = SERIAL.lock().map_err(|_| "串口锁失败".to_string())?;
    *guard = None;
    Ok(())
}

#[tauri::command]
fn is_connected() -> Result<bool, String> {
    let guard = SERIAL.lock().map_err(|_| "串口锁失败".to_string())?;
    Ok(guard.is_some())
}

fn send_inner(command: &str) -> Result<Vec<String>> {
    let mut guard = SERIAL.lock().map_err(|_| anyhow!("串口锁失败"))?;
    let serial = guard.as_mut().ok_or_else(|| anyhow!("未连接串口"))?;

    let cmd = format!("{}\n", command.trim());
    serial.write_all(cmd.as_bytes())?;
    serial.flush()?;
    std::thread::sleep(Duration::from_millis(50));

    let start = Instant::now();
    let mut lines = Vec::new();
    let mut buf = [0u8; 512];
    let mut acc = String::new();

    while start.elapsed() < Duration::from_millis(500) {
        match serial.read(&mut buf) {
            Ok(n) if n > 0 => {
                let s = String::from_utf8_lossy(&buf[..n]);
                acc.push_str(&s);
                while let Some(pos) = acc.find('\n') {
                    let line = acc[..pos].trim().to_string();
                    acc = acc[pos + 1..].to_string();
                    if !line.is_empty() {
                        lines.push(line);
                    }
                }
            }
            Ok(_) => break,
            Err(e) if e.kind() == std::io::ErrorKind::TimedOut => break,
            Err(e) => return Err(anyhow!(e)),
        }
    }

    if !acc.trim().is_empty() {
        lines.push(acc.trim().to_string());
    }

    Ok(lines)
}

#[tauri::command]
fn send_command(command: String) -> Result<Vec<String>, String> {
    send_inner(&command).map_err(|e| e.to_string())
}

#[tauri::command]
fn get_all() -> Result<ConfigData, String> {
    let lines = send_inner("GET").map_err(|e| e.to_string())?;
    let mut values = HashMap::new();
    for line in &lines {
        if let Some((k, v)) = line.split_once('=') {
            values.insert(k.trim().to_string(), v.trim().to_string());
        }
    }
    Ok(ConfigData {
        values,
        raw_lines: lines,
    })
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            list_ports,
            connect_serial,
            disconnect_serial,
            is_connected,
            send_command,
            get_all
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
