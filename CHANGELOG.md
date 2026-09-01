# Changelog


## [2.3.0] — 2026-09-01

### Added — B3 API 稳定性策略（微缩 REP）
- 三级稳定性等级（Stable/Growth/Internal）+ 变更流程 + 现有 API 基线
  （docs/design/20260901-b3-api-stability.md）
- CHANGELOG 纪律：Added/Changed/Deprecated/Removed 四节为接口变更唯一权威

### Deprecated — B3 弃用演练
-  重载——scenario 应经
  set_parameter 注入而非工厂参数。替代：create_lidar(cfg) +
  set_parameter("scenario")。移除：v2.4.0


### Fixed — 复审整改 R1-R5（度量诚实/安全默认/竞态/信任链）
- R1: initial capture 补 --ignore-errors（supervisor_node 以 0% 真正入分母）
- R2: kDefaultMinValidEchoes=50 单一事实源（真机 fail-safe 默认）
- R3: demo_grid_ 加锁 + 160KB 快照拷贝（P0-B 竞态根治 + TSAN 回归锁）
- R4: 重启超时 30s + 计数器原子写持久化 + P0-K 真话 + v2.2.0 tag 修复
- R5: compose 单 compute 服务 + entrypoint 直通名 + package.xml 补 8 依赖

## [2.2.0] — 2026-08-31（历史发布，带已知瑕疵）

- TODO: Fast-DDS XML profile field testing
- TODO: Multi-threaded executor for cancel-action test coverage

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
### Fixed — 三审 Wave 4 宣称对账与度量真化
- README 四处宣称对账：看门狗两分法（supervisor 真/health_monitor 修复
  后真）+ 告警如实标注未实现；:9091 仅插桩构建存在；fleet_multi 标注
  单机演示形态；部署节降级 roadmap
- **P0-I 覆盖率真分母**：lcov --initial 先捕获零覆盖文件（supervisor/
  health_monitor 等不再从分母消失）——数字下降但为真
- **P0-J 徽章新鲜度**：badge push 失败置红 + rebase 重试（曾静默 4 天）
- **版本三宇宙对齐**：package.xml 0.3.0 / CHANGELOG Unreleased / tag
  v0.1.0 → 统一 2.2.0
- toolkit Docker 修复：独立 quality cmake 块（引用已迁移的 test/ 目录）
  → colcon 一体化构建+测试

### Fixed — OTA 真签名（审计行动 §8.3-2）
- **恒真桩替换**：`ota_agent` 签名校验 `/*signature_valid=*/true` →
  ed25519 fail-closed 验签（`domain/ota/package_signer.hpp`，OpenSSL EVP）
- 信任模型：私钥离线签发/公钥设备烧录（`ota.public_key_pem`），
  签名随版本送达（`ota.target_signature`）；缺失/错误 → REJECTED_SIGNATURE
- 测试 +8：签名器 6 例（全反例）+ agent 3 条拒绝路径（槽位零触碰）

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

### 补遗（2026-09-01 更正）

**P0-I 更正**：v2.2.0 发布时的「真分母 86.7%」宣称——`--initial` 机制
当时实际未生效（root cause: initial 命令缺 `--ignore-errors`，被 `|| true`
吞掉），86.7% 的下降来自新增 test_health_restart 带来的 gcda，非零覆盖
文件进分母。机制已在 v2.3.0 真正修通（supervisor_node 以 0% 入列验证）。

**Wave 1-3 安全修复清单**（此前遗漏未入发布说明）：
- P0-A: 感知主链路 stack-use-after-return 根治（std::array 值语义）
- P0-C: health_monitor 重启序列异步化（死锁根治）
- P0-D: OTA 验签绑定镜像内容 {version, sha256, size}
- P0-E: 公钥构造期钉住（运行时不可替换）
- P0-F: install 排除私钥分发
- P0-L: motor_ctrl 独立形态 MultiThreadedExecutor（饿死修复）
- P1: goal_pose/motor goal NaN 入口校验

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
