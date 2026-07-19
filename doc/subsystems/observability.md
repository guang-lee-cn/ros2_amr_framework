# 可观测性

## 在总体架构中的位置

```mermaid
flowchart TB
    subgraph nodes["业务节点"]
        L[LidarNode]
        F[FusionNode]
        D[DecisionNode]
        M[MotorCtrlNode]
        H[HealthMonitor]
    end

    subgraph obs["可观测性基础设施"]
        TR[Traces: TracerContext]
        ME[Metrics: shared_metrics()]
        LO[Logs: RingBuffer → JSON]
    end

    L & F & D & M & H --> TR
    L & F & D & M & H --> ME
    F --> LO

    TR --> LTTNG[LTTng (kernel)]
    ME --> PROM[Prometheus :9090]
    LO --> STDOUT[stdout JSON]
    
    style obs fill:#f3e5f5,stroke:#7b1fa2
```

> 可观测性是横切关注点——不参与数据流和控制流，但为两条流提供可视化。

## 三条支柱

| 支柱 | 回答的问题 | 实现 | 热路径开销 |
|------|-----------|------|:---:|
| **Traces** | "一次请求经过了哪些节点？每跳多久？" | `TracerContext` (thread_local) | ~50ns |
| **Metrics** | "IMU 频率稳定吗？P99 延迟在恶化吗？" | `shared_metrics()` (POSIX 共享内存) | ~10ns |
| **Logs** | "这次错误发生在哪个 trace 里？" | SPSC RingBuffer + 后台 JSON serializer | ~10ns |

### Traces

```cpp
TRACE_SCOPE(amr::trace::FUSION_TIMER);  // 进入 span，生成 trace_id
LOG_OBS(INFO, "fusion", "done", lat_us); // trace_id 自动注入 log event
// scope 退出 → 恢复父级 TracerContext
```

- 进程内（Fusion→Decision→MotorCtrl）trace_id 通过 thread_local 自动共享
- 跨进程（sensor→compute）不传播——DDS 无标准 propagator（设计决策，见 M8 分析）

### Metrics

- `shared_metrics()` 通过 `shm_open("/amr_metrics_registry")` 映射同一块共享内存
- 5 个进程（lidar/imu/camera/compute/health）共享同一套计数器
- 热路径只有 `std::atomic` store，无 syscall

### Logs

- 热路径：`LOG_OBS` → `RingBuffer::try_push()` → ~10ns 返回
- 后台线程：`RingBuffer::pop()` → JSON 序列化 → stdout
- 固定大小 LogEvent (~192B)，消息字段限制 64 字节

## Trace-Log 关联路径

```
Metrics 告警 P99 > 50ms at 14:03:22
  → Jaeger 搜索 14:03:20~14:03:25 的 trace
  → trace_id=0x3f2a, fusion::cluster_detect 花了 800ms
  → grep 日志 trace_id=0x3f2a
  → "LiDAR outlier range=655.35 at idx=127"
  → 根因：硬件瞬时异常 → 异常聚类 → 降级处理耗时暴增
```

## 依赖

| 依赖 | 说明 |
|------|------|
| `observability/ring_buffer.hpp` | 无锁 SPSC 环形缓冲区 |
| `observability/tracer.hpp` | TracerContext + ScopedSpan |
| `observability/metrics_registry.hpp` | POSIX 共享内存指标 |
| `observability/trace_points.hpp` | Span 名称注册表 |

## 参考

- [可观测性设计文档](../guides/07-observability-design.md) — 架构设计 + spdlog 对比
- [可观测性使用指南](../guides/08-observability-usage.md) — API + 注意事项
