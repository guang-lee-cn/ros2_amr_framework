#!/usr/bin/env python3
"""coverage_report.py — 从 lcov .info 直接计算覆盖率（B4：可复算度量链）。

背景（2026-08-25 外部审计 P1-a）：lcov 2.0 的 --list per-file 数字与 .info
自身 DA 数据矛盾（ring_buffer 21/21 全命中，--list 报 61.9%）；旧
quality.sh 用 awk 解析 --list 生成 coverage_full.txt，明细因此不可信，
且总览数字无原始件可复算。修复原则：**单一定义、单源计算**——本脚本
直接解析 .info 的 DA 行（line_no,exec_count），明细/聚合/badge 三者同源。

用法:
  coverage_report.py <coverage.info> <out_dir>
输出:
  stdout                      人读摘要（聚合 + 最低 N 文件）
  <out_dir>/coverage_full.txt 全文件明细（与 .info DA 直算一致）
  <out_dir>/badge.json        shields.io endpoint 格式
"""
import json
import os
import sys

THRESHOLDS = [(80.0, "brightgreen"), (60.0, "yellow"), (40.0, "orange")]


def parse_info(path):
    files = []  # (name, hit, total)
    name, hit, total = "", 0, 0
    seen = {}
    with open(path) as f:
        for line in f:
            if line.startswith("SF:"):
                if total:
                    seen.setdefault(name, [0, 0])
                    seen[name][0] += hit
                    seen[name][1] += total
                name, hit, total = line[3:].strip(), 0, 0
            elif line.startswith("DA:"):
                fields = line[3:].strip().split(",")
                total += 1
                if len(fields) >= 2 and int(fields[1]) > 0:
                    hit += 1
    if total:
        seen.setdefault(name, [0, 0])
        seen[name][0] += hit
        seen[name][1] += total
    return [(n, h, t) for n, (h, t) in seen.items()]


def main():
    info_path, out_dir = sys.argv[1], sys.argv[2]
    files = parse_info(info_path)
    if not files:
        print("no coverage data", file=sys.stderr)
        return 1

    agg_hit = sum(f[1] for f in files)
    agg_total = sum(f[2] for f in files)
    pct = 100.0 * agg_hit / agg_total if agg_total else 0.0
    short = lambda n: n.split("ros2_robot_middleware/")[-1]  # noqa: E731

    files.sort(key=lambda f: f[1] / f[2])
    with open(os.path.join(out_dir, "coverage_full.txt"), "w") as f:
        f.write(f"# 由 coverage_report.py 从 lcov .info 直算（DA 行口径）\n")
        f.write(f"# 聚合: {agg_hit}/{agg_total} = {pct:.1f}%  文件数: {len(files)}\n")
        for n, h, t in files:
            f.write(f"{short(n):<70s} {100.0*h/t:5.1f}%  {h:5d}/{t:5d}\n")

    color = next((c for th, c in THRESHOLDS if pct >= th), "red")
    badge = {
        "schemaVersion": 1,
        "label": "coverage",
        "message": f"{pct:.1f}%",
        "color": color,
    }
    with open(os.path.join(out_dir, "badge.json"), "w") as f:
        json.dump(badge, f, indent=2)
        f.write("\n")

    print(f"聚合: {agg_hit}/{agg_total} = {pct:.1f}% ({len(files)} 文件)")
    print("最低 5:")
    for n, h, t in files[:5]:
        print(f"  {100.0*h/t:5.1f}%  {short(n)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
