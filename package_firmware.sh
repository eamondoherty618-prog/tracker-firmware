#!/bin/bash
set -e

VERSION=$(grep 'TRACKER_FIRMWARE_VERSION' include/tracker_config.h | grep -o '"[^"]*"' | tr -d '"')
BIN=".pio/build/lilygo_t_sim7000g/firmware.bin"

if [ ! -f "$BIN" ]; then
  echo "firmware.bin not found — building first..."
  /Users/eamon/Library/Python/3.9/bin/pio run -e lilygo_t_sim7000g
fi

OUTDIR="tracker-flash-v${VERSION}"
rm -rf "$OUTDIR"
mkdir "$OUTDIR"

cp "$BIN" "$OUTDIR/firmware.bin"

# ── Mac / Linux flash script ───────────────────────────────────────────────────
cat > "$OUTDIR/flash_mac.sh" << 'MACEOF'
#!/bin/bash
echo ""
echo "  123 Mobile Track — Tracker Firmware Flasher"
echo "  ─────────────────────────────────────────────"
echo ""
echo "  1. Plug the tracker in via USB"
echo "  2. Press Enter when ready"
echo ""
read -r

# Install esptool silently if not present
if ! python3 -m esptool version &>/dev/null 2>&1; then
  echo "  Installing esptool..."
  pip3 install esptool --quiet
fi

# Auto-detect the USB serial port
PORT=$(ls /dev/cu.usbserial-* 2>/dev/null | head -1)
if [ -z "$PORT" ]; then
  PORT=$(ls /dev/cu.SLAB_USBtoUART* 2>/dev/null | head -1)
fi
if [ -z "$PORT" ]; then
  echo ""
  echo "  Could not auto-detect the tracker. Available ports:"
  ls /dev/cu.* 2>/dev/null || echo "  (none found)"
  echo ""
  echo "  Enter the port (e.g. /dev/cu.usbserial-12345):"
  read -r PORT
fi

echo ""
echo "  Flashing to $PORT ..."
echo ""
python3 -m esptool --port "$PORT" --chip esp32 --baud 921600 \
  write_flash 0x10000 firmware.bin

echo ""
echo "  Done! The tracker will restart and come online automatically."
echo ""
MACEOF
chmod +x "$OUTDIR/flash_mac.sh"

# ── Windows flash script ───────────────────────────────────────────────────────
cat > "$OUTDIR/flash_windows.bat" << 'WINEOF'
@echo off
echo.
echo   123 Mobile Track -- Tracker Firmware Flasher
echo   ---------------------------------------------
echo.
echo   1. Plug the tracker in via USB
echo   2. Press any key when ready
echo.
pause > nul

pip install esptool --quiet 2>nul || pip3 install esptool --quiet 2>nul

echo.
echo   Detecting port...
echo.

REM List available COM ports for the user to identify
python -m serial.tools.list_ports 2>nul

echo.
set /p PORT="  Enter the COM port (e.g. COM3): "

echo.
echo   Flashing to %PORT% ...
echo.
python -m esptool --port %PORT% --chip esp32 --baud 921600 write_flash 0x10000 firmware.bin

echo.
echo   Done! The tracker will restart and come online automatically.
echo.
pause
WINEOF

# ── README ─────────────────────────────────────────────────────────────────────
cat > "$OUTDIR/README.txt" << READMEEOF
123 Mobile Track — Tracker Firmware v${VERSION}
================================================

WHAT YOU NEED
  - The tracker (LilyGo T-SIM7000G)
  - A USB cable (USB-C or Micro-USB depending on your board)
  - Python 3 installed (https://www.python.org/downloads/)

HOW TO FLASH

  Mac / Linux:
    1. Open Terminal
    2. cd into this folder
    3. Run:  bash flash_mac.sh
    4. Follow the prompts

  Windows:
    1. Open this folder
    2. Double-click flash_windows.bat
    3. Follow the prompts
    4. When asked for the COM port, check Device Manager
       under "Ports (COM & LPT)" while the tracker is plugged in

AFTER FLASHING
  The tracker restarts automatically and will appear online
  in the 123 Mobile Track dashboard within about 60 seconds.
  Future firmware updates will happen automatically over the air —
  no USB cable needed again.

READMEEOF

# ── Zip it up ──────────────────────────────────────────────────────────────────
ZIP="tracker-flash-v${VERSION}.zip"
rm -f "$ZIP"
zip -r "$ZIP" "$OUTDIR" -x "*.DS_Store"
rm -rf "$OUTDIR"

echo ""
echo "  Packaged: $ZIP"
echo "  Send this file to your buddy — they just unzip and run the script."
echo ""
