# Quality Engineering

## Quick Run

```bash
# Full gate: coverage build → test → coverage report → gate
./quality.sh

# ASan + UBSan mode (memory errors + undefined behavior)
./quality.sh asan

# Run tests only (coverage mode, no gate check)
./scripts/run_tests.sh coverage

# Static analysis (cppcheck)
./scripts/static_analysis.sh
```

## Test Modules (52 cases, 8 modules)

| Module | Cases | Description |
|--------|:---:|-------------|
| `test_sensor_nodes` | 3 | Lidar/IMU/Camera data validation |
| `test_motor_ctrl` | 4 | Action server + SetParam service |
| `test_decision` | 1 | Perception→Goal dispatch |
| `test_fusion` | 3 | Detection + degradation (HAL-based) |
| `test_observability` | 11 | RingBuffer, MetricsRegistry, TraceContext, TracePoints |
| `test_kalman_filter` | 8 | Linear KF + EKF range-bearing + consistency |
| `test_monitoring` | 14 | HeartbeatAnalyzer, RecoveryPolicy, MonitoringService |
| `test_sensor_hal` | 8 | Simulated sensors + PerceptionService HAL integration |

## Coverage

```bash
./test.sh          # prints line coverage + delta vs previous run
```

Coverage data tracked in `test/coverage/`:
- `coverage.txt` — current line %
- `coverage.prev.txt` — previous run
- `coverage_full.txt` — per-file breakdown
- `html/index.html` — line-by-line visualization

## Writing New Tests

### Naming Convention

```
Module_GivenCondition_ExpectedBehavior
```

### Domain Layer Tests (no ROS2)

```cpp
TEST(KalmanFilterTest, Linear_ConvergesToConstantPosition) {
  KalmanFilter2D<> kf;
  for (int i = 0; i < 20; ++i) {
    kf.predict(0.1, 0.0, 0.0);
    kf.update(5.0, 10.0);
  }
  EXPECT_NEAR(kf.x(), 5.0, 0.2);
}
```

### Infrastructure Tests (with ROS2)

```cpp
TEST_F(SensorNodeTest, LidarNode_TimerFires_RangesInPhysicalBounds) {
  auto node = std::make_shared<LidarNode>();
  node->configure();
  node->activate();
  // ... subscribe, spin_until, assert ...
}
```

## Excluded from Coverage

| Pattern | Reason |
|---------|--------|
| `*_main.cpp` | Thin entry points (11 lines each) |
| `fleet_manager_*` | Requires multi-process integration |
| `compute_container.cpp` | Launch entry, not unit-testable |
| `observability/log_worker*` | Background thread, async I/O |
