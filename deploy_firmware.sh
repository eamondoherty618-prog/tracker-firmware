#!/bin/bash
set -e

VERSION=$(grep 'TRACKER_FIRMWARE_VERSION' include/tracker_config.h | grep -o '"[^"]*"' | tr -d '"')
BIN=".pio/build/lilygo_t_sim7000g/firmware.bin"

echo "Building firmware v$VERSION..."
/Users/eamon/Library/Python/3.9/bin/pio run -e lilygo_t_sim7000g

SIZE=$(wc -c < "$BIN" | tr -d ' ')
echo "Uploading v$VERSION ($SIZE bytes) to OTA server..."

source ~/.nvm/nvm.sh && nvm use 20 --silent

npx --yes netlify blobs:set fleet-ota "firmware/v${VERSION}.bin" --input "$BIN"
npx netlify blobs:set fleet-ota "latest" "{\"version\":\"${VERSION}\",\"size\":${SIZE},\"uploaded_at\":\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\"}"

echo "Done — trackers will auto-update to v$VERSION on next check-in"
