# ADR：B1 Supervisor 实体化（进程级崩溃监管/依赖序重启/健康门）

> 对应迭代2 B1（docs/design/20260824-iteration2-audit-goals.md）：
> 「supervisor 实体化：崩溃监管/依赖序重启/健康门；声明式配置驱动；
> kill -9 任一节点按策略恢复（health_monitor/OtaCoordinator 零件复用）」
> 状态：已实施（2026-08-25）

## 背景与缺口

| 现有件 | 能力 | 缺口 |
|--------|------|------|
| ros2 launch | 声明式拉起 | 进程死亡不 respawn（compute kill -9 = 永久停滞，soak 注入白名单因此排除 compute） |
| health_monitor | 心跳超时检测 + ChangeState 重启 | 只能重启**进程内** lifecycle 状态，进程死了无能为力；且不在 simulation.launch.py 里 |
| sim_watchdog | scan 断流→全栈重启 | 单一信号源、整栈粒度、恢复 2-4min；不看业务进程死活 |

「落后于 stock」审计项的本体：框架没有**监管原语**——节点组合是声明式的
（pipeline.nodes），进程生命周期管理却散落在 launch/脚本/watchdog 三处。

## 决策

### D1：进程级 supervisor（`amr_supervisor` 可执行）

posix_spawn 拉起子进程（各自独立进程组，`posix_spawnattr_setpgroup`），
waitpid(WNOHANG) 250ms 轮询驱动状态机。supervisor 是唯一常驻编排进程；
ros2 launch / nohup 只负责把 supervisor 本身拉起来。

- 拒绝 `launch respawn=True`：没有依赖序/退避/预算/级联，用 stock 开关
  等于框架没长能力（但共存：supervisor 可由 launch 拉起）
- 拒绝 SIGCHLD 信号驱动：异步信号安全复杂度 > 收益；重启时延由退避主导，
  250ms 轮询不构成瓶颈
- 子进程组式清场：kill(-pgid) 防孙进程泄漏（2026-08-16 进程泄漏教训：
  `pkill gz` 残留整套节点栈）

### D2：策略内核是 Domain 纯逻辑（`domain/monitoring/supervisor_policy.hpp`）

零 ROS 头文件（分层红线），状态机全部单测锁定：

```
STOPPED → STARTING → RUNNING
              │ (exit≠0 / startup 超时)     │ (exit, 长驻进程 exit 0 也算)
              ▼                            ▼
           BACKOFF ──(退避到期)──▶ STARTING（restarts_in_window++）
              │ window 内 > max_restarts
              ▼
            FATAL ──(级联)──▶ 依赖者全部停止（不给下游喂死源）
```

- **指数退避**：base×2^step，封顶 max；RUNNING 稳定 ≥ window 后计数与
  步数清零（镜像 RecoveryPolicy 的 reset_on_ok 语义）
- **重启预算**：window 内超 max_restarts → FATAL（镜像 RecoveryPolicy 的
  escalate(ERROR→FATAL) 语义，kMaxRetries=3 的推广：可配 + 带时间窗）
- **依赖序**：Kahn 拓扑排序决定拉起序；依赖重启 → 传递依赖者按逆拓扑先杀、
  依赖恢复 RUNNING 后按正拓扑重生（gz 重启 → spawn/bridge/… 全链有序回来）
- **oneshot 语义**：spawn_amcl 类一次性进程 exit 0 = 成功 STOPPED；
  长驻进程 exit 0 = 意外死亡，照常重启
- **迟到的 EXITED 免疫**：EXITED 事件只在 STARTING/RUNNING 相位有效，
  停滞僵尸的晚到 reap 不污染新周期状态

### D3：声明式配置（参数 schema，pipeline.nodes 同一哲学）

```
supervisor.children = [scan_filter, compute, patrol]
supervisor.<name>.cmd = [argv...]          # 如 ["ros2","run",...]
supervisor.<name>.depends_on = [...]
supervisor.<name>.oneshot = bool
supervisor.<name>.max_restarts / window_s / backoff_base_ms /
    backoff_max_ms / startup_timeout_s
```

配置校验在 on_configure：拓扑序计算 + 环/未知依赖拒绝启动
（配置错 = 启动失败，不带病运行）。

### D4：复用件清单（「零件复用」的落点）

| 复用 | 从哪 | 怎么用 |
|------|------|--------|
| HealthReport/HealthStatus 消息 | health_monitor | supervisor 状态出口（latched_state QoS），消费端（Grafana/Foxglove）零新增 |
| budget→FATAL 升级语义 | RecoveryPolicy | 窗口化推广（见 D2） |
| AmrNode 基类 | 收敛层 | 心跳/QoS/生命周期托管，CLAUDE.md 节点形态红线 |
| amr::qos 词汇表 | 收敛层 | 状态发布 latched_state，不手搓 QoS |

### D5：健康门分级

- **v1（本次）**：进程存活 + startup 超时（STARTING 超窗 = crash 路径）。
  这已覆盖「kill -9 任一节点按策略恢复」验收。
- **v2（后续）**：heartbeat 话题级健康门（STARTING→RUNNING 需心跳确认），
  对接 health_monitor 的心跳格式——AmrNode 心跳已在所有新节点里。

## 后果与边界

- supervisor 自身单点：真机用 systemd unit 兜底（部署文档职责）；仿真/开发
  由 launch 或 nohup 拉起
- 子进程重启不保留进程内状态（lifecycle configure 重走）——与 sim_watchdog
  全栈重启同语义，粒度更细、时延更低（退避 1s 起 vs 判定窗 2min）
- soak 联动：B1 验证后 `soak_run.sh` 注入白名单扩 `compute`
  （victim 白名单注释即解除点）

## 验证记录（2026-08-25）

- 单测：`test_supervisor_policy.cpp` ——拓扑序（链/菱形/环拒绝/未知依赖拒绝）、
  退避曲线、预算→FATAL、稳定窗清零、oneshot、迟到 EXITED 免疫、级联停止
- 集成：3 真实子进程（scan_filter + compute_container + patrol_3c）——
  kill -9 compute → 级联停 patrol → 退避重启 compute → patrol 跟随重生；
  连杀超预算 → FATAL（见 change journal 当日条目）
