#!/usr/bin/env python3
"""基准二(吞吐)：zenoh 持续灌流测有效吞吐与丢帧率。
用法: zenoh_throughput.py --role recv
      zenoh_throughput.py --role send --size 65536 --duration 15
"""
import argparse
import json
import threading
import time

import zenoh

DATA_KEY = "mw/bench/zenoh/data"
STATS_KEY = "mw/bench/zenoh/stats"


def run_recv(session):
    frames = []
    bytes_total = [0]
    lock = threading.Lock()
    t_first = [None]
    t_last = [None]
    stats_event = threading.Event()
    sender_stats = {}

    def on_data(sample):
        now = time.monotonic()
        with lock:
            if t_first[0] is None:
                t_first[0] = now
            t_last[0] = now
            frames.append(len(sample.payload.to_bytes()))
            bytes_total[0] += len(sample.payload.to_bytes())

    def on_stats(sample):
        sender_stats.update(json.loads(sample.payload.to_bytes()))
        stats_event.set()

    session.declare_subscriber(DATA_KEY, handler=on_data)
    session.declare_subscriber(STATS_KEY, handler=on_stats)
    print("recv ready", flush=True)
    stats_event.wait(timeout=300)

    with lock:
        dur = (t_last[0] or 0) - (t_first[0] or 0)
        n = len(frames)
        b = bytes_total[0]
    sent_n = sender_stats.get("sent_frames", n)
    print(json.dumps({
        "bench": "zenoh_throughput",
        "mode": sender_stats.get("mode", "drop"),
        "payload_bytes": sender_stats.get("payload_bytes"),
        "duration_s": round(max(dur, 1e-9), 2),
        "recv_frames": n,
        "sent_frames": sent_n,
        "loss_pct": round(100.0 * (sent_n - n) / sent_n, 3) if sent_n else None,
        "goodput_MBps": round(b / max(dur, 1e-9) / 1e6, 2),
        "goodput_Mbps": round(b * 8 / max(dur, 1e-9) / 1e6, 1),
    }, ensure_ascii=False), flush=True)


def run_send(session, size, duration, mode="drop"):
    import zenoh as _z
    cc = _z.CongestionControl.BLOCK if mode == "block" else _z.CongestionControl.DROP
    payload = b"\xA5" * size
    time.sleep(2.0)  # 等接收方订阅就绪
    t_end = time.monotonic() + duration
    sent = 0
    t0 = time.monotonic()
    while time.monotonic() < t_end:
        session.put(DATA_KEY, payload, congestion_control=cc)
        sent += 1
    wall = time.monotonic() - t0
    session.put(STATS_KEY, json.dumps({
        "sent_frames": sent, "payload_bytes": size, "wall_s": round(wall, 2),
        "mode": mode}))
    time.sleep(1.0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--role", choices=["send", "recv"], required=True)
    ap.add_argument("--size", type=int, default=65536)
    ap.add_argument("--duration", type=int, default=15)
    ap.add_argument("--mode", choices=["drop", "block"], default="drop")
    args = ap.parse_args()

    session = zenoh.open(zenoh.Config())
    try:
        if args.role == "recv":
            run_recv(session)
        else:
            run_send(session, args.size, args.duration, args.mode)
    finally:
        session.close()


if __name__ == "__main__":
    main()
