#!/usr/bin/env bash
set -euo pipefail

APP_DIR="/home/eamon/fleet-tracker"
SERVICE_PATH="/etc/systemd/system/fleet-tracker.service"
SRC_DIR="/mnt/c/ops/fleet-tracker"

mkdir -p "$APP_DIR"
rm -rf "$APP_DIR/server" "$APP_DIR/data"
mkdir -p "$APP_DIR"

cp -R "$SRC_DIR/server" "$APP_DIR/"
mkdir -p "$APP_DIR/data"
if [ -f "$SRC_DIR/data/telemetry.jsonl" ]; then
  cp "$SRC_DIR/data/telemetry.jsonl" "$APP_DIR/data/telemetry.jsonl"
fi

chown -R eamon:eamon "$APP_DIR"

cat > "$SERVICE_PATH" <<'EOF'
[Unit]
Description=Fleet Tracker Receiver
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=eamon
WorkingDirectory=/home/eamon/fleet-tracker
ExecStart=/usr/bin/python3 /home/eamon/fleet-tracker/server/fleet_server.py --host 0.0.0.0 --port 8081
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable --now fleet-tracker.service
systemctl restart fleet-tracker.service
systemctl --no-pager --full status fleet-tracker.service | sed -n '1,40p'
curl -sS http://127.0.0.1:8081/api/fleet/health
