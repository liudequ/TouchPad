@echo off
setlocal

cd /d "%~dp0\..\.."

if not exist ".venv" (
  echo [build] Missing .venv, creating virtual environment...
  py -3 -m venv .venv
  if errorlevel 1 goto :error
)

call ".venv\Scripts\activate.bat"
if errorlevel 1 goto :error

echo [build] Installing runtime dependencies...
python -m pip install -r "tools\ui\requirements.txt"
if errorlevel 1 goto :error

echo [build] Installing build dependencies...
python -m pip install pyinstaller
if errorlevel 1 goto :error

if exist "build\TouchpadConfigUI" rmdir /s /q "build\TouchpadConfigUI"
if exist "dist\TouchpadConfigUI" rmdir /s /q "dist\TouchpadConfigUI"

echo [build] Building Windows executable...
pyinstaller ^
  --noconfirm ^
  --clean ^
  --windowed ^
  --name TouchpadConfigUI ^
  --collect-all PySide6 ^
  --hidden-import serial.tools.list_ports_windows ^
  --add-data "tools\ui;tools\ui" ^
  "tools\ui\touchpad_config_ui.py"
if errorlevel 1 goto :error

echo [build] Done. Output: dist\TouchpadConfigUI\TouchpadConfigUI.exe
exit /b 0

:error
echo [build] Failed.
exit /b 1
