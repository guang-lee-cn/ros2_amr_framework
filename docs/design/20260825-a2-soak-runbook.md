# A2 Soak 测试运行手册（72h 长稳 + 故障注入）

> 对应迭代2 A 类 A2（docs/design/20260824-iteration2-audit-goals.md）：
> 「72h soak + 故障注入：连续负载、周期性 kill -9、内存/泄露监控」
> 验收：soak 报告——吞吐/时延曲线、**恢复次数=注入次数**、RSS 平稳。

## Harness 组成

| 件 | 文件 | 角色 |
|----|------|------|
| 编排 | `scripts/soak_run.sh` | 采样循环 + 故障注入调度 + 恢复判定 + 收尾出报告 |
| 采样 | `scripts/soak_monitor.py` | /goal_pose(吞吐) /amcl_pose(位姿) /cmd_vel(活性) → CSV |
| 报告 | `scripts/soak_report.py` | 汇总 CSV → A2 验收对照 summary.md |
| 恢复 | `scripts/sim_watchdog.sh`（复用） | /scan_raw 断流 2min → run_sim 清场重启 |

**设计决策——注入对象白名单 {bridge, gz}**：
当前架构唯一自动恢复链是 sim_watchdog（scan 断流→全栈重启）。
`parameter_bridge` 是 /scan_raw 的发布者、`gz sim` 是传感源头，杀任一个
必然触发该链；而 `compute_container` 被杀后无人拉起（迭代2 B1 supervisor
的缺口），注入它会令 soak 永久停滞——**compute 注入留给 B1 落地后扩展**。

**恢复判定**（防止"scan 好了但业务死了"的假恢复）：
scan 探针 ≥ MIN_VALID **且** 注入后出现新 /goal_pose 事件
（soak_monitor 独立进程跨栈重启存活；patrol 随栈重启即重发首个 goal）。

## 正式 72h 跑

```bash
cd ~/code/ros2_ws/src/ros2_amr_framework
nohup ./scripts/soak_run.sh >/tmp/soak.log 2>&1 &
# 数据目录默认 /tmp/amr_soak_<时间戳>，日志会打印
```

- 默认参数：72h / 每 30min 注入（bridge↔gz 轮换）/ 60s 采样 / 恢复超时 15min
- 中途看进展：`tail -f /tmp/soak.log`；`wc -l /tmp/amr_soak_*/inject_log.csv`
- 结束方式：自然到点或 `kill -INT <pid>` → 自动生成 `<OUT>/summary.md`
- ⚠️ soak 结束后仿真栈仍在跑（供检查）；全停照常 `pkill -f sim_watch && pkill 'gz sim'` + run_sim 清场

### 可调参数（环境变量）

| 变量 | 默认 | 说明 |
|------|------|------|
| `DURATION_MIN` | 4320 | 总时长（分钟） |
| `INJECT_INTERVAL_MIN` | 30 | 注入间隔 |
| `SAMPLE_INTERVAL` | 60 | 采样周期（秒） |
| `RECOVERY_TIMEOUT_S` | 900 | 恢复超时，超时记 failure 但继续 |
| `VICTIMS` | bridge,gz | 轮换表（仅支持 bridge/gz） |
| `METRICS_EVERY` | 10 | :9091 时延快照间隔（tick，0=关） |

## smoke（15min，入库前验证过一轮）

```bash
DURATION_MIN=15 INJECT_INTERVAL_MIN=5 SAMPLE_INTERVAL=15 \
  OUT=/tmp/amr_soak_smoke ./scripts/soak_run.sh
```

预期：≥2 次注入、全部恢复（MTTR 约 2-4min = watchdog 4×30s 判定窗 +
run_sim 重启 ~40s）、RSS 表平稳、summary.md 三项验收有数据。

### smoke 实测（2026-08-25，15min 真跑）【铁证】

- 注入 **2** 次（bridge/gz 各一）、恢复 **2** 次，MTTR **125s / 150s**
  ——「恢复次数=注入次数」链路实证 ✅
- 顺带捕获 1 次真实的 WSL2 运行时渲染劣化（非注入），watchdog 自愈 301s
  ——harness 的「非注入劣化」检测器按设计工作 ✅
- 抽签启动 2/3（try1 盲、try2 健康 266 回波）——run_sim 兜底如常
- 教训落码：短窗（<1h）不判 RSS 泄漏趋势（首轮 smoke 在 12min 窗口上
  把 foxglove 启动期内存爬升误报为泄漏）；注入与外部劣化撞窗时外部记账
  截止到注入点

## 读报告

- **恢复次数=注入次数**：`## 故障注入` 段给出注入/恢复/超时计数与逐次时延
- **RSS 平稳**：每进程 首小时/末小时均值 + 最小二乘斜率，>10MB/h 标 ⚠️
- **吞吐**：goal 事件/h 与循环数/h（patrol 3 站/轮）
- **时延曲线**：需 `AMR_PERF_INSTRUMENTATION=ON` 构建，:9091 快照按 tick 落盘
- **非注入劣化**（external 事件）：WSL2 gpu_lidar 渲染劣化的 72h 实证频率——
  这不是噪声，是 A2 的观测目标之一（watchdog 自愈能力的数据）

## 已知边界

1. WSL2 启动抽签由 run_sim 兜底（3 次重试），连续失败建议 `wsl --shutdown`
2. patrol 随栈重启归位到 idx=0、机器人在 (1,0) 重生——吞吐统计按 goal 事件
   总数计，重启不中断计数
3. watchdog 复用：若已有 sim_watchdog 在跑，soak 不再起第二只（避免双重启）
