@echo off
REM flash.bat - One-shot flash script for the ESP32-C3 electronic piano.
REM
REM Requires esptool.py to be available (pip install esptool).  If you
REM prefer PlatformIO, use `pio run -t upload` instead.

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "FW=%SCRIPT_DIR%dist\firmware.bin"
set "CHIP=esp32c3"

if not exist "%FW%" (
    echo [flash] %FW% not found.  Run `pio run` first.
    exit /b 1
)

echo [flash] Available serial ports:
powershell -NoProfile -Command "Get-CimInstance Win32_SerialPort | Select-Object -ExpandProperty DeviceID | Where-Object { $_ -like 'COM*' }"

set "PORT="
set /p PORT=[flash] Enter the COM port (e.g. COM7): 
if "%PORT%"=="" (
    echo [flash] No port given, aborting.
    exit /b 2
)

echo [flash] Flashing %FW% to %PORT% ...
python -m esptool --chip %CHIP% -p %PORT% write_flash 0x0 "%FW%"
if errorlevel 1 (
    echo [flash] esptool returned an error.
    exit /b 3
)

echo [flash] Done.  Open a serial monitor at 115200 baud to see the boot log.
endlocal
