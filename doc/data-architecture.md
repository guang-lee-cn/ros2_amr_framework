# 数据架构设计

## 数据流总图

```
传感器 → DDS → ISensor::read() → LidarScan(值拷贝) → TF2 变换 → DBSCAN → Tracker → PerceptionObjects → DDS
                                                                │           │
                                                                ▼           ▼
                                                           spdlog JSON   Prometheus Metrics
                                                           (stdout)      (:9090)
```

## 存储分层

| 层级 | 存储介质 | 数据类型 | 生命周期 | 容量 |
|------|---------|---------|---------|:---:|
| L0 - 内存缓存 | LidarScan 栈数组 (8KB) | 最新一帧 LiDAR | 单次 tick() | 8KB |
| L0 - 内存缓存 | CameraFrame 传感器内部 buffer (900KB) | 最新一帧图像 | 单次 read() | 900KB |
| L1 - 热路径日志 | spdlog async ring (8192 slots) | 结构化日志事件 | 消费后丢弃 | ~1.5MB |
| L2 - 本地日志 | stdout JSON (Docker logging driver 接管) | 持久化日志 | 容器生命周期 | Docker 轮转 |
| L3 - 时序指标 | Prometheus (外部 pull) | Counter/Gauge/Histogram | Prometheus 配置决定 | 外部 TSDB |
| L4 - DDS 缓存 | Fast-DDS History (depth=10) | 消息历史 | publisher 轮转 | 10 × 消息大小 |

## 核心数据模型

### LidarScan（值语义，线程安全）

```cpp
struct LidarScan {
    static constexpr int kMaxRanges = 2048;
    float ranges[2048];     // 栈数组，每次 read() 拷贝全部 8KB
    size_t range_count;
    float angle_min, angle_increment;
};
```

### TrackedObject（持久 ID）

```cpp
struct TrackedObject {
    float x, y, vx, vy;    // KF 状态
    int track_id;           // 跨帧持久 ID
    int age, miss_count;    // 生命周期管理
};
```

### PerceptionObjects（ROS2 消息）

```msg
std_msgs/Header header
Object[] objects          // Object: {string id, float32 x/y/z}
uint64 trace_id           // 可选：分布式 trace 传播（M8 deferred）
```

## 数据生命周期

| 数据类型 | 创建 | 消费 | 销毁 |
|---------|------|------|------|
| LidarScan | `ISensor::read()` 栈分配 | DBSCAN::detect() 读取 ranges 指针 | tick() 返回时自动析构 |
| CameraFrame | 传感器内部 `buf_` 分配 | tick() 中读取 data 指针（视图） | 下次 `read()` 覆盖 |
| PerceptionObjects | FusionNode `timer_callback` 栈构造 | DecisionNode `on_perception` 回调 | DDS 回调结束后 SharedPtr 析构 |
| LogEvent | spdlog async queue push | 后台线程格式化 + stdout write | 格式化后丢弃 |
| Prometheus metrics | `std::atomic` 写入 SHM | HealthMonitor `prometheus_metrics()` 读取 | 新值覆盖旧值，保留最新 |
| KF cov_[] | `reset()` 初始化 | predict → update 反复修改 | FusionNode 析构时释放 |

## 内存防溢出策略

| 场景 | 策略 |
|------|------|
| LidarScan ranges 超过 kMaxRanges | 截断写入，`range_count = min(actual, kMaxRanges)` |
| spdlog 异步队列满 (8192 slots) | `async_overflow_policy::block` — 丢弃旧消息 |
| Prometheus histogram 超出 64 buckets | 最后一个 bucket 覆盖 `[2^63, +∞)` |
| Tracker 超过 max_tracks=20 | 新的检测被丢弃，不创建新 track |
| FleetManager 超过 kMaxAmrs=3 | 新的 AMR 心跳被忽略 |
