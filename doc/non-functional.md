# 非功能需求与架构约束

## 性能

### 延迟基线

| 指标 | P50 | P99 | 测量方式 |
|------|:---:|:---:|------|
| 传感器→Fusion 输入延迟 | <1ms (DDS SHM) | <5ms | `ros2 topic delay` |
| Fusion::tick() 端到端 | ~2ms | ~8ms | Prometheus `amr_fusion_latency_seconds` |
| Sensor→cmd_vel 总延迟 | ~5ms | ~12ms | Prometheus `amr_e2e_latency_seconds` |
| 热路径单次 trace/metrics/log | <100ns | — | 微基准（`std::atomic` store + SPSC push） |

### 吞吐

| 资源 | 限制 | 说明 |
|------|:---:|------|
| LiDAR | 10Hz (100ms cycle) | SICK TiM781 标称频率 |
| IMU | 100Hz (10ms cycle) | BMI088 标称频率 |
| Camera | 5Hz (200ms cycle) | RealSense D435 标称频率 |
| Fusion tick | 200ms | 与最慢传感器对齐 |
| Decision | 事件驱动 | 受 Fusion publish 频率限制 |
| MotorCtrl | 10Hz | 插值步进循环 |

### CPU / 内存

| 进程 | CPU (idle) | CPU (peak) | 内存 (RSS) |
|------|:---:|:---:|:---:|
| lidar_node | <1% | ~3% | ~50MB |
| imu_node | <1% | ~1% | ~40MB |
| camera_node | <1% | ~5% | ~80MB |
| compute_container | ~2% | ~15% | ~120MB |
| health_monitor | <1% | ~1% | ~40MB |
| fleet_manager | <1% | ~1% | ~40MB |

> WSL2 x86_64 实测。ARM64 (RK3588) 预期 CPU +20%，内存 -30%（无桌面环境）。

---

## 可靠性

| 机制 | 实现 | 恢复时间 |
|------|------|:---:|
| 传感器降级 | DegradationPolicy (FULL→NO_IMU→CRITICAL→FATAL) | 实时评估 |
| 看门狗重启 | HealthMonitor + lifecycle ChangeState | <2s |
| 进程隔离 | 6 个独立进程，传感器 crash 不扩散 | — |
| 最大重试 | RecoveryPolicy::kMaxRetries=3 | 失败后标记 FATAL |
| DDS 消息丢失 | Fast-DDS reliable QoS（IMU/Perception） | 自动重传 |
| 内存泄漏检测 | CI + ASan (planned) | — |

---

## 实时性

| 保障 | 说明 |
|------|------|
| 热路径零分配 | `LOG_OBS` / `TRACE_SCOPE` / `shared_metrics()` 无 `malloc`、无 `syscall` |
| 日志异步 | spdlog async thread pool，不阻塞 IMU 100Hz 回调 |
| 数据拷贝最小化 | Lidar 值拷贝 (8KB)，Camera 视图模式 (零拷贝) |
| 执行器策略 | compute_container: MultiThreadedExecutor，sensor: SingleThreadedExecutor |
| DDS SHM | 进程内共享内存传输，避免 UDP 序列化开销 |

---

## 可观测性

| 支柱 | 实现 | 热路径开销 |
|------|------|:---:|
| Traces | TracerContext (thread_local) + trace_id 注入 LOG_OBS | ~50ns |
| Metrics | `std::atomic` + POSIX 共享内存 + Prometheus HTTP endpoint | ~10ns |
| Logs | spdlog async (1 thread, 8192 queue) + JSON pattern | ~100ns |

三条支柱通过 trace_id 关联。Jaeger + Grafana + grep 完成联合分析。

详见 [guides/07-observability-design.md](guides/07-observability-design.md) [guides/08-observability-usage.md](guides/08-observability-usage.md)

---

## 安全

| 层级 | 实现 |
|------|------|
| DDS 通信加密 | SROS2: per-node keystore + governance.p7s + permissions.xml |
| 进程隔离 | ROS2 Node 独立进程，cgroup 资源限制 (Docker) |
| 输入校验 | Mahalanobis 3σ 离群值拒绝 (KF)；DBSCAN 噪点标记 |
| 内存安全 | 智能指针（unique_ptr/shared_ptr），CRTP 编译期类型检查 |

---

## 可扩展性

| 维度 | 实现 |
|------|------|
| 传感器 | YAML 驱动 `SensorFactory`，新增传感器 = 新增 Adapter 类 + 1 个 `if` 分支 |
| 多 AMR | FleetManager 订阅各 AMR namespace 的健康报告，支持 kMaxAmrs=3 |
| DDD 分层 | 新增域逻辑不影响通信层；换 RMW (Fast→Cyclone) 无需改动 domain 代码 |
| 构建 | `colcon build --packages-select` 只编译本项目 |

---

## 容量边界

| 边界 | 值 | 超限行为 |
|------|:---:|------|
| 最大传感器频率 | IMU 100Hz (10ms budget) | 回调积压 → 延迟上升 |
| DDS 消息队列 | QoS depth=10 | 消息丢弃（best_effort）/ 阻塞（reliable） |
| RingBuffer log 容量 | 4096 events | 溢出事件丢弃（`try_push` 返回 false） |
| Prometheus histogram 桶 | 64 buckets (log scale) | 超出最大桶的事件计入 +Inf |
| 最大 AMR 数量 | 3 (FleetManager) | 需改 `kMaxAmrs` 常量 |
| 最大 Track 数量 | 20 (Tracker) | 新检测被丢弃 |
