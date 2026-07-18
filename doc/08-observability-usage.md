# 可观测性系统 — 使用指南与现状评估

---

## 一、使用指南

### 快速上手

```cpp
// 1. 在 main() 中启动日志后台线程（已集成在 compute_container 中）
#include "ros2_robot_middleware/observability/logging.hpp"
amr::observability::Logging::init();

// 2. 在 callback 中添加 trace span + metrics + log
#include "ros2_robot_middleware/observability/trace_points.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"

void FusionNode::timer_callback() {
    TRACE_SCOPE(amr::trace::FUSION_TIMER);           // 生成 trace_id，RAII 管理 span

    auto t0 = std::chrono::steady_clock::now();
    // ... 业务逻辑 ...

    auto &m = amr::observability::shared_metrics();
    m.fusion_latency.record(latency_us);             // Metrics: histogram
    m.fusion_cycle_count.fetch_add(1, relaxed);       // Metrics: counter

    LOG_OBS(INFO, "fusion", "cycle done", lat_us, obj_count);  // Logs: trace_id 自动注入
}

// 3. 程序退出前排空日志（已集成在 compute_container 中）
amr::observability::Logging::shutdown();
```

### 查看结果

```bash
# Metrics — Prometheus 端点
curl http://localhost:9090/metrics

# Logs — JSON 行输出到 stdout
./compute_container 2>&1 | grep '"mod":"fusion"'

# Traces — ros2 trace（需 LTTng session）
ros2 trace --session-name amr_session
```

### 新增 Span 的协作流程

1. 在 `trace_points.hpp` 中加一行 `constexpr`，按 `LAYER::ACTION` 命名规范
2. 代码中用 `TRACE_SCOPE(amr::trace::YOUR_NAME)` ——**禁止裸字符串**
3. Code review 时 grep `TRACE_SCOPE(`，裸字符串直接打回

---

## 二、注意事项

### 热路径约束

| 操作 | 开销 | 限制 |
|------|:---:|------|
| `TRACE_SCOPE()` | ~50ns（trace_id 生成 + RAII） | 随意使用 |
| `shared_metrics().xxx.store()` | ~10ns（原子操作） | 随意使用 |
| `LOG_OBS()` | ~10ns（ring buffer push） | **消息长度固定 64 字节，超长截断** |
| `LOG_OBS()` 后台线程 | ~100μs（JSON 序列化 + write） | 不在实时路径 |

### 不要做的事

- **不要在 callback 里调用 `std::cout`** — syscall 延迟不确定，可能阻塞实时线程。用 `LOG_OBS` 代替
- **不要用裸字符串调用 `TRACE_SCOPE`** — 必须使用 `trace_points.hpp` 中注册的符号
- **不要在高频路径（>1kHz）每周期打 log** — ring buffer 容量 4096，高频打满会导致日志丢失
- **不要在 `LOG_OBS` 中传动态字符串** — 消息字段是固定 64 字节数组，传入 `std::string` 会截断

### 跨进程注意事项

- `shared_metrics()` 通过 POSIX 共享内存 (`/amr_metrics_registry`) 跨进程共享
- 共享内存大小为 `sizeof(MetricsRegistry)` ≈ 4KB，无需配置
- 共享内存泄漏：进程异常退出时 `/dev/shm/amr_metrics_registry` 不会自动清理。调试期间可手动 `rm /dev/shm/amr_metrics_registry`
- Trace 跨进程不传播——sensor 进程和 compute_container 各自有独立的 trace_id

---

## 三、当前方案评估

### 优点

| 维度 | 评价 |
|------|------|
| **热路径性能** | **优秀**。三条路径（trace/metrics/log）均无 malloc、无 syscall，总开销 <100ns |
| **延迟确定性** | **优秀**。ring buffer 的 push 延迟固定 ~10ns，与消息内容无关 |
| **跨进程 Metrics** | **良好**。POSIX 共享内存实现跨进程指标聚合，零 IPC 开销 |
| **Trace-Log 关联** | **良好**。`TRACE_SCOPE` 生成 trace_id → `LOG_OBS` 自动注入，Jaeger + grep 可串联 |
| **API 一致性** | **良好**。`trace_points.hpp` 注册表防止命名分裂 |
| **零外部依赖** | **良好**。不需要 OpenTelemetry SDK、不需要 Jaeger Collector |
| **编译期开关** | **良好**。`TRACE_SCOPE` / `LOG_OBS` 默认启用，`AMR_TRACING_ENABLED` 可关闭 |

### 缺点和限制

| 问题 | 影响 | 优先级 |
|------|------|:---:|
| **跨进程 Trace 不传播** | 传感器进程和 compute_container 各自独立 trace——无法追踪"IMU 数据 → Fusion → Decision → MotorCtrl"的完整链路 | 中 |
| **日志格式固定 JSON** | 不支持自定义字段、不支持多 sink。spdlog 的 sink 机制比我们完备得多 | 低 |
| **日志消息固定长度** | `LogEvent::message` 只有 64 字节，长消息被截断 | 低 |
| **无日志轮转** | stdout 输出依赖 systemd/journald/Docker logging driver 做轮转，自研方案没有文件轮转能力 | 低 |
| **Prometheus Histogram 桶的精度** | 当前桶边界为 2^n μs（1μs, 2μs, 4μs...），1ms 以内的精度尚可，1ms 以上分辨率粗糙 | 低 |
| **共享内存不自动清理** | 进程异常退出后 `/dev/shm/amr_metrics_registry` 残留，下次启动可能读到旧数据 | 低 |
| **无 Grafana Dashboard JSON** | 目前没有预置的 Grafana Dashboard 模板，需要手动配置 | 低 |

---

## 四、迭代计划

### M7.x（当前迭代的尾巴，可选）

| 任务 | 工作量 | 描述 |
|------|:---:|------|
| 共享内存清理脚本 | 0.5h | `launch` 脚本启动前 `rm -f /dev/shm/amr_metrics_registry` |
| Grafana Dashboard JSON | 1h | 预置一个 dashboard 模板，导出一份 JSON 放入 config/ |

### M8（分布式 Trace 传播，下个大迭代）

| 任务 | 工作量 | 描述 |
|------|:---:|------|
| DDS 消息头注入 trace_id | 2d | 在 `PerceptionObjects` 等核心消息中增加 `trace_parent` 字段，或利用 Fast-DDS QoS user_data |
| 子 trace 继续 | 1d | 消费者收到消息后，提取 trace_id → 设置到 `TracerContext` → 子 span 自动继承 |
| 跨进程验证 | 1d | 端到端测试：IMU 数据 → 完整 trace 在 Jaeger 中可见 |

### M9（生产化，远期）

| 任务 | 工作量 | 描述 |
|------|:---:|------|
| spdlog 替换自研 ring buffer | 1d | 保留 `TRACE_SCOPE` + `shared_metrics()`，日志层切换到 spdlog async + JSON formatter |
| 日志级别运行时过滤 | 0.5d | spdlog 原生支持 `set_level()` |
| 多 sink（文件 + stdout + 网络） | 0.5d | spdlog 原生支持 |
| HealthMonitor 测试覆盖 | 1d | 心跳监控 + 看门狗恢复的单测（依赖多进程，复杂度高） |
| FleetManager 测试覆盖 | 1d | 多 AMR 集群的单测 |

### 不做的事

| 条目 | 理由 |
|------|------|
| 自研分布式追踪后端 | Jaeger 已足够 |
| 自研时序数据库 | Prometheus 已足够 |
| OpenTelemetry SDK 全量集成 | 热路径开销不可接受（OTel 有内存分配），且 ROS2 原生 LTTng 更匹配 |
| 完整的 APM 平台 | 超出个人项目范围 |
