#!/usr/bin/env python3
"""Local/self-hosted reference /v1/scores server matching worker.js identically."""
import argparse
import json
import re
import ssl
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

SLUG_RE = re.compile(r"^[a-z0-9_]{1,24}$")
STORE = {}
LOCK = threading.Lock()


def clean_name(raw: str) -> str:
    s = re.sub(r"[^A-Za-z0-9 _-]", "", str(raw or "")).strip()[:6]
    return s or "PLAYER"


def sort_rows(rows):
    rows.sort(key=lambda r: (-r["score"], r.get("updated", 0)))
    return rows


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def _send(self, code, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        p = urlparse(self.path)
        if p.path == "/health":
            return self._send(200, {"ok": True, "service": "fruit-slicer-lb", "version": 1})
        if p.path != "/v1/scores":
            return self._send(404, {"ok": False, "error": "not_found"})
        qs = parse_qs(p.query)
        mode = qs.get("mode", [""])[0]
        player_id = qs.get("player_id", [""])[0][:32]
        limit = min(25, max(1, int(qs.get("limit", ["10"])[0] or 10)))
        is_test = qs.get("test", ["0"])[0] == "1"
        if not SLUG_RE.match(mode):
            return self._send(400, {"ok": False, "error": "bad_mode"})
        key = ("test:" if is_test else "live:") + mode
        with LOCK:
            rows = list(sort_rows(STORE.get(key, [])))
        idx = next((i for i, r in enumerate(rows) if player_id and r["player_id"] == player_id), -1)
        return self._send(200, {
            "ok": True,
            "mode": mode,
            "top": rows[:limit],
            "you": {"rank": idx + 1, "name": rows[idx]["name"], "score": rows[idx]["score"]} if idx >= 0 else None,
            "total": len(rows),
        })

    def do_POST(self):
        if urlparse(self.path).path != "/v1/scores":
            return self._send(404, {"ok": False, "error": "not_found"})
        length = int(self.headers.get("Content-Length", "0") or 0)
        try:
            body = json.loads(self.rfile.read(length).decode("utf-8"))
        except Exception:
            return self._send(400, {"ok": False, "error": "bad_json"})
        mode = str(body.get("mode", ""))
        player_id = re.sub(r"[^A-Za-z0-9_-]", "", str(body.get("player_id", "")))[:32]
        score = max(0, int(body.get("score", 0) or 0))
        platform = str(body.get("platform", "unknown"))[:12]
        is_test = body.get("test") in (1, True, "1")
        if not SLUG_RE.match(mode) or not player_id:
            return self._send(400, {"ok": False, "error": "bad_params"})
        name = clean_name(body.get("name", ""))
        now = int(time.time())
        key = ("test:" if is_test else "live:") + mode
        with LOCK:
            rows = STORE.setdefault(key, [])
            ex = next((r for r in rows if r["player_id"] == player_id), None)
            if ex:
                ex["score"] = max(ex["score"], score)
                ex["name"] = name
                ex["platform"] = platform
                ex["updated"] = now
            else:
                rows.append({"player_id": player_id, "name": name, "score": score, "platform": platform, "updated": now})
            sort_rows(rows)
            idx = next((i for i, r in enumerate(rows) if r["player_id"] == player_id), -1)
            snap = list(rows)
        return self._send(200, {
            "ok": True,
            "mode": mode,
            "top": snap[:10],
            "you": {"rank": idx + 1, "name": snap[idx]["name"], "score": snap[idx]["score"]} if idx >= 0 else None,
            "total": len(snap),
        })

    def do_DELETE(self):
        p = urlparse(self.path)
        if p.path != "/v1/scores":
            return self._send(404, {"ok": False, "error": "not_found"})
        qs = parse_qs(p.query)
        mode = qs.get("mode", [""])[0]
        player_id = qs.get("player_id", [""])[0]
        is_test = qs.get("test", ["0"])[0] == "1"
        if not is_test or not SLUG_RE.match(mode) or not player_id:
            return self._send(403, {"ok": False, "error": "forbidden"})
        key = "test:" + mode
        with LOCK:
            STORE[key] = [r for r in STORE.get(key, []) if r["player_id"] != player_id]
        return self._send(200, {"ok": True})


def make_server(host="127.0.0.1", port=8787, cert=None, key=None):
    srv = ThreadingHTTPServer((host, port), Handler)
    if cert and key:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.minimum_version = ssl.TLSVersion.TLSv1_2
        ctx.load_cert_chain(certfile=cert, keyfile=key)
        srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
    return srv


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8787)
    ap.add_argument("--cert")
    ap.add_argument("--key")
    args = ap.parse_args()
    srv = make_server(args.host, args.port, args.cert, args.key)
    print(f"Listening on {'https' if args.cert else 'http'}://{args.host}:{args.port}")
    srv.serve_forever()
