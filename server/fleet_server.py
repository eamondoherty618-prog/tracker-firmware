#!/usr/bin/env python3
"""Tiny fleet-tracker receiver for local prototyping.

This server intentionally uses only Python's standard library so it can run on a
fresh Mac. It is suitable for bench testing and early firmware bring-up, not for
production fleet data.
"""

from __future__ import annotations

import argparse
import json
import math
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

TRIP_MIN_SPEED_KPH = 4.0
TRIP_END_GAP_S = 300  # 5 minutes stationary ends a trip
TRIP_MIN_POINTS = 3


def _haversine_km(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    r = 6371.0
    d_lat = math.radians(lat2 - lat1)
    d_lon = math.radians(lon2 - lon1)
    a = math.sin(d_lat / 2) ** 2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(d_lon / 2) ** 2
    return r * 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))


def _packet_time(packet: dict[str, Any]) -> float:
    ts = packet.get("received_at") or packet.get("gps", {}).get("timestamp") or ""
    try:
        import datetime
        return datetime.datetime.fromisoformat(ts.replace("Z", "+00:00")).timestamp()
    except Exception:
        return 0.0


def segment_trips(device_id: str) -> list[dict[str, Any]]:
    if not TELEMETRY_LOG.exists():
        return []

    packets: list[dict[str, Any]] = []
    with TELEMETRY_LOG.open("r", encoding="utf-8") as fh:
        for line in fh:
            try:
                p = json.loads(line)
            except json.JSONDecodeError:
                continue
            if p.get("device_id") != device_id:
                continue
            if not p.get("has_fix"):
                continue
            gps = p.get("gps") or {}
            if not gps.get("lat") or not gps.get("lon"):
                continue
            packets.append(p)

    packets.sort(key=_packet_time)

    trips: list[dict[str, Any]] = []
    current: list[dict[str, Any]] = []

    for pkt in packets:
        speed = float((pkt.get("gps") or {}).get("speed_kph") or 0)
        t = _packet_time(pkt)

        if not current:
            if speed >= TRIP_MIN_SPEED_KPH:
                current = [pkt]
            continue

        last_t = _packet_time(current[-1])
        gap = t - last_t

        if gap > TRIP_END_GAP_S and speed < TRIP_MIN_SPEED_KPH:
            if len(current) >= TRIP_MIN_POINTS:
                trips.append(_build_trip(device_id, current))
            current = []
        else:
            current.append(pkt)

    if len(current) >= TRIP_MIN_POINTS:
        trips.append(_build_trip(device_id, current))

    trips.sort(key=lambda t: t["start_time"], reverse=True)
    return trips


def _build_trip(device_id: str, packets: list[dict[str, Any]]) -> dict[str, Any]:
    start_t = _packet_time(packets[0])
    end_t = _packet_time(packets[-1])
    duration_s = max(1, int(end_t - start_t))

    speeds = [float((p.get("gps") or {}).get("speed_kph") or 0) for p in packets]
    lats = [float((p.get("gps") or {}).get("lat") or 0) for p in packets]
    lons = [float((p.get("gps") or {}).get("lon") or 0) for p in packets]

    distance_km = sum(
        _haversine_km(lats[i], lons[i], lats[i + 1], lons[i + 1])
        for i in range(len(lats) - 1)
    )

    events: list[dict[str, Any]] = []
    for p in packets:
        ev = p.get("event")
        if ev:
            gps = p.get("gps") or {}
            events.append({
                "type": ev,
                "time": p.get("received_at", ""),
                "lat": float(gps.get("lat") or 0),
                "lon": float(gps.get("lon") or 0),
                "speed_kph": float(gps.get("speed_kph") or 0),
            })

    trip_id = f"{device_id}-{int(start_t)}"

    import datetime
    def ts(epoch: float) -> str:
        return datetime.datetime.utcfromtimestamp(epoch).strftime("%Y-%m-%dT%H:%M:%SZ")

    return {
        "id": trip_id,
        "device_id": device_id,
        "start_time": ts(start_t),
        "end_time": ts(end_t),
        "duration_s": duration_s,
        "distance_km": round(distance_km, 3),
        "max_speed_kph": round(max(speeds), 1) if speeds else 0,
        "avg_speed_kph": round(sum(speeds) / len(speeds), 1) if speeds else 0,
        "point_count": len(packets),
        "event_count": len(events),
        "events": events,
        "start_lat": lats[0],
        "start_lon": lons[0],
        "end_lat": lats[-1],
        "end_lon": lons[-1],
        "_packets": packets,
    }


def get_trip_detail(device_id: str, trip_id: str) -> dict[str, Any] | None:
    for trip in segment_trips(device_id):
        if trip["id"] == trip_id:
            route = [
                {
                    "lat": float((p.get("gps") or {}).get("lat") or 0),
                    "lon": float((p.get("gps") or {}).get("lon") or 0),
                    "speed_kph": float((p.get("gps") or {}).get("speed_kph") or 0),
                    "time": p.get("received_at", ""),
                    "motion_state": p.get("motion_state", ""),
                }
                for p in trip["_packets"]
            ]
            summary = {k: v for k, v in trip.items() if k != "_packets"}
            return {"trip": summary, "route": route}
    return None


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

        if parsed.path == "/api/fleet/telemetry":
            query = parse_qs(parsed.query)
            device_id = query.get("device_id", [""])[0]
            if device_id:
                packet = latest_by_device.get(device_id)
                if packet is None:
                    json_response(
                        self,
                        HTTPStatus.NOT_FOUND,
                        {"ok": False, "error": "device not found"},
                        include_body=include_body,
                    )
                    return
                json_response(self, HTTPStatus.OK, {"ok": True, "device": packet}, include_body=include_body)
                return
            # No device_id — return all devices that look like real trackers.
            trackers = {k: v for k, v in latest_by_device.items() if k.startswith("tracker-")}
            json_response(self, HTTPStatus.OK, {"ok": True, "devices": trackers}, include_body=include_body)
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

        if parsed.path == "/api/fleet/history":
            query = parse_qs(parsed.query)
            device_id = query.get("device_id", [""])[0]
            only_fix = query.get("has_fix", [""])[0] == "true"
            try:
                limit = min(int(query.get("limit", ["200"])[0]), 2000)
            except ValueError:
                limit = 200

            results: list[dict[str, Any]] = []
            if TELEMETRY_LOG.exists():
                with TELEMETRY_LOG.open("r", encoding="utf-8") as handle:
                    for line in handle:
                        try:
                            packet = json.loads(line)
                        except json.JSONDecodeError:
                            continue
                        if device_id and packet.get("device_id") != device_id:
                            continue
                        if only_fix and not packet.get("has_fix"):
                            continue
                        results.append(packet)
            json_response(
                self,
                HTTPStatus.OK,
                {"ok": True, "count": len(results), "packets": results[-limit:]},
                include_body=include_body,
            )
            return

        if parsed.path == "/api/fleet/trips":
            query = parse_qs(parsed.query)
            device_id = query.get("device_id", ["tracker-001"])[0]
            trips = segment_trips(device_id)
            # Strip internal _packets before sending
            clean = [{k: v for k, v in t.items() if k != "_packets"} for t in trips]
            json_response(self, HTTPStatus.OK, {"ok": True, "trips": clean}, include_body=include_body)
            return

        if parsed.path.startswith("/api/fleet/trips/"):
            trip_id = parsed.path.split("/api/fleet/trips/", 1)[1]
            query = parse_qs(parsed.query)
            device_id = query.get("device_id", ["tracker-001"])[0]
            detail = get_trip_detail(device_id, trip_id)
            if detail is None:
                json_response(self, HTTPStatus.NOT_FOUND, {"ok": False, "error": "trip not found"}, include_body=include_body)
                return
            json_response(self, HTTPStatus.OK, {"ok": True, **detail}, include_body=include_body)
            return

        if parsed.path == "/api/fleet/alerts":
            query = parse_qs(parsed.query)
            device_id = query.get("device_id", ["tracker-001"])[0]
            alerts: list[dict[str, Any]] = []
            if TELEMETRY_LOG.exists():
                with TELEMETRY_LOG.open("r", encoding="utf-8") as fh:
                    for line in fh:
                        try:
                            p = json.loads(line)
                        except json.JSONDecodeError:
                            continue
                        if p.get("device_id") != device_id:
                            continue
                        ev = p.get("event")
                        if not ev:
                            continue
                        gps = p.get("gps") or {}
                        alerts.append({
                            "id": f"{device_id}-{p.get('received_at','')}",
                            "device_id": device_id,
                            "type": ev,
                            "title": "Hard brake" if ev == "hard_brake" else "Rapid acceleration",
                            "severity": "warning",
                            "time": p.get("received_at", ""),
                            "speed_kph": float(gps.get("speed_kph") or 0),
                            "lat": float(gps.get("lat") or 0) or None,
                            "lon": float(gps.get("lon") or 0) or None,
                        })
            alerts.sort(key=lambda a: a["time"], reverse=True)
            json_response(self, HTTPStatus.OK, {"ok": True, "alerts": alerts}, include_body=include_body)
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
