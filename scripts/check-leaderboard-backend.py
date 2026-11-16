#!/usr/bin/env python3
"""Health-check and end-to-end /v1/scores verifier for Fruit Slicer Online Leaderboards.

Usage:
  python3 scripts/check-leaderboard-backend.py --url https://<worker>.workers.dev
  python3 scripts/check-leaderboard-backend.py --local   # spawns local TLS 1.2 server + tests full flow
"""
import argparse
import json
import os
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

MODES = [
    "classic", "arcade", "zen", "frenzy", "sudden_death", "minefield",
    "combo_rush", "daily", "beat_slice", "orchard_blast", "orchard_endless",
    "boss_rush", "recipe", "sharpshooter", "juggle",
]


def req_json(url, method="GET", body=None, ctx=None):
    data = json.dumps(body).encode("utf-8") if body is not None else None
    headers = {"User-Agent": "FruitSlicerCheck/2.0"}
    if data is not None:
        headers["Content-Type"] = "application/json"
    r = urllib.request.Request(url, data=data, headers=headers, method=method)
    with urllib.request.urlopen(r, timeout=15, context=ctx) as resp:
        return resp.status, json.loads(resp.read().decode("utf-8"))


def check_tls12(base_url, ctx=None):
    u = urllib.parse.urlparse(base_url)
    if u.scheme != "https":
        return "HTTP (local/non-TLS)"
    host = u.hostname
    port = u.port or 443
    tctx = ctx or ssl.create_default_context()
    tctx.minimum_version = ssl.TLSVersion.TLSv1_2
    tctx.maximum_version = ssl.TLSVersion.TLSv1_2
    with socket.create_connection((host, port), timeout=10) as sock:
        with tctx.wrap_socket(sock, server_hostname=host) as ss:
            return f"{ss.version()} / {ss.cipher()[0]}"


def run_checks(base_url, ctx=None):
    base = base_url.rstrip("/")
    tls_info = check_tls12(base, ctx)
    print(f"[check] TLS 1.2 handshake: {tls_info}")

    code, health = req_json(f"{base}/health", ctx=ctx)
    assert code == 200 and health.get("ok"), f"Health check failed: {health}"
    print("[check] /health OK")

    pid = "testchk_agent_01"
    try:
        # 1. Submit initial isolated test=1 score on classic, check 6-char truncation/clean, check MAX upsert
        code, res = req_json(f"{base}/v1/scores", "POST", {
            "mode": "classic", "player_id": pid, "name": "NICKBIN_LONG", "score": 120,
            "platform": "3ds", "test": 1,
        }, ctx=ctx)
        assert code == 200 and res.get("ok") and res["you"]["name"] == "NICKBI" and res["you"]["score"] == 120, res

        # Lower score + updated name -> keeps MAX(120) and updates name in-place
        code, res2 = req_json(f"{base}/v1/scores", "POST", {
            "mode": "classic", "player_id": pid, "name": "NI2", "score": 80,
            "platform": "linux", "test": 1,
        }, ctx=ctx)
        assert res2["you"]["score"] == 120 and res2["you"]["name"] == "NI2", res2

        # Higher score -> updates score to 250
        code, res3 = req_json(f"{base}/v1/scores", "POST", {
            "mode": "classic", "player_id": pid, "name": "NI2", "score": 250,
            "platform": "win", "test": 1,
        }, ctx=ctx)
        assert res3["you"]["score"] == 250 and any(r["score"] == 250 for r in res3["top"]), res3
        print("[check] POST /v1/scores MAX-score & 6-char name upsert OK")

        # 2. Check all 15 stable mode slugs respond cleanly over GET
        for m in MODES:
            c, g = req_json(f"{base}/v1/scores?mode={m}&player_id={pid}&limit=10&test=1", ctx=ctx)
            assert c == 200 and g.get("ok") and g.get("mode") == m, f"GET {m} failed: {g}"
        print(f"[check] All {len(MODES)} mode slugs GET OK")
    finally:
        # 3. Clean up isolated test entry so live leaderboards stay untouched
        for m in ("classic",):
            req_json(f"{base}/v1/scores?mode={m}&player_id={pid}&test=1", "DELETE", ctx=ctx)
        print("[check] test=1 cleanup DELETE OK")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default=os.environ.get("FRUITSLICER_LB_URL", ""))
    ap.add_argument("--local", action="store_true", help="Spin up local TLS 1.2 test server and verify")
    args = ap.parse_args()

    if args.local or not args.url:
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), "leaderboard_server"))
        import server as lb_srv

        tmp = tempfile.mkdtemp(prefix="fslb_")
        cert = os.path.join(tmp, "cert.pem")
        key = os.path.join(tmp, "key.pem")
        use_tls = shutil.which("openssl") is not None
        if use_tls:
            subprocess.run([
                "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                "-keyout", key, "-out", cert, "-days", "1", "-subj", "/CN=127.0.0.1",
            ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        srv = lb_srv.make_server("127.0.0.1", 0, cert if use_tls else None, key if use_tls else None)
        port = srv.server_address[1]
        t = threading.Thread(target=srv.serve_forever, daemon=True)
        t.start()
        ctx = ssl.create_default_context(cafile=cert) if use_tls else None
        if use_tls:
            ctx.check_hostname = False
        url = f"{'https' if use_tls else 'http'}://127.0.0.1:{port}"
        try:
            run_checks(url, ctx=ctx)
        finally:
            srv.shutdown()
            srv.server_close()
            shutil.rmtree(tmp, ignore_errors=True)
    else:
        run_checks(args.url)
    print("ALL LEADERBOARD BACKEND CHECKS PASSED")


if __name__ == "__main__":
    main()
