#!/usr/bin/env bash
# Build the factory image and copy it (plus an MD5 sidecar) onto a microSD card,
# so a tracker can be re-flashed in the field with no laptop: insert the card,
# power-cycle, the firmware self-flashes and reboots. Works on units already
# running >= v0.10.5; identity (NVS) survives, so a fleet unit stays itself.
# Card must be FAT32.
#
# Usage: ./make_sd_card.sh [/Volumes/YOURCARD]
set -euo pipefail
cd "$(dirname "$0")"

PIO=/Users/eamon/Library/Python/3.9/bin/pio
"$PIO" run -e factory

BIN=.pio/build/factory/firmware.bin
MD5=$(md5 -q "$BIN")
VER=$(grep -oE '#define TRACKER_FIRMWARE_VERSION "[^"]+"' include/tracker_config.h | cut -d'"' -f2)

DEST="${1:-}"
if [ -n "$DEST" ]; then
  if [ ! -d "$DEST" ]; then
    echo "Card path $DEST not found — is it mounted?"
    exit 1
  fi
  cp "$BIN" "$DEST/firmware.bin"
  echo "$MD5" > "$DEST/firmware.md5"
  sync
  echo "==> v$VER written to $DEST. Eject the card, insert into the tracker, power-cycle."
else
  echo "==> Built v$VER: $BIN (md5 $MD5)"
  echo "    Re-run with the mounted card path to copy it on: ./make_sd_card.sh /Volumes/YOURCARD"
fi
