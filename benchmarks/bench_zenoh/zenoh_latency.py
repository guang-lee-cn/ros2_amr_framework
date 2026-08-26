#!/usr/bin/env python3
"""基准二(时延)：zenoh ping-pong 往返时延。
用法: zenoh_latency.py --role pong
      zenoh_latency.py --role ping --size 256 --samples 3000 --warmup 300
协议与 bench_ipc 相同：payload 前 12 字节 = seq(u32 LE) + t_send_ns(u64 LE)。
"""
import argparse
import json
import struct
import threading
import time

import zenoh

PING_TOPIC = "mw/bench/zenoh/ping"
PONG_TOPIC = "mw/bench/zenoh/pong"


def encode(seq: int, t_ns: int, size: int) -> bytes:
    return struct.pack("<IQ", seq, t_ns) + b"\xA5" * max(0, size - 12)


def decode(buf: bytes):
    seq, t_ns = struct.unpack_from("<IQ", buf, 0)
    return seq, t_ns


def pct(sorted_vals, p):
    i = min(len(sorted_vals) - 1, int(p * len(sorted_vals)))
    return sorted_vals[i]


def run_pong(session):
    def on_ping(sample):
        seq, _ = decode(sample.payload.to_bytes())
        if seq == 0:
            return  # 探测帧
        session.put(PONG_TOPIC, sample.payload.to_bytes())

    session.declare_subscriber(PING_TOPIC, handler=on_ping)
    print("pong ready", flush=True)
    threading.Event().wait()


def run_ping(session, size, samples, warmup):
    rtts = []
    seq = 0
    received = threading.Event()
    lock = threading.Lock()

    def on_pong(sample):
        nonlocal received
        s, t_ns = decode(sample.payload.to_bytes())
        now = time.monotonic_ns()
        if s != seq:
            return
        rtt_us = (now - t_ns) / 1000.0
        with lock:
            if seq >= warmup:
                rtts.append(rtt_us)
            received.set()

    session.declare_subscriber(PONG_TOPIC, handler=on_pong)
    time.sleep(1.0)  # 匹配等待

    while len(rtts) < samples:
        seq += 1
        payload = encode(seq, time.monotonic_ns(), size)
        received.clear()
        session.put(PING_TOPIC, payload)
        if not received.wait(timeout=2.0):
            session.put(PING_TOPIC, payload)  # 重发
            received.wait(timeout=2.0)

    rtts.sort()
    total = sum(rtts)
    print(json.dumps({
        "bench": "zenoh_latency",
        "payload_bytes": size,
        "samples": len(rtts),
        "rtt_us": {
            "min": round(rtts[0], 1), "p50": round(pct(rtts, 0.50), 1),
            "p90": round(pct(rtts, 0.90), 1), "p99": round(pct(rtts, 0.99), 1),
            "p999": round(pct(rtts, 0.999), 1), "max": round(rtts[-1], 1),
            "mean": round(total / len(rtts), 1),
        },
        "one_way_us": {"p50": round(pct(rtts, 0.50) / 2, 1), "p99": round(pct(rtts, 0.99) / 2, 1)},
    }, ensure_ascii=False), flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--role", choices=["ping", "pong"], required=True)
    ap.add_argument("--size", type=int, default=256)
    ap.add_argument("--samples", type=int, default=3000)
    ap.add_argument("--warmup", type=int, default=300)
    args = ap.parse_args()

    session = zenoh.open(zenoh.Config())
    try:
        if args.role == "pong":
            run_pong(session)
        else:
            run_ping(session, args.size, args.samples, args.warmup)
    finally:
        session.close()


if __name__ == "__main__":
    main()
