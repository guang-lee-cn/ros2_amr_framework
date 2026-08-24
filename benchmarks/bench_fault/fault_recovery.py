#!/usr/bin/env python3
"""基准三：故障恢复时间——pong 被 kill -9 后，拉起新 pong 到 ping 恢复收帧的墙钟时间。
口径: 下界 = 进程重启 + DDS 重发现 + 重匹配（不含心跳检测策略时延）。
方法: kill 后 20ms 内到达的帧视为在途回声，之后的首帧即真实恢复。
"""
import argparse
import json
import os
import select
import subprocess
import sys
import time

ROS_ENV = dict(os.environ)
ROS_ENV["RMW_IMPLEMENTATION"] = "rmw_cyclonedds_cpp"

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PONG_BIN = os.path.join(ROOT, "install/bench_ipc/lib/bench_ipc/pong")
PING_BIN = os.path.join(ROOT, "install/bench_ipc/lib/bench_ipc/ping")


def spawn_pong():
    return subprocess.Popen(
        ["taskset", "-c", "4", PONG_BIN],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=ROS_ENV)


def read_line_with_timeout(pipe, timeout_s):
    """select 等一行; 超时返回 None。"""
    deadline = time.monotonic() + timeout_s
    buf = b""
    while True:
        remain = deadline - time.monotonic()
        if remain <= 0:
            return None
        r, _, _ = select.select([pipe], [], [], remain)
        if not r:
            return None
        ch = os.read(pipe, 1)
        if not ch:
            return ""  # EOF
        buf += ch
        if ch == b"\n":
            return buf.decode()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rounds", type=int, default=5)
    ap.add_argument("--silence_ms", type=int, default=400)
    args = ap.parse_args()

    # 入口清理：杀掉可能污染测量的残留实例（方括号避免自匹配）
    subprocess.run(["pkill", "-9", "-f", "bench_ipc/pon[g]"], check=False)
    subprocess.run(["pkill", "-9", "-f", "bench_ipc/pin[g]"], check=False)
    time.sleep(0.5)

    spawned = []

    def spawn_tracked():
        p = spawn_pong()
        spawned.append(p)
        return p

    recoveries_ms = []
    for rnd in range(args.rounds):
        if rnd == 0:
            pong = spawn_tracked()
            time.sleep(1.5)
        ping = subprocess.Popen(
            ["taskset", "-c", "2", PING_BIN, "--ros-args",
             "-p", "size:=1024", "-p", "samples:=100000", "-p", "warmup:=50",
             "-p", "stream:=true"],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=ROS_ENV)

        fd = ping.stdout.fileno()
        healthy = 0
        t_kill = None
        t_recover = None
        while True:
            line = read_line_with_timeout(fd, 10.0)
            if line is None or line == "":
                break
            if not line.startswith("S "):
                continue
            now = time.monotonic()
            if t_kill is None:
                healthy += 1
                if healthy >= 300:
                    pong.kill()
                    pong.wait()
                    t_kill = now
                    pong = spawn_tracked()  # supervisor 立即拉起新实例
            else:
                # 在途回声判据: kill 后 20ms 内到达的帧视为杀前乒乓余帧;
                # 之后的首帧 = 重发机制下经新 pong 的真实恢复
                # (串行乒乓 RTT 亚毫秒级, 20ms 已足够宽; 若有残留 pong 会因
                #  帧流连续而在 20ms 后误判——入口 pkill 已保证无残留)
                if (now - t_kill) * 1000.0 >= 20.0:
                    t_recover = now
                    break
        if t_kill and t_recover:
            recoveries_ms.append(round((t_recover - t_kill) * 1000.0, 1))
            print(f"round {rnd}: recovery = {recoveries_ms[-1]} ms", flush=True)
        else:
            print(f"round {rnd}: no valid recovery measured", file=sys.stderr, flush=True)
        ping.kill()
        ping.wait()
        time.sleep(0.5)

    for p in spawned:
        p.kill()
    if recoveries_ms:
        recoveries_ms.sort()
        print(json.dumps({
            "bench": "fault_recovery",
            "rmw": "rmw_cyclonedds_cpp",
            "rounds": len(recoveries_ms),
            "recovery_ms": {
                "min": recoveries_ms[0],
                "median": recoveries_ms[len(recoveries_ms) // 2],
                "max": recoveries_ms[-1],
            },
            "note": "下界口径: exec重启+DDS重发现+重匹配; 不含心跳检测时延(实际supervisor需加检测周期)",
        }, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    main()
