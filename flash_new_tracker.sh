#!/usr/bin/env bash
# Flash a brand-new LilyGo T-SIM7000G with the generic factory image and print
# the identity it will adopt (tracker-<last 6 hex of its MAC>). One command per
# board — no per-device compiling, no editing configs.
#
# Usage: ./flash_new_tracker.sh [/dev/cu.usbserialXXXX]
# (port auto-detected when only one board is plugged in)
set -euo pipefail
cd "$(dirname "$0")"

PIO=/Users/eamon/Library/Python/3.9/bin/pio

PORT="${1:-}"
if [ -z "$PORT" ]; then
  PORT=$(ls /dev/cu.usbserial* 2>/dev/null | head -1 || true)
fi
if [ -z "$PORT" ]; then
  echo "No USB serial device found — plug the board in (install the CP210x driver if it never shows up)."
  exit 1
fi

echo "==> Flashing factory image to $PORT"
LOG=$(mktemp)
"$PIO" run -e factory -t upload --upload-port "$PORT" 2>&1 | tee "$LOG"

# esptool prints the efuse MAC while flashing; the firmware names itself from
# its last three octets, so the ID can be printed without opening a monitor.
MAC=$(grep -oE '([0-9a-f]{2}:){5}[0-9a-f]{2}' "$LOG" | head -1 || true)
rm -f "$LOG"
if [ -n "$MAC" ]; then
  HEX=$(echo "$MAC" | tr -d ':')
  ID="tracker-${HEX: -6}"
  VER=$(grep -oE '#define TRACKER_FIRMWARE_VERSION "[^"]+"' include/tracker_config.h | cut -d'"' -f2)
  echo "$(date +%F),$MAC,$ID,v$VER" >> fleet_inventory.csv
  echo ""
  echo "=============================================================="
  echo "  This unit is:  $ID          (logged to fleet_inventory.csv)"
  echo ""
  echo "  Next: insert SIM, power it in the vehicle, then in the app:"
  echo "  Devices -> Add Device -> claim '$ID' -> assign its vehicle."
  echo "=============================================================="
else
  echo "!! Could not read the MAC from esptool output. Open a serial monitor —"
  echo "   the board prints 'Self-provisioned from efuse MAC as: tracker-...' at boot."
fi
