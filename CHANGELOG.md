# Changelog

## [Unreleased]

### Added — D1 第三方扩展验收（迭代2）
- **「X 分钟接入」成为数据**：干净上下文代理两轮实测 19.5min/8 摩擦 →
  修复（宏 token-pasting bug、静态库注册路径文档化、6 处文档对账）→
  8m48s/2 摩擦一次全过（范围翻倍）
- 扩展范本入库：ultrasonic（发布方）+ temperature/temperature_monitor
  （消费方）全链（传感器/注册/节点/测试/构建注册）
- `AMR_REGISTER_SENSOR` 修复：__LINE__ 拼名 + used 属性（原 token 粘贴
  字符串字面量无法编译）；registry.hpp 确立两条注册路径

### Added — A5 断言式 e2e（审计行动④）
- **`test_e2e_behavior`**：scene_simulator 合成闭环（无 Gazebo，CI 容器可跑）
  + compute 管线同款组合；三场景断言——到点真停（cmd 静默+位移<0.1m）、
  遇障绕行不进物理碰撞盘、断源 3s 降级+恢复后到达
- SceneSimulatorNode：NodeOptions 构造 + `pause()/resume()` 故障注入 API

### Fixed — e2e 挖出的 4 个真 bug
- **PurePursuit 到点停不下**：固定 0.1 爬行 + 容差 0.05 < 路径量化误差 →
  死区边缘绕圈（实测到达后 0.275m/3s 漂移）；容差 0.10 + v≤dist 收敛
- **lidar_snapshot 丢戳**：StampGate 对适配器路径失明（旧帧冒充现在）
- **sick 适配器缓存帧判活缺失**：断源后降级永不触发；到达时间判活 1s 窗
- **fusion 降级冻结**：evaluate_degradation 在 stamp gate return 之后

### Added — B1 进程级 supervisor（迭代2）
- **`amr_supervisor`**：声明式配置驱动的崩溃监管/依赖序重启/健康门——
  posix_spawn 独立进程组、waitpid tick、指数退避、窗口化重启预算→FATAL、
  死源级联让位（逆拓扑）、HealthReport(latched) 状态出口
- `domain/monitoring/supervisor_policy.hpp`：策略内核纯逻辑（零 ROS），22 单测
- 集成实证：kill -9 compute → 级联恢复全程 **1.0s**（watchdog 全栈路径 2-4min）；
  预算耗尽 → FATAL + 级联停依赖者（见 change journal 2026-08-25）

### Added — A2 soak harness
- `scripts/soak_run.sh / soak_monitor.py / soak_report.py`：72h 长稳编排 +
  故障注入 + RSS/吞吐/恢复率报告；smoke 实证注入2/恢复2（MTTR 125-150s）

### Fixed — 测试
- **test_control_loop 补注册**：随 15b93b2 入库但漏 CMake 注册，从未编译/运行；
  适配现行 API（`amr::domain` 命名空间、uint8 代价场、`GridUpdater` 三参 Params），
  修复两处未运行而未暴露的问题（仿真起点偏离路径起点、包围圈量化缝隙），9 用例全绿
- 测试套件实况：**32 模块**（ctest 100% 通过），README 数据同步刷新

## [2.1.0] — 2026-07-31

### Added — 控制层自研闭环 (P3)
- **路径平滑** `path_smoother.hpp`：内切圆弧圆角（无 overshoot），直线密集采样，6 测试
- **跟踪误差监控** `track_error_monitor.hpp`：横向误差→降速/停止，接入 MotorCtrlNode，7 测试
- **动态避障重规划** `grid_updater.hpp`：感知→膨胀标记→A* 重规划→平滑，4 测试
- **商业部署方案** `deployment-plan.md`：Docker + OTA + 版本锁定

### Added — 可观测性增强
- **PerfRegistry → Prometheus**：compute_container :9091 端点暴露 AMR_PERF_PHASE 阶段延迟
- **Grafana dashboard** `config/grafana/amr_dashboard.json`：5 面板（阶段延迟/健康/降级）

### Changed — HAL 层重组 (P2)
- `amr::hal` 命名空间 + `hal/` 目录（sensor/actuator/common）
- **SensorFactory → Registry**：静态插件注册，加传感器零改框架
- metrics_registry → .cpp 拆分（POSIX 头移出 header）

### Changed — 融合层升级 (P1)
- **DBSCAN → PCL** 策略模式（`IClusterAlgorithm`），3.2x 加速
- **robot_localization EKF** 集成 → /odom 定位闭环
- MotorCtrlNode 开环→闭环（订阅 /odom）
- Camera 占位清理（删除 900KB 噪声生成）
- IActuator 接口 + 驱动接入指南

### Changed — 测试
- 15 模块 98 用例（新增 astar/pure_pursuit/path_smoother/track_error/grid_updater）
- test_motor_ctrl 从 CI 排除（spin_once 竞态待 P2 修复）

## [2.0.0] — 2026-07-18

### Added — M7: Observability System
- **Traces**: TracerContext (thread_local + atomic trace_id) for in-process span correlation
- **Trace Points**: 15 constexpr symbols in `trace_points.hpp` — single source of truth for span names
- **Metrics**: Prometheus endpoint (:9090/metrics) with 4 histogram tiers (fusion/decision/motor/e2e)
- **Metrics**: POSIX shared memory (`shm_open`) for cross-process counter aggregation
- **Logs**: Lock-free SPSC RingBuffer (~10ns push) + background JSON serializer
- **Grafana**: 8-panel dashboard JSON (`config/grafana_dashboard.json`)
- **Docs**: `doc/07-observability-design.md`, `doc/08-observability-usage.md`

### Added — ADR-6: EKF Upgrade
- Pluggable measurement models: LinearMeasurement (default) + RangeBearingMeasurement
- Compile-time template policy — zero runtime overhead
- `init_from_measurement()` avoids singular Jacobian at origin
- Joseph form covariance + Mahalanobis outlier rejection preserved

### Changed — DDD Refactoring
- 4-layer architecture: domain/application/infrastructure/observability
- Physical directory restructuring: headers in `include/.../infrastructure/`, source in `src/infrastructure/`
- `kalman_filter.hpp` moved to `domain/perception/`
- ADR-10: DDD directory layering decision record

### Changed — Test Suite
- Split from 1 monolithic file (13 cases) to 7 modules (48 cases)
- `AMR_TEST_ONLY()` macro for test-only instrumentation
- Injectable degradation timeouts: degradation tests from 8s → 1s
- Total suite: 37s → 2.8s
- New modules: test_observability, test_kalman_filter, test_monitoring

### Changed — ADR-7 Finalized
- Hybrid executor: MultiThreadedExecutor (compute_container) + SingleThreadedExecutor (sensors/monitor)
- P99 e2e latency reduced ~50% (25ms → 12ms)

### Changed — Launch
- `clean_shm` step removes stale `/dev/shm/amr_metrics_registry` on startup
- All 4 launch files updated

---

## [0.1.0] — 2026-06-17

### Added — Phase 9: Docker
- Multi-stage Dockerfile: builder (gcc/cmake/lcov), runtime (ros-core only), dev (with rqt)
- docker-compose.yml: 6 services, health checks, startup ordering (sensors → fusion → decision → motor_ctrl)
- `ipc: host` for cross-container DDS SHM transport
- Resource limits: 256MB / 0.5 CPU per container

### Changed — Phase 8: Integration Tests
- 9 GWT-pattern tests covering all 6 nodes (`NodeName_Given_Then` convention)
- GoogleTest once-per-suite fixture (DDS cross-test isolation is unreliable with per-test init)
- `test.sh` auto-detects lcov; `COVERAGE=1` enables branch-coverage instrumentation
- Coverage report output to `mdDoc/coverage/html/index.html`

### Added — Phase 7: Observability
- `RCLCPP_INFO_THROTTLE` logs on all 6 nodes (5s/10s throttle periods)
- rqt_graph screenshot saved to `mdDoc/rqt_result.png`

### Added — Phase 6: Pipeline
- Sensor layer: LidarNode (360° SICK TiM781), ImuNode (BMI088 noise), CameraNode (640×480 D435)
- Fusion layer: multi-sensor object extraction via lidar range clustering (<3m threshold)
- Decision layer: PerceptionObjects → MoveToPose action client
- Actuation layer: MotorCtrlNode with Action Server + SetParam Service
- 7 custom interfaces: 5 msg, 1 srv, 1 action
- `system.launch.py`: launches full 6-node pipeline

### Added — Phase 5: CI
- GitHub Actions workflow: build + test on push
- `colcon build` compiles all 6 nodes + static library

### Docs
- PRD (8 sections), Design Doc (topology + interfaces), Cost Estimation, ROS2 Guide, DDS Customization

## [0.2.0] — 2026-06-17

### Added — Phase 13: Health Monitoring + Prometheus
- `health_monitor_node`: heartbeat-based monitoring of 6 robot nodes
- New messages: `HealthStatus` (node_name/status/last_seen/timeout), `HealthReport` (Header + HealthStatus[])
- 1Hz heartbeat publishers on all 6 nodes (dedicated topics, std_msgs/String)
- Health check service at `/health/check` (SetParam request/response)
- Embedded Prometheus HTTP server on `:9090/metrics` (raw TCP socket, zero library deps)
- Prometheus gauge metrics: `ros2_node_health_seconds`, `ros2_node_timeout_seconds`
- Status classification: OK / WARN (80% timeout) / ERROR / STALE
- Per-node configurable timeout via ROS2 parameters
- Launch file and docker-compose updated to 7 services

### Fixed
- C++ name hiding bug: `HealthMonitorNode::create_publisher()` shadowed `rclcpp::Node::create_publisher<T>()` — fixed with qualified `rclcpp::Node::create_publisher<T>(...)` call

## [Unreleased]
- TODO: Fast-DDS XML profile field testing
- TODO: Multi-threaded executor for cancel-action test coverage
