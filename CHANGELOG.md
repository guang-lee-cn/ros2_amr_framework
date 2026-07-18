# Changelog

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
