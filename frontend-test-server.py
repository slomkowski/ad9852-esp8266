#!/usr/bin/env python3
"""Dev server for testing data/index.html against emulated firmware endpoints."""

import sys
from http.server import HTTPServer, BaseHTTPRequestHandler
from pathlib import Path
from urllib.parse import urlparse, parse_qs

PORT = 8378
HTML = Path(__file__).parent / "data" / "index.html"

freq = 100_000.0
mult = 5


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        p = urlparse(self.path)

        if p.path == "/":
            body = HTML.read_bytes()
            self._respond(200, "text/html; charset=utf-8", body)

        elif p.path == "/api/freq":
            self._respond(200, "text/plain", f"{freq:.0f}".encode())

        elif p.path == "/api/multiplier":
            self._respond(200, "text/plain", str(mult).encode())

        else:
            self.send_error(404)

    def do_POST(self):
        global freq, mult
        p = urlparse(self.path)
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length).decode()
        qs = parse_qs(body)

        if p.path == "/api/freq":
            if "freq" in qs:
                v = float(qs["freq"][0])
                if v > 0:
                    freq = v
                    print(f"  freq → {freq:.0f} Hz")
            self._respond(200, "text/plain", b"ok")

        elif p.path == "/api/multiplier":
            if "mult" in qs:
                v = int(qs["mult"][0])
                if v < 4:
                    mult = 4
                elif v > 15:
                    mult = 15
                else:
                    mult = v
                print(f"  mult → ×{mult}  (SYSCLK = {20 * mult} MHz)")
            self._respond(200, "text/plain", b"ok")

        else:
            self.send_error(404)

    def _respond(self, code, content_type, body: bytes):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        print(f"  {self.command} {self.path}  →  {args[1]}")


if __name__ == "__main__":
    if not HTML.exists():
        sys.exit(f"error: {HTML} not found")
    print(f"Serving {HTML}  on  http://localhost:{PORT}")
    HTTPServer(("", PORT), Handler).serve_forever()