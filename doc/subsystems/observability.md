# 可观测性

## 一、位置

```mermaid
flowchart TB
    subgraph nodes["业务节点"]
        FUSION[FusionNode]
        DECISION[DecisionNode]
        MOTOR[MotorCtrlNode]
        HEALTH[HealthMonitor]
    end

    subgraph obs["可观测性层 (库链接)"]
        TR[Traces: TracerContext]
        ME[Metrics: shared_metrics()]
        LO[Logs: spdlog async]
    end

    FUSION & DECISION & MOTOR & HEALTH --> TR
    FUSION & DECISION & MOTOR --> ME
    FUSION --> LO

    TR -.-> DIAG["/diagnostics + LTTng"]
    ME -.-> PROM["Prometheus :9090"]
    LO -.-> STDOUT["stdout JSON"]

    style obs fill:#f3e5f5,stroke:#7b1fa2,stroke-width:3px
```

> 横切关注点，以库形式链接到所有业务节点。不参与数据流/控制流，为两条流提供可视化。

## 二、内部结构

| 支柱 | 组件 | 实现 | 热路径延迟 |
|------|------|------|:---:|
| **Traces** | `TracerContext` (thread_local) + `ScopedSpan` (RAII) | trace_id 自动注入 LOG_OBS | ~50ns |
| **Metrics** | `shared_metrics()` → `std::atomic` → POSIX `shm_open` | 5 进程共享 `/amr_metrics_registry` | ~10ns |
| **Logs** | `spdlog` async logger (1 thread, 8192 queue) | JSON pattern → stdout | ~100ns |

### Trace Points (trace_points.hpp)

| Span 名称 | 位置 | 说明 |
|------|------|------|
| `FUSION_TIMER` | FusionNode::timer_callback | 融合主循环 |
| `FUSION_DEGRADATION` | FusionNode (降级时) | 降级事件标记 |
| `FUSION_CLUSTER_DETECT` | PerceptionService::fuse | DBSCAN 聚类 |
| `DECISION_ON_PERCEPTION` | DecisionNode::on_perception | 感知→Goal 分发 |
| `DECISION_SEND_GOAL` | DecisionNode::send_goal | Action 发送 |
| `MOTOR_EXECUTE` | MotorCtrlNode::execute | 轨迹执行 |
| `MOTOR_STEP` | MotorCtrlNode (每步) | 单步插值 |
| `HEALTH_CHECK` | HealthMonitor::check_health | 健康巡检 |

### Metrics 速查

| 指标 | 类型 | 说明 |
|------|:---:|------|
| `amr_sensor_rate_hz` | Gauge | LiDAR/IMU/Camera 实际频率 |
| `amr_fusion_latency_seconds` | Histogram | 融合耗时分布 |
| `amr_decision_latency_seconds` | Histogram | 决策耗时分布 |
| `amr_motor_latency_seconds` | Histogram | 执行耗时分布 |
| `amr_e2e_latency_seconds` | Histogram | 端到端 (sensor→cmd_vel) |
| `amr_degradation_level` | Gauge | 当前降级等级 (0-4) |
| `amr_degradation_events_total` | Counter | 降级事件累计 |
| `amr_object_count` | Gauge | 跟踪目标数 |

## 三、核心流程

### Trace→Log 关联

```
TRACE_SCOPE(amr::trace::FUSION_TIMER) → TracerContext.trace_id = 42, span_id = 1
  │
  ├─ 业务逻辑...
  │
  └─ LOG_OBS(INFO, "fusion", "done", lat_us, count)
       → LogEvent.trace_id = 42  ← 自动注入
       → spdlog async queue → stdout JSON
```

### 故障定位路径

```
1. Prometheus 告警 P99 > 50ms @ 14:03:22
2. grep 日志 14:03:20~14:03:25 → trace_id=0x3f2a
3. 关联日志: "LiDAR outlier range=655.35" → 根因: 硬件瞬时异常
```

## 四、接口

| 接口 | 类型 | 消费者 |
|------|------|--------|
| `TracerContext` (thread_local) | 进程内 | 所有业务节点 |
| `shared_metrics()` (POSIX SHM) | 跨进程 | HealthMonitor (Prometheus) |
| `spdlog` async logger | stdout JSON | Docker logging driver / journald |
| `/diagnostics` (DDS) | 跨进程 | rqt_runtime_monitor |
| `:9090/metrics` (HTTP) | 跨进程 | Prometheus → Grafana |

## 五、边界与降级

| 故障 | 行为 |
|------|------|
| spdlog 队列满 (8192) | `async_overflow_policy::block` — 丢弃旧消息，不阻塞热路径 |
| SHM 文件残留 | launch 脚本 `rm -f /dev/shm/amr_metrics_registry` 启动前清理 |
| Prometheus socket bind 失败 | `RCLCPP_WARN`，业务不中断 |
| TracerContext 未初始化 | `LOG_OBS` 中 trace_id = 0（无活跃 trace） |

### 测试覆盖

| 测试 | 覆盖 |
|------|------|
| `test_observability` (11) | RingBuffer (5), MetricsRegistry (3), TraceContext (3) |

## 六、参考

- [可观测性设计文档](../guides/07-observability-design.md)
- [可观测性使用指南](../guides/08-observability-usage.md)
- [Grafana Dashboard](https://github.com/guang-lee-cn/ros2_amr_framework/blob/main/config/grafana_dashboard.json)
