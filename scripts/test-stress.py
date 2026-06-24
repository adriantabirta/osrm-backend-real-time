#!/usr/bin/env python3
"""
Stress test: random UDP traffic (up to 100 pkt/s) + concurrent OSRM route/traffic HTTP requests.

Requires a running osrm-routed with --enable-live-data and valid map data.

Environment variables:
  OSRM_URL          default http://127.0.0.1:7070
  UDP_HOST          default 127.0.0.1
  UDP_PORT          default 9900
  DURATION_SEC      default 30
  MAX_UDP_RATE      default 100 (packets per second, random intervals)
  ROUTE_WORKERS     default 8 concurrent HTTP workers
  ROUTE_COORDS      default 28.8608,47.0105;28.8750,47.0200 (Moldova / Chisinau)
"""

from __future__ import annotations

import json
import os
import random
import socket
import struct
import sys
import threading
import time
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from typing import List, Tuple


def env_int(name: str, default: int) -> int:
    return int(os.environ.get(name, default))


def env_str(name: str, default: str) -> str:
    return os.environ.get(name, default)


@dataclass
class StressStats:
    udp_sent: int = 0
    udp_errors: int = 0
    route_ok: int = 0
    route_errors: int = 0
    traffic_ok: int = 0
    traffic_errors: int = 0
    route_latencies_ms: List[float] = field(default_factory=list)
    lock: threading.Lock = field(default_factory=threading.Lock)

    def add_route_latency(self, ms: float) -> None:
        with self.lock:
            self.route_latencies_ms.append(ms)

    def summary(self) -> str:
        with self.lock:
            lat = self.route_latencies_ms
        p50 = sorted(lat)[len(lat) // 2] if lat else 0.0
        p95 = sorted(lat)[int(len(lat) * 0.95)] if len(lat) >= 20 else (max(lat) if lat else 0.0)
        return (
            f"UDP sent={self.udp_sent} udp_errors={self.udp_errors} | "
            f"route ok={self.route_ok} err={self.route_errors} | "
            f"traffic ok={self.traffic_ok} err={self.traffic_errors} | "
            f"route latency ms p50={p50:.1f} p95={p95:.1f}"
        )


def encode_varint(n: int) -> bytes:
    if n < 0:
        n &= (1 << 64) - 1
    out = bytearray()
    while n > 0x7F:
        out.append((n & 0x7F) | 0x80)
        n >>= 7
    out.append(n)
    return bytes(out)


def encode_key(field: int, wire_type: int) -> bytes:
    return encode_varint((field << 3) | wire_type)


def encode_traffic_batch(
    user_id: int, lat: float, lon: float, speed_kmh: float, bearing_deg: float, timestamp_ms: int
) -> bytes:
    """Protobuf traffic.TrafficBatch with a single TrafficPacket."""
    body = b"".join(
        [
            encode_key(1, 0) + encode_varint(user_id),
            encode_key(2, 1) + struct.pack("<d", lat),
            encode_key(3, 1) + struct.pack("<d", lon),
            encode_key(4, 5) + struct.pack("<f", speed_kmh),
            encode_key(5, 5) + struct.pack("<f", bearing_deg),
            encode_key(6, 0) + encode_varint(timestamp_ms),
        ]
    )
    return encode_key(1, 2) + encode_varint(len(body)) + body


def parse_route_coords(raw: str) -> Tuple[str, str]:
    parts = [p.strip() for p in raw.split(";") if p.strip()]
    if len(parts) < 2:
        raise ValueError("ROUTE_COORDS must contain at least two lon,lat pairs separated by ;")
    return parts[0], parts[1]


def http_get(url: str, timeout: float = 15.0) -> Tuple[int, str]:
    req = urllib.request.Request(url, headers={"User-Agent": "osrm-live-traffic-stress/1.0"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        body = resp.read().decode("utf-8", errors="replace")
        return resp.status, body


def udp_sender(
    stats: StressStats,
    stop: threading.Event,
    host: str,
    port: int,
    max_rate: int,
    base_lat: float,
    base_lon: float,
) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    window_start = time.monotonic()
    sent_in_window = 0
    rng = random.Random(42)

    try:
        while not stop.is_set():
            now = time.monotonic()
            if now - window_start >= 1.0:
                window_start = now
                sent_in_window = 0

            if sent_in_window >= max_rate:
                time.sleep(0.005)
                continue

            user_id = rng.randint(1, 50_000)
            lat = base_lat + rng.uniform(-0.02, 0.02)
            lon = base_lon + rng.uniform(-0.02, 0.02)
            speed = rng.uniform(1.0, 130.0)
            bearing = rng.uniform(0.0, 359.0)
            ts = int(time.time() * 1000)
            packet = encode_traffic_batch(user_id, lat, lon, speed, bearing, ts)

            try:
                sock.sendto(packet, (host, port))
                stats.udp_sent += 1
                sent_in_window += 1
            except OSError:
                stats.udp_errors += 1

            # Random micro-intervals: average well below 100/s but bursty
            time.sleep(rng.uniform(0.0, 0.02))
    finally:
        sock.close()


def route_worker(stats: StressStats, stop: threading.Event, osrm_url: str, coord_a: str, coord_b: str) -> None:
    url = f"{osrm_url}/route/v1/driving/{coord_a};{coord_b}?overview=false"
    while not stop.is_set():
        started = time.monotonic()
        try:
            status, body = http_get(url)
            if status == 200 and '"code":"Ok"' in body:
                stats.route_ok += 1
                stats.add_route_latency((time.monotonic() - started) * 1000.0)
            else:
                stats.route_errors += 1
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError):
            stats.route_errors += 1
        time.sleep(random.uniform(0.05, 0.25))


def traffic_worker(stats: StressStats, stop: threading.Event, osrm_url: str) -> None:
    url = f"{osrm_url}/traffic/v1/driving/"
    while not stop.is_set():
        try:
            status, body = http_get(url)
            if status == 200 and '"code":"Ok"' in body:
                stats.traffic_ok += 1
            else:
                stats.traffic_errors += 1
        except (urllib.error.URLError, TimeoutError):
            stats.traffic_errors += 1
        time.sleep(random.uniform(0.1, 0.4))


def probe_osrm(osrm_url: str) -> bool:
    try:
        status, _ = http_get(f"{osrm_url}/nearest/v1/driving/28.8608,47.0105?number=1", timeout=3.0)
        return status == 200
    except Exception:
        return False


def main() -> int:
    osrm_url = env_str("OSRM_URL", "http://127.0.0.1:7070").rstrip("/")
    udp_host = env_str("UDP_HOST", "127.0.0.1")
    udp_port = env_int("UDP_PORT", 9900)
    duration = env_int("DURATION_SEC", 30)
    max_udp_rate = env_int("MAX_UDP_RATE", 100)
    route_workers = env_int("ROUTE_WORKERS", 8)
    coord_a, coord_b = parse_route_coords(env_str("ROUTE_COORDS", "28.8608,47.0105;28.8750,47.0200"))

    print("=== OSRM live traffic stress test ===")
    print(
        f"osrm={osrm_url} udp={udp_host}:{udp_port} duration={duration}s "
        f"max_udp_rate={max_udp_rate}/s route_workers={route_workers}"
    )

    if not probe_osrm(osrm_url):
        print(
            "ERROR: OSRM server not reachable. Start osrm-routed with --enable-live-data first.",
            file=sys.stderr,
        )
        return 2

    stats = StressStats()
    stop = threading.Event()

    udp_thread = threading.Thread(
        target=udp_sender,
        args=(stats, stop, udp_host, udp_port, max_udp_rate, 47.0105, 28.8608),
        daemon=True,
    )
    udp_thread.start()

    with ThreadPoolExecutor(max_workers=route_workers + 2) as pool:
        futures = [pool.submit(route_worker, stats, stop, osrm_url, coord_a, coord_b) for _ in range(route_workers)]
        futures.append(pool.submit(traffic_worker, stats, stop, osrm_url))
        futures.append(pool.submit(traffic_worker, stats, stop, osrm_url))

        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            time.sleep(1.0)
            print(f"[{int(deadline - time.monotonic())}s left] {stats.summary()}")

        stop.set()
        udp_thread.join(timeout=5.0)
        for fut in as_completed(futures, timeout=10.0):
            fut.result()

    print(f"FINAL: {stats.summary()}")

    if stats.udp_sent < duration:
        print("FAIL: too few UDP packets sent", file=sys.stderr)
        return 1
    if stats.route_ok == 0:
        print("FAIL: no successful route responses", file=sys.stderr)
        return 1
    if stats.route_errors > stats.route_ok * 0.05:
        print("FAIL: route error rate > 5%", file=sys.stderr)
        return 1

    print("OK: stress test passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
