#!/usr/bin/env python3
"""Tiny fleet-tracker receiver for local prototyping.

This server intentionally uses only Python's standard library so it can run on a
fresh Mac. It is suitable for bench testing and early firmware bring-up, not for
production fleet data.
"""

from __future__ import annotations

import argparse
import json
import os
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, urlparse
from mimetypes import guess_type


ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "data"
TELEMETRY_LOG = DATA_DIR / "telemetry.jsonl"
PUBLIC_DIR = Path(__file__).resolve().parent / "public"
MAX_BODY_BYTES = 16 * 1024

latest_by_device: dict[str, dict[str, Any]] = {}


def load_latest_from_log() -> None:
    if not TELEMETRY_LOG.exists():
        return

    with TELEMETRY_LOG.open("r", encoding="utf-8") as handle:
        for line in handle:
            try:
                packet = json.loads(line)
            except json.JSONDecodeError:
                continue
            device_id = str(packet.get("device_id") or "unknown")
            latest_by_device[device_id] = packet


def json_response(
    handler: BaseHTTPRequestHandler,
    status: HTTPStatus,
    payload: Any,
    include_body: bool = True,
) -> None:
    body = json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json")
    handler.send_header("Content-Length", str(len(body)))
    handler.send_header("Access-Control-Allow-Origin", "*")
    handler.end_headers()
    if include_body:
        handler.wfile.write(body)


def text_response(
    handler: BaseHTTPRequestHandler,
    status: HTTPStatus,
    payload: str,
    include_body: bool = True,
) -> None:
    body = payload.encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "text/plain; charset=utf-8")
    handler.send_header("Content-Length", str(len(body)))
    handler.end_headers()
    if include_body:
        handler.wfile.write(body)


class FleetHandler(BaseHTTPRequestHandler):
    server_version = "FleetTrackerPrototype/0.1"

    def log_message(self, fmt: str, *args: Any) -> None:
        print("%s - %s" % (self.address_string(), fmt % args))

    def do_OPTIONS(self) -> None:
        self.send_response(HTTPStatus.NO_CONTENT)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.end_headers()

    def do_HEAD(self) -> None:
        self.handle_request(include_body=False)

    def do_GET(self) -> None:
        self.handle_request(include_body=True)

    def handle_request(self, include_body: bool) -> None:
        parsed = urlparse(self.path)

        if parsed.path in ("/health", "/api/fleet/health"):
            json_response(
                self,
                HTTPStatus.OK,
                {"ok": True, "devices": len(latest_by_device)},
                include_body=include_body,
            )
            return

        if parsed.path == "/api/fleet/latest":
            query = parse_qs(parsed.query)
            device_id = query.get("device_id", [""])[0]
            if device_id:
                packet = latest_by_device.get(device_id)
                if packet is None:
                    json_response(
                        self,
                        HTTPStatus.NOT_FOUND,
                        {"error": "device not found"},
                        include_body=include_body,
                    )
                    return
                json_response(self, HTTPStatus.OK, packet, include_body=include_body)
                return

            json_response(
                self,
                HTTPStatus.OK,
                {"devices": latest_by_device},
                include_body=include_body,
            )
            return

        if self.serve_static(parsed.path, include_body=include_body):
            return

        text_response(self, HTTPStatus.NOT_FOUND, "not found\n", include_body=include_body)

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path != "/api/fleet/telemetry":
            json_response(self, HTTPStatus.NOT_FOUND, {"error": "not found"})
            return

        content_length = self.headers.get("Content-Length")
        if content_length is None:
            json_response(self, HTTPStatus.LENGTH_REQUIRED, {"error": "missing Content-Length"})
            return

        try:
            body_size = int(content_length)
        except ValueError:
            json_response(self, HTTPStatus.BAD_REQUEST, {"error": "invalid Content-Length"})
            return

        if body_size > MAX_BODY_BYTES:
            json_response(self, HTTPStatus.REQUEST_ENTITY_TOO_LARGE, {"error": "body too large"})
            return

        raw_body = self.rfile.read(body_size)
        try:
            packet = json.loads(raw_body.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            json_response(self, HTTPStatus.BAD_REQUEST, {"error": "invalid JSON"})
            return

        if not isinstance(packet, dict):
            json_response(self, HTTPStatus.BAD_REQUEST, {"error": "JSON body must be an object"})
            return

        device_id = str(packet.get("device_id") or "unknown")
        packet["received_at"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        packet["remote_addr"] = self.client_address[0]

        DATA_DIR.mkdir(exist_ok=True)
        with TELEMETRY_LOG.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps(packet, separators=(",", ":"), sort_keys=True))
            handle.write("\n")

        latest_by_device[device_id] = packet
        json_response(self, HTTPStatus.ACCEPTED, {"ok": True, "device_id": device_id})

    def serve_file(self, path: Path, content_type: str, include_body: bool = True) -> None:
        try:
            body = path.read_bytes()
        except FileNotFoundError:
            text_response(self, HTTPStatus.NOT_FOUND, "not found\n", include_body=include_body)
            return

        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if include_body:
            self.wfile.write(body)

    def serve_static(self, request_path: str, include_body: bool = True) -> bool:
        raw_path = request_path or "/"
        normalized = raw_path.lstrip("/") or "index.html"

        candidates = [
            PUBLIC_DIR / normalized,
            PUBLIC_DIR / f"{normalized}.html",
        ]

        if raw_path == "/":
            candidates = [PUBLIC_DIR / "dashboard.html", PUBLIC_DIR / "index.html", *candidates]

        for candidate in candidates:
            try:
                resolved = candidate.resolve()
            except FileNotFoundError:
                continue

            if not str(resolved).startswith(str(PUBLIC_DIR.resolve())):
                continue

            if resolved.is_file():
                content_type = guess_type(str(resolved))[0] or "application/octet-stream"
                if content_type.startswith("text/"):
                    content_type = f"{content_type}; charset=utf-8"
                self.serve_file(resolved, content_type, include_body=include_body)
                return True

        return False


def main() -> None:
    parser = argparse.ArgumentParser(description="Run the fleet tracker prototype server")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", default=int(os.environ.get("PORT", "8080")), type=int)
    args = parser.parse_args()

    load_latest_from_log()
    server = ThreadingHTTPServer((args.host, args.port), FleetHandler)
    print(f"Fleet tracker server listening on http://{args.host}:{args.port}")
    print(f"Telemetry log: {TELEMETRY_LOG}")
    server.serve_forever()


if __name__ == "__main__":
    main()
