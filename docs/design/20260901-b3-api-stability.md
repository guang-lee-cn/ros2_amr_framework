# API 稳定性策略（B3 · 微缩 REP · 2026-09-01）

> 迭代 2 B3：「amr::qos / AmrNode 版本化与弃用策略」
> 验收标准：CHANGELOG 驱动的接口变更记录 + 一条真实弃用演练

## 1. 稳定性等级

本仓库的公开 API 分三级（对标语义化版本 + REP 兼容性分级）：

| 等级 | 涵盖 | 变更规则 |
|------|------|---------|
| **Stable** | `amr::qos::*`（4 个工厂函数）、`AmrNode` 公开方法、`ISensor/IActuator` CRTP 接口、`OtaCoordinator::run_update`、`CollisionGuard::Params` | ** semver 语义**——签名变更 = breaking，须 major 版本 + CHANGELOG 弃用期 |
| **Growth** | `PerceptionService`、`AStarPlanner`、`PurePursuit`、`StampGate`、`SupervisorPolicy` | 小版本可加参数/字段（默认值兼容）；移除须一个 minor 周期的弃用警告 |
| **Internal** | 所有 `detail::`、私有成员、`test_hooks`（AMR_TEST_ONLY） | 无承诺，随时可变 |

## 2. 变更流程

### 2.1 添加（所有等级）
- 新参数/字段/方法：直接加，CHANGELOG 记录 `Added`
- 新命名空间/类：需同时更新 `docs/design/architecture-overview.md` 分层图

### 2.2 修改签名（Stable/Growth）
1. **弃用旧签名**：`[[deprecated("Use new_signature instead. Removal: vX.Y")]]`
2. **保留旧签名 ≥ 1 个 minor 周期**（编译可用但产生警告）
3. **移除**：下一个 major 版本
4. 每步在 CHANGELOG 记录：`Deprecated` → `Removed`

### 2.3 修改行为（不改签名）
- 语义变更（如默认值翻转 P0-G）：CHANGELOG `Changed` + 测试更新
- 行为 bug 修复：CHANGELOG `Fixed` + 回归测试

## 3. 现有 API 版本基线（2026-09-01 = v2.3.0）

### amr::qos（Stable，自 v0.1.0）
```cpp
inline rclcpp::QoS sensor_stream(std::size_t depth = 5);      // best_effort
inline rclcpp::QoS reliable_stream(std::size_t depth = 10);    // reliable
inline rclcpp::QoS latched_state();                            // reliable+transient_local
inline rclcpp::QoS control_stream();                           // reliable+depth 1
```
**承诺**：函数签名冻结；depth 参数可加新默认值；新增工厂函数不 breaking。

### AmrNode（Stable，自 v2.1.0）
```cpp
class AmrNode : public rclcpp_lifecycle::LifecycleNode {
  int64_t now_ns() const;
  template <typename MsgT> auto create_pub(topic, qos = reliable_stream());
  void start_heartbeat(topic, period = 1s);
  void stop_heartbeat();
  void register_stale_gate(sensor, stale_ms);
  bool fresh(sensor, stamp_ns);
  MetricsRegistry & metrics();
};
```
**承诺**：方法签名冻结；新横切面方法（如 `start_trace`）按 Growth 加。

### CollisionGuard::Params（Growth，自 v2.3.0）
```cpp
static constexpr int kDefaultMinValidEchoes = 50;  // 单一事实源
struct Params {
  float stop_dist, safe_dist, fov_half, range_max, crawl_speed;
  std::chrono::milliseconds stale_timeout;
  int min_valid_echoes = kDefaultMinValidEchoes;
};
```
**承诺**：新字段可加（带默认值）；`kDefaultMinValidEchoes` 值变更 = Changed。

## 4. 弃用演练（B3 验收标准）

### 演练场景：弃用 `sensor_factory.hpp` 中的 `create_lidar(cfg, scenario)` 重载

**理由**：scenario 参数仅在仿真场景有意义，真机路径不应看到此参数。
新代码应显式配置 scenario 参数而非依赖工厂函数重载。

**步骤**：
1. 加 `[[deprecated]]` 属性
2. 提供替代路径（已在 v2.3.0 存在：set_parameter("scenario")）
3. CHANGELOG 记录 `Deprecated`
4. 下一个 minor 周期移除

（实际代码标注见同 commit 的 sensor_factory.hpp 变更）

## 5. CHANGELOG 纪律

每次 API 变更**必须**出现在 CHANGELOG 对应节：
- `Added`：新 API
- `Changed`：行为变更（含默认值翻转）
- `Deprecated`：即将移除的 API + 替代方案 + 移除版本
- `Removed`：已移除的 API（major 版本）

CHANGELOG 是接口变更的**唯一权威记录**——代码注释、journal 不替代。
