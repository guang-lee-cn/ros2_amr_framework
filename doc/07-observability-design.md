# M7：可观测性系统设计 — Traces / Metrics / Logs

> 状态：实施中
> 目标：建立端到端的可观测性体系，热路径延迟 < 50ns，独立于被观测系统

---

## 一、核心设计原则

1. **热路径零分配**：IMU/Lidar callback 内不做 malloc、不做 syscall、不做字符串操作
2. **观测通道独立**：Traces/Metrics/Logs 不与 DDS 共享通道——DDS 挂了不影响观测
3. **嵌入而非独立**：观测代码嵌入 Node 内部，不建独立 Node 传输观测数据
4. **优先复用 ROS2 生态**：不引入新的 SDK 依赖，LTTng 和 Prometheus 都是 ROS2 已集成的方案

---

## 二、技术选型

| 支柱 | 选择 | 理由 |
|------|------|------|
| **Traces** | **LTTng** | ROS2 原生集成（`ros2 trace`），内核级追踪，tracepoint 开销 ~ns 级 |
| **Metrics** | **Prometheus** + `std::atomic` | 复用 ADR-4 HTTP 端点，热路径原子操作 ~10ns |
| **Logs** | **SPSC Ring Buffer**（binary）+ 后台序列化 | 热路径无锁 push ~10ns，JSON 序列化推到后台线程 |
| **Trace 传播** | LTTng session ID | 跨进程自动关联，无需 DDS 消息内传 W3C header |

### 为什么不选 OpenTelemetry

- OTel C++ SDK 有内存分配和线程模型开销
- ROS2 已内建 LTTng tracepoint，rclcpp 层已埋点，无需额外集成
- AMR 是实时嵌入式系统，不是云原生微服务——选型逻辑不同

---

## 三、架构

```
每个 Node 内部：

  实时路径（callback）                非实时路径（后台线程）
  ┌────────────────────┐           ┌──────────────────────┐
  │  业务逻辑            │           │  ring_buffer::pop()   │
  │    │                │           │    │                  │
  │    ├─ LTTng trace_  │           │    ├─ JSON serialize  │
  │    │  point() ~ns   │           │    └─ stdout / file   │
  │    │                │           │                       │
  │    ├─ counter.fetch_ │          │  metrics_loop()       │
  │    │  add(1) ~10ns   │          │    └─ Prometheus gauge│
  │    │                │           │       push (HTTP)     │
  │    └─ ring.push()   │           │                       │
  │       ~10ns          │           │  trace session        │
  └────────────────────┘           │    └─ lttng session    │
                                     │       daemon write    │
                                     └──────────────────────┘
```

**关键隔离**：实时路径只做原子操作和无锁 push，所有 I/O 和序列化在后台线程完成。DDS 故障、磁盘满、Prometheus 拉取超时——都不影响 callback 延迟。

---

## 四、实现计划

### M7.1：基础设施

- [ ] `RingBuffer<T>` — 无锁 SPSC，支持多个 producer 一个 consumer
- [ ] `Tracer` — LTTng tracepoint 封装，`TRACE_SCOPE("fusion::process")` 宏
- [ ] `MetricsRegistry` — 基于 `std::atomic` 的 counter/gauge/histogram
- [ ] `LogWorker` — 后台线程，从 ring buffer 取数据，序列化 JSON，写 stdout

### M7.2：Traces — LTTng 全链路

- [x] 每个 Node callback 加 span tracepoint
- [x] Fusion → Decision → MotorCtrl 父子 span 关联（利用 LTTng 进程内上下文）
- [x] 跨进程 trace 通过 LTTng session ID 自动关联

### M7.3：Metrics — Prometheus 指标

- [x] `amr_sensor_rate_hz` (Gauge) — 传感器实际频率
- [x] `amr_fusion_latency_us` (Histogram) — 融合耗时分布
- [x] `amr_e2e_latency_us` (Histogram) — 端到端延迟
- [x] `amr_degradation_level` (Gauge) — 当前降级等级
- [x] `amr_degradation_events_total` (Counter) — 降级事件累计

### M7.4：Logs — 结构化日志

- [x] 热路径：`LOG_EVENT("fusion_cycle", {latency, object_count})` → ring buffer push
- [x] 后台线程：ring buffer pop → JSON serialize → stdout
- [x] JSON 格式包含 trace_id 用于关联

### M7.5：集成验证

- [x] 启动仿真 → 运行 5 分钟 → `ros2 trace` 有完整调用链
- [x] Prometheus 端点暴露 Histogram 延迟数据
- [x] Ctrl+C 后 ring buffer 不丢数据

---

## 五、自研 Ring Buffer 日志 vs spdlog

### 性能对比

```
单次日志调用开销（x86_64，微基准）：

方案                              热路径延迟    内存分配    syscall
─────────────────────────────────────────────────────────────────
自研 ring buffer (try_push)       ~10-15ns      无         无
spdlog async (thread_pool)        ~50-100ns     偶尔       无
spdlog sync (rotating_file)       ~1-5μs        偶尔       有 (write)
ROS2 RCLCPP_INFO                  ~2-8μs        有         有
```

自研方案快约 5-10 倍，但差距来自**做的事不同**——我们把格式化和 I/O 全部推到后台线程：

```
我们的热路径：     memcpy(LogEvent) → atomic store → 返回
spdlog 热路径：    fmtlib 解析格式串 → 生成时间戳 → 队列 push → 返回
```

spdlog 多出来的 40-90ns 主要花在格式串解析和时间戳生成上——这两个操作在 async 模式下也在调用线程完成。

### spdlog 有而我们没有的

| 能力 | 自研 | spdlog |
|------|:---:|:---:|
| 格式字符串（`"hello {}"_fmt`） | ❌ 固定字段 | ✅ fmtlib |
| 多 sink（文件+控制台+网络） | ❌ 只 stdout | ✅ |
| 日志轮转（按大小/时间切文件） | ❌ | ✅ |
| 日志级别过滤（编译期/运行时） | ❌ | ✅ |
| 多生产者-多消费者安全 | ❌ SPSC only | ✅ MPMC |
| 自定义格式模式 | ❌ 固定 JSON | ✅ |
| 彩色终端输出 | ❌ | ✅ |

### 为什么选了自研而非 spdlog

**对 AMR 项目的实时路径，自研方案是正确的选择，但不是因为速度。**

1. **延迟确定性**：格式解析 + 时间戳生成的开销是变量的——不同格式串、不同字段数，耗时不同。ring buffer 的 memcpy + store 延迟是固定的，与日志内容无关
2. **架构意志**：ring buffer 的设计表达了"日志不能阻塞实时路径"的立场。面试官看到的是你理解这个 trade-off，而不是"我装了 spdlog"
3. **零依赖**：不引入额外第三方库，与项目的 C++17 + ROS2 生态无缝集成

### 如果进生产

会换 spdlog async 模式 + 自定义 JSON formatter，原因：

```
- 日志轮转是运维必需品，不是加分项
- 多 sink（文件 + stdout + 网络）是分布式系统的标配
- spdlog async 的 100ns 开销在 AMR 场景下完全可接受
                      （10ms IMU 周期的 0.001%）
- fmtlib 是 C++20 std::format 的基础，未来可直接切换标准库
```

### 面试话术

> "我们自研了 ring buffer + 后台线程的日志方案，热路径 ~10ns，延迟确定。不是因为 spdlog 不够快——spdlog async 模式 ~100ns 对 AMR 也完全够用——而是想展示对实时系统中日志 I/O 可能成为抖动源的意识。ring buffer 把格式化和 I/O 完全推到后台，热路径只有一次 memcpy 和一次原子 store。如果进生产我会选 spdlog async + 自定义 formatter 输出 JSON，兼顾工程完备性和实时性能。"
