#!/usr/bin/env python3
"""soak_report.py — 汇总 soak_run.sh 数据目录，产出 A2 验收对照的 summary。

用法: python3 soak_report.py <soak数据目录>   （stdout 即 markdown 报告）

A2 验收（docs/design/20260824-iteration2-audit-goals.md）:
  吞吐/时延曲线、恢复次数=注入次数、RSS 平稳 —— 三项各给数据+判定。
"""
import math
import os
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else "."
LEAK_KB_PER_H = 10 * 1024  # RSS 斜率超过 10MB/h 判泄漏趋势


def read_csv(name, ts_col=0):
    """读 CSV 跳过表头；ts_col 指定哪一列必须是数值（inject_log 首列是事件名）。"""
    path = os.path.join(OUT, name)
    if not os.path.exists(path):
        return []
    rows = []
    with open(path) as f:
        for line in f:
            parts = line.strip().split(",")
            if parts and parts[ts_col].replace(".", "", 1).replace("-", "", 1).isdigit():
                rows.append(parts)
    return rows


def fmt_h(seconds):
    return f"{seconds / 3600:.1f}h"


def slope_kb_per_h(pairs):
    """最小二乘斜率 (ts, rss_kb) → KB/h；样本不足或 rss 恒 0 返回 0。"""
    n = len(pairs)
    if n < 5:
        return 0.0
    xs = [p[0] for p in pairs]
    ys = [p[1] for p in pairs]
    if max(ys) == 0:
        return 0.0
    mx, my = sum(xs) / n, sum(ys) / n
    den = sum((x - mx) ** 2 for x in xs)
    if den == 0:
        return 0.0
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / den * 3600.0


samples = read_csv("samples.csv")
goals = read_csv("goals.csv")
monitor = read_csv("monitor.csv")
rss = read_csv("rss.csv")
events = read_csv("inject_log.csv", ts_col=1)

lines = []
add = lines.append

add("# Soak 报告")
add("")
add(f"- 数据目录: `{OUT}`")

# ── 时长与 scan 健康 ──────────────────────────────────────────────────
if samples:
    t0, t1 = float(samples[0][0]), float(samples[-1][0])
    dur = t1 - t0
    vals = [int(r[1]) for r in samples]
    degraded = sum(1 for v in vals if v < 50)
    add(f"- 时长: {fmt_h(dur)}（{len(samples)} 个采样点）")
    add(f"- scan 健康: 中位 {sorted(vals)[len(vals)//2]} 回波, "
        f"劣化采样点 {degraded}/{len(samples)} ({100*degraded/len(samples):.1f}%)")
else:
    dur = 0.0
    add("- ⚠️ 无采样数据")

# ── 吞吐（patrol goal 事件）──────────────────────────────────────────
STATIONS = {(17.0, 4.0): "机台1", (17.0, -4.0): "机台2", (1.0, 0.0): "home"}


def station(x, y):
    return next((n for (gx, gy), n in STATIONS.items()
                 if math.hypot(x - gx, y - gy) < 1.5), f"({x:.1f},{y:.1f})")


if dur > 0:
    per_h = len(goals) / (dur / 3600)
    add(f"- 吞吐: {len(goals)} 个 goal 事件（≈{per_h:.1f}/h, patrol 一轮 3 站 ≈{per_h/3:.1f} 循环/h）")
    if goals:
        names = [station(float(g[1]), float(g[2])) for g in goals]
        add(f"  站序: {' → '.join(names[:12])}{' …' if len(names) > 12 else ''}")

# ── 运动（monitor.csv）───────────────────────────────────────────────
if len(monitor) > 1:
    dist = 0.0
    moving = 0
    for a, b in zip(monitor, monitor[1:]):
        try:
            step = math.hypot(float(b[1]) - float(a[1]), float(b[2]) - float(a[2]))
        except ValueError:  # NaN 位姿（amcl 未上线窗口）
            continue
        dist += max(0.0, step)  # 重生回 (1,0) 的负位移不算运动
        if step > 0.05:
            moving += 1
    ctrl_alive = sum(1 for m in monitor if float(m[3]) > 0.5)
    add(f"- 运动: 累计位移 {dist:.0f} m（采样间隔位移>0.05m 的窗口 {moving}）")
    add(f"- 控制环: cmd_vel 发布窗口 {ctrl_alive}/{len(monitor)} "
        f"({100*ctrl_alive/len(monitor):.0f}%) —— 注: 计的是消息率（零速也发）, "
        f"真运动看上一行")

# ── 故障注入与恢复（验收: 恢复次数=注入次数）──────────────────────────
inj = [e for e in events if e[0] == "inject"]
rec = [e for e in events if e[0] == "recovered"]
tmo = [e for e in events if e[0] == "timeout"]
ext = [e for e in events if e[0] == "external"]
add("")
add("## 故障注入")
add(f"- 注入 **{len(inj)}** 次, 恢复 **{len(rec)}** 次"
    + (f", 超时 **{len(tmo)}** 次" if tmo else ""))
if inj:
    dts = [int(r[3]) for r in rec]
    verdict = "✅ 恢复次数=注入次数" if len(rec) == len(inj) and not tmo else \
              ("⚠️ 有超时未恢复" if tmo else "⚠️ 部分未确认")
    add(f"- 恢复时延: 中位 {sorted(dts)[len(dts)//2]}s / 最大 {max(dts)}s —— {verdict}")
    for r in rec:
        add(f"  - t+{r[3]}s ← {r[2]}")
    for r in tmo:
        add(f"  - ❌ {r[2]} 超时 {r[3]}s 未恢复")
if ext:
    add(f"- 非注入劣化（WSL2 渲染/栈自亡, watchdog 自愈）: **{len(ext)}** 次, "
        f"最长 {max(int(e[3]) for e in ext)}s —— 72h 视角的渲染劣化实证数据")

# ── RSS 平稳性（验收: 无泄漏趋势）────────────────────────────────────
add("")
add("## 进程内存 (RSS)")
by_name = {}
for r in rss:
    by_name.setdefault(r[1], []).append((float(r[0]), int(r[2])))
if by_name:
    add("| 进程 | 均值MB | 峰值MB | 首小时MB | 末小时MB | 斜率MB/h | 判定 |")
    add("|------|-------:|-------:|---------:|---------:|---------:|------|")
    for name, pairs in sorted(by_name.items()):
        vals = [v for _, v in pairs]
        mb = lambda kb: kb / 1024  # noqa: E731
        h = dur / 3600 if dur else 1.0
        first = [v for t, v in pairs if t - pairs[0][0] <= 3600] or vals[:1]
        last = [v for t, v in pairs if pairs[-1][0] - t <= 3600] or vals[-1:]
        s = slope_kb_per_h(pairs)
        if dur < 3600:
            flag = "—（<1h 窗口不判趋势）"
        else:
            flag = "⚠️ 泄漏趋势" if s > LEAK_KB_PER_H else "平稳"
        add(f"| {name} | {mb(sum(vals)/len(vals)):.0f} | {mb(max(vals)):.0f} | "
            f"{mb(sum(first)/len(first)):.0f} | {mb(sum(last)/len(last)):.0f} | "
            f"{mb(s):.2f} | {flag} |")
else:
    add("- ⚠️ 无 RSS 数据")

# ── 时延快照索引（:9091, 存在时）────────────────────────────────────
snaps = sorted(f for f in os.listdir(OUT) if f.startswith("metrics_"))
add("")
add("## 时延快照")
add(f"- :9091 快照 {len(snaps)} 份"
    + (f"（{snaps[0]} … {snaps[-1]}）" if snaps else " — 端点未启用"
       "（需 AMR_PERF_INSTRUMENTATION=ON 构建）"))

print("\n".join(lines))
