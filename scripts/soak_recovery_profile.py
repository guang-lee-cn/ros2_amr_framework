#!/usr/bin/env python3
"""soak_recovery_profile.py — 从 supervisor 状态机日志对齐注入时间，
产出精确恢复时长画像（替代 soak 主循环 60s 轮询粒度的 downtime_s）。

用法:
  # 容器内提取事件流（supervisor 自己的状态迁移行）
  grep -a "supervisor]:" /tmp/scene_launch.log \
    | grep -aE "RUNNING → |→ RUNNING|BACKOFF|FATAL|已杀" > sup_events.txt
  python3 soak_recovery_profile.py sup_events.txt inject_log.csv

输入:
  sup_events.txt  — supervisor 状态迁移日志行（[ts] [supervisor]: child: A → B）
  inject_log.csv  — soak 注入记录（event,ts,detail,downtime_s）

输出（stdout）: 死亡检测时延 / 完整恢复时延 的分布 + 分受害者分解
"""
import csv
import re
import statistics
import sys

EVT = re.compile(r'\[(\d+\.\d+)\] \[supervisor\]: (\S+?): (\S+ → \S+|BACKOFF|FATAL|已杀.*)')


def load_events(path):
    events = []
    for line in open(path):
        m = EVT.search(line)
        if m:
            events.append((float(m.group(1)), m.group(2), m.group(3)))
    events.sort()
    return events


def load_injects(path):
    injects = []
    for r in csv.DictReader(open(path)):
        if r['event'] == 'inject':
            injects.append((float(r['ts']), r['detail'].split('(')[0]))
    return injects


def first_after(events, ts, child, needle):
    """注入后该 child 第一个含 needle 的状态事件时间，无则 None。"""
    for ets, c, kind in events:
        if ets > ts and c == child and needle in kind:
            return ets
    return None


def stats(v, name):
    if not v:
        print(f"{name}: 无样本")
        return
    s = sorted(v)
    print(f"{name}: n={len(s)} 中位={statistics.median(s):.2f}s "
          f"P90={s[int(len(s) * 0.9)]:.2f}s 最大={max(s):.2f}s 最小={min(s):.2f}s")


def main():
    events = load_events(sys.argv[1])
    injects = load_injects(sys.argv[2])
    print(f"supervisor 事件 {len(events)} 条，注入 {len(injects)} 次\n")

    detect, recover = [], []
    by_victim = {}
    for ts, victim in injects:
        d = first_after(events, ts, victim, 'RUNNING →')
        r = first_after(events, ts, victim, '→ RUNNING')
        if d:
            detect.append(d - ts)
        if r:
            recover.append(r - ts)
            by_victim.setdefault(victim, []).append(r - ts)

    stats(detect, "死亡检测时延 (inject → RUNNING→STOPPED)")
    stats(recover, "完整恢复时延 (inject → STARTING→RUNNING)")
    for victim, v in sorted(by_victim.items()):
        stats(v, f"  └ {victim}")
    # 分解：检测段 vs 拉起段
    if detect and recover:
        spawn = [r - d for r, d in zip(sorted(recover), sorted(detect))
                 if r > d]
        stats(spawn, "拉起段 (STOPPED→RUNNING)")


if __name__ == '__main__':
    main()
