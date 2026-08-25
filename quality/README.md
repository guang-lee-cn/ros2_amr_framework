# Quality Engineering

## Quick Run

```bash
# Full gate: coverage build → test → coverage report → gate
./quality.sh

# ASan + UBSan + LSan (memory errors + UB + leaks, DDS noise suppressed)
./quality.sh asan

# Run tests only (coverage mode, no gate check)
./scripts/run_tests.sh coverage

# Static analysis (cppcheck)
./scripts/static_analysis.sh
```

## Test Modules（261 cases, 32 modules — 2026-08-25 实况）

模块清单的**唯一权威源是 CMakeLists 的 `add_amr_test` 注册表**（此处不再
维护易腐的复制表——2026-08-25 外部审计 P1-a：数字腐烂源于多处手抄）。
快速对账：

```bash
grep -c 'add_amr_test' CMakeLists.txt          # 模块数
grep -h -c '^TEST' quality/src/test_*.cpp | paste -sd+ | bc   # 用例数
```

代表性模块（完整清单见 CMakeLists）：

| Module | Description |
|--------|-------------|
| `test_supervisor_policy` | B1 监管状态机：拓扑序/退避/预算→FATAL/级联让位 |
| `test_control_loop` | 决策-执行闭环集成（A*→平滑→PurePursuit→误差自纠） |
| `test_kalman_filter` | Joseph 协方差对称 1e-12、Mahalanobis 拒绝一致性 |
| `test_monitoring` | HeartbeatAnalyzer, RecoveryPolicy, MonitoringService |
| `test_sensor_hal` | HAL 集成（Simulated sensors + Registry） |

## Coverage

```bash
./quality.sh          # 构建→测试→覆盖率→门禁（lcov 失败=门禁红，不放行）
./quality.sh html     # 同上 + HTML 报告
```

覆盖率数据在 `quality/data/`（口径：coverage_report.py 从 lcov .info 直算，
CI 归档 .info 原始件可复算）：
- `coverage.txt` — 当前行覆盖 %
- `coverage.prev.txt` — 上次运行
- `coverage_full.txt` — 全文件明细（含最低文件排序）
- `badge.json` — shields.io endpoint（README 徽章数据源，CI 实测更新）
- `html/index.html` — 逐行可视化（`./quality.sh html`）

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
