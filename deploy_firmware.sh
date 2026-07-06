#!/bin/bash
set -e

VERSION=$(grep 'TRACKER_FIRMWARE_VERSION' include/tracker_config.h | grep -o '"[^"]*"' | tr -d '"')
BIN=".pio/build/lilygo_t_sim7000g/firmware.bin"

echo "Building firmware v$VERSION..."
/Users/eamon/Library/Python/3.9/bin/pio run -e lilygo_t_sim7000g

SIZE=$(wc -c < "$BIN" | tr -d ' ')
echo "Uploading v$VERSION ($SIZE bytes) to OTA server..."

source ~/.nvm/nvm.sh && nvm use 20 --silent

# Blob writes MUST run from the directory linked to the production site —
# run from anywhere else and the CLI prints "Success" while the write lands
# in the wrong store (v0.10.1/0.10.2 never reached the fleet this way; the
# OTA endpoint kept serving the old version). Verify against the live
# endpoint afterwards, never trust the Success line alone.
BIN_ABS="$(pwd)/$BIN"
MD5=$(md5 -q "$BIN_ABS")
pushd "$HOME/123-mobile-track/frontend" > /dev/null
npx --yes netlify blobs:set fleet-ota "firmware/v${VERSION}.bin" --input "$BIN_ABS"
npx netlify blobs:set fleet-ota "latest" "{\"version\":\"${VERSION}\",\"size\":${SIZE},\"uploaded_at\":\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\",\"md5\":\"${MD5}\"}"
popd > /dev/null

sleep 5
LIVE=$(curl -s "https://123mobiletrack.com/api/fleet/ota/check?version=0.0.0&k=c5cc56a23546eb487223fe810ae8a8b76d83376ee09140f7" | grep -o "\"version\":\"[^\"]*\"" | head -1)
echo "OTA endpoint now serves: $LIVE (expected v$VERSION)"
echo "Done — trackers will auto-update to v$VERSION on next check-in"
