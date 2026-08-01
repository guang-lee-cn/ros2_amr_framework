# M13：Trace 传播与关联设计 —— 消息携带双字段（trace_id + span_id）

> 状态：设计定稿（2026-08-01）
> 定位：对 [07-observability-design.md](07-observability-design.md)（M7）的**演进**，填补其明确缺口："跨进程：未实现"。
> 核心决策：**W3C Trace Context 双字段，消息携带传播，非 thread_local**。

---

## 一、背景：M7 的三个缺口

| 缺口 | 表现 | 本设计解法 |
|------|------|-----------|
| 跨进程未实现 | trace_id 是 thread_local，`/fusion`→`/decision` 跨进程断链 | trace_id 进消息，随 DDS 走，进程边界自然打通 |
| MultiThreadedExecutor 丢上下文 | 回调跳线程，thread_local context 丢失 | carrier 从 thread_local 换成消息（回调无共享状态） |
| 无 span 拓扑 | 只有 trace_id，无法表达扇出/扇入结构 | 加 span_id + parent_span_id，构成因果树 |

## 二、核心概念

- **trace**：一次完整工作单元（如 lidar scan 42 那一个周期）。全程同一 `trace_id`（16B，全局唯一）。
- **span**：trace 里的一段（如 fusion 的订阅 callback）。各 span 有自己的 `span_id`（8B）。
- **parent_span_id**：把 span 串成**树**，而非链——可表达一次发布 → 多订阅（扇出）。

```
trace_id = "lidar scan 42 周期"
fusion  callback:  span_id=A,  parent=null      ← 根
  │ 发布
decision callback: span_id=B,  parent_span_id=A
  │ 发布
motor   callback:  span_id=C,  parent_span_id=B
```

**ROS2 的自然 span 单位 = 订阅 callback**（与 ros2_tracing/CARET 对齐）。

## 三、设计

### 3.1 传播机制

- 内部管道消息（`/fusion`→`/decision`→`/motor`）加 `trace_id` + `span_id` 两个字段。
- 源头（fusion 收传感器）生成 trace_id；每跳回调：接收时提取、起新 span、发布时注入。
- 传感器边界用**天然键**进入：std 消息（`sensor_msgs`）无法加字段，fusion 以 `(stamp, seq)` 生成 trace_id——边界天然键，管道内传播。

### 3.2 诊断事件 Schema

```json
{
  "trace_id":     "1f2e3d4c5b6a7988",
  "span_id":      "0a0b0c0d0e0f0102",
  "parent_span_id":"0a0b0c0d0e0f0001",
  "instance":     "amr-01@robot1",
  "node":         "decision",
  "kind":         "cycle",
  "topic":        "/decision",
  "wall_ts":      1722500000123,
  "lat_us":       230,
  "level":        "INFO",
  "msg":          "replan triggered"
}
```

`instance`（robot_id@hostname）解决 fleet 下 node 名重复；`wall_ts` 同机共享 CLOCK_MONOTONIC（µs），跨机 NTP（S 级）。

## 四、线程安全（按构造成立）

关联状态**没有共享可变实例**：

| 状态 | 载体 | 依据 |
|------|------|------|
| trace_id / parent_span_id | 消息字段（发布后不可变） | 只读，无锁 |
| 当前 span_id | callback 局部变量 | 每回调一份 |
| span_id 生成 | `std::atomic` 计数器 | 原子无锁 |

```cpp
// 订阅回调内（任意线程，MultiThreadedExecutor 安全）
auto ctx = extract_trace(msg);        // 只读消息字段
uint64_t my_span = next_span_id();    // atomic fetch_add
log_event({.trace_id = ctx.trace_id,
           .span_id = my_span,
           .parent_span_id = ctx.span_id, ...});
// 发布：写入正在构造的消息
out.trace_id = ctx.trace_id;
out.span_id  = my_span;
```

**禁用**：全局可变 current-context（数据竞争）；依赖 thread_local 跨线程（上下文丢失，非竞态）。

## 五、性能（每跳）

| 项 | 成本 |
|----|------|
| payload +24B（trace_id 16 + span_id 8） | 对 KB 级消息可忽略 |
| span_id 生成 | atomic fetch_add ~20ns |
| 提取/注入 | 两次 24B 拷贝，<1µs |
| 日志 push | 已有无锁环形缓冲，无变化 |
| 锁 / 系统调用 / 网络 | 热路径零 |

每跳亚微秒级；受限于日志频率而非消息频率（100Hz×4 节点 = 400 事件/s，后台线程零压力）。**比 OTel SDK 更省**——不背 span 对象分配、context 栈、导出器批处理、偶发锁竞争。

可选优化：图像流不进因果链，可不加字段。

## 六、改动面

| 文件 | 改动 |
|------|------|
| 内部管道 msg 定义 | 加 `trace_id`/`span_id` 字段 |
| [tracer.hpp](../../include/ros2_robot_middleware/observability/tracer.hpp) | thread_local 方案 → 消息注入/提取 |
| 各 node 发布/订阅 | 发布注入、接收提取 + 起 span（每跳 1-2 行） |
| [log_worker.hpp](../../include/ros2_robot_middleware/observability/log_worker.hpp) | JSON schema 加 trace_id/span_id/parent_span_id |
| [metrics_registry.hpp](../../include/ros2_robot_middleware/observability/metrics_registry.hpp) | 收缩为热指标（rate/degradation_level），计数类改由事件流聚合 |
| health_monitor | 心跳/恢复事件落流；trace 间隙可做重启检测 |
| 新增 | 事件采集（本地文件）+ DuckDB 分析层 + 故障查询模板 |

## 七、自研 vs 开源

| 层 | 自研（轻） | 开源（成熟） |
|----|-----------|-------------|
| 传播 | msg 双字段注入/提取 | ros-opentelemetry（车队/接标准后端时） |
| 事件流 | 统一 schema + 本地采集 | Loki + Promtail |
| 存储 | — | DuckDB（离线）/ Loki（在线） |
| 指标 | shm 热指标 + 导出 | Prometheus + Grafana |
| 内核时间线 | — | ros2_tracing (LTTng) 兜底 |
| 后分析 | 故障查询模板（DuckDB SQL） | ROSQL（车队/OTel 栈） |

指标关联（exemplar）：**单机砍，车队补**——避开 Prometheus scrape 丢 exemplar 与 trace_id 高基数两个已知坑。

## 八、边界

> **因果 = 消息边界**：消息到哪，trace_id 到哪。线程/进程/主机全程覆盖。
> **时间对齐**：同机 µs / 跨机 S 级（NTP）。
> **内核调度**：用户态事件看不到，LTTng 兜底。
> **外部系统**（云/DB/UI）：传播止步，时间+维度兜底。
> **车队**：node 名重复，必须带 `instance` 维度。

天然适用 `/scan→/fusion→/decision→/motor` 消息链；链外（health_monitor/看门狗）用时间+维度即可。

## 九、与 M7 的关系

| M7 项 | 处理 |
|-------|------|
| TracerContext (thread_local) | 替换为消息携带双字段 |
| 热路径零分配 / 无 syscall | 保留（新方案满足） |
| Prometheus + shm 指标 | 保留，收缩为热指标 |
| SPSC Ring Buffer 日志 | 保留，schema 加字段 |
| "跨进程：未实现" | **本设计闭合该缺口** |
