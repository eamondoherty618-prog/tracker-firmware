#!/usr/bin/env bash
set -euo pipefail

SRC_ROOT="/mnt/c/ops/fleet-tracker"
DEST_ROOT="/home/eamon/fleet-tracker"

mkdir -p "$DEST_ROOT"

rm -rf "$DEST_ROOT/server"
cp -R "$SRC_ROOT/server" "$DEST_ROOT/server"

mkdir -p "$DEST_ROOT/data"
if [ ! -f "$DEST_ROOT/data/telemetry.jsonl" ] && [ -f "$SRC_ROOT/data/telemetry.jsonl" ]; then
  cp "$SRC_ROOT/data/telemetry.jsonl" "$DEST_ROOT/data/telemetry.jsonl"
fi

chown -R eamon:eamon "$DEST_ROOT"
