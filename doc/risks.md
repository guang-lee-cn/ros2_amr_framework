# 风险与应对方案

## 架构风险

| 风险 | 影响 | 规避方案 | 应急预案 |
|------|:---:|------|------|
| DDS 通信延迟超标 (>10ms) | 融合管线输出延迟增大，决策滞后 | Fast-DDS XML QoS per-topic 调优；SHM 传输替代 UDP | 降级到 SingleThreadedExecutor 减少线程切换 |
| 内存泄漏 | 长期运行（>72h）内存持续增长 | 智能指针（unique_ptr/shared_ptr）；CI 集成 ASan | `heaptrack` 运行时分析；重启进程（HealthMonitor 看门狗） |
| DDS 消息丢失（UDP 不可靠） | IMU 数据缺失导致 KF 发散 | IMU topic 使用 reliable QoS；best_effort 仅用于可容忍丢失的数据（Camera） | 降级策略：传感器超时 → 自动降级到 NO_IMU 状态 |
| 日志阻塞实时线程 | IMU 100Hz 回调延迟尖刺 | spdlog async 线程池（1 线程，8192 队列）；热路径不调 write() | 队列满时 spdlog `async_overflow_policy::block` → 丢弃旧消息而非阻塞 |
| 进程间共享内存残留（异常退出） | 重启后读到旧 metrics 数据 | launch 脚本启动前 `rm -f /dev/shm/amr_metrics_registry` | 手动清理 `rm /dev/shm/amr_metrics_registry` |

## 技术风险

| 风险 | 影响 | 规避方案 | 应急预案 |
|------|:---:|------|------|
| Fast-DDS 版本兼容性 | ROS2 Jazzy 升级后 ABI 变化 | 固定 RMW 版本；CI 构建覆盖目标 ROS2 发行版 | 切换 RMW 实现（`RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`） |
| spdlog / fmt 版本冲突 | 链接错误（`DSO missing`） | CMake `find_package(spdlog REQUIRED)` + 显式 `target_link_libraries(... fmt::fmt)` | 回退到自研 RingBuffer（`Logging::init()` 保留） |
| 编译器 ABI 不兼容 | CI 和开发机不一致 | GitHub Actions 固定 `ubuntu-24.04` + `ros:jazzy-ros-base` 容器 | 本地 Docker 构建验证 |
| LifecycleNode 隐式转换失败 | `dynamic_cast` 或 `static_cast` 编译错误 | 使用 LifecycleNode 的 `get_clock()` / `get_node_base_interface()` 等访问器，不依赖 `rclcpp::Node` 继承 | — |

## 业务风险

| 风险 | 影响 | 规避方案 | 应急预案 |
|------|:---:|------|------|
| 传感器频率超出设计上限（>100Hz IMU） | 回调队列积压，延迟上升 | 每个传感器独立进程 + 独立 CPU 核绑定（`taskset`） | 降采样（`throttle` QoS）或丢弃旧帧 |
| 多物体密集场景 (>10 objects) | DBSCAN 计算耗时 >1ms | DBSCAN O(n²) 对 360 点远未触达上限，500+ 点才需考虑空间索引 | 限制 `max_objects=10`，超出裁剪 |
| RingBuffer 日志满（>4096 events） | 日志丢失 | `try_push` 返回 false → 静默丢弃；health_ 记录 dropped count | 增大 `kBufferSize` 常量；切换到 `spdlog` 的多 sink + 文件轮转 |
| 多 AMR 集群 DDS 域冲突 | 消息跨机器人串扰 | ROS2 namespace 隔离（`/amr1/`, `/amr2/`）+ DDS partition QoS | FleetManager 过滤异常的 HealthReport |
