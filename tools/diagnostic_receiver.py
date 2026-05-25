#!/usr/bin/env python3
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import time

OUT = Path("/tmp/windows-fleet-diagnostic.txt")


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        OUT.write_bytes(body)
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b"ok\n")
        print(f"received diagnostic at {time.strftime('%H:%M:%S')} -> {OUT}")

    def log_message(self, fmt, *args):
        return


ThreadingHTTPServer(("0.0.0.0", 8766), Handler).serve_forever()
