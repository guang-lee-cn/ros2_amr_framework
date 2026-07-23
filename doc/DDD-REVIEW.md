# DDD 分层合理性审视

> 跳出既有设计，从第一性原理重新审视当前四层架构的必要性。

---

## 一、当前分层的实际问题

### 1.1 application/ 层太薄——形同虚设

```
当前 application/ 层全部代码：

PerceptionService::tick(dt)
  → lidar_.read()         // infrastructure call
  → tf_.transform_scan()   // infrastructure call
  → degradation_.evaluate() // domain call
  → kf_.predict()          // domain call

PerceptionService::fuse(level)
  → detector_.detect()     // domain call
  → tracker_.update()      // domain call
```

这不是"用例编排"——是**顺序函数调用**。`tick()` 本质是几个 domain 函数的顺序组合，没有任何条件分支、事务管理、或跨 aggregate 的协调逻辑。把 `tick()` 内联到 `FusionNode::timer_callback()` 中，逻辑完全等价，省掉一个 application 层。

**结论**：application/ 层 4 个 Service 类总共 ~300 行，全部可以合并到 domain/ 或 infrastructure/（取决于是否依赖 ISensor）。当前的 application/ 只是 DDD 教科书的教条映射，不是实际复杂度驱动的分层。

### 1.2 domain/ 和 application/ 不构成独立测试边界

```
当前：domain/（纯逻辑）和 application/（编排）都不依赖 ROS2

测试时：
  test_dbscan.cpp       → 直接测 ClusterDetector
  test_kalman_filter.cpp → 直接测 KalmanFilter2D
  test_tracker.cpp      → 直接测 MultiObjectTracker
  test_monitoring.cpp   → 直接测 HeartbeatAnalyzer

  test_sensor_hal.cpp   → 测 PerceptionService（需要注入 mock sensor）
                          ← 这是唯一区别：application/ 需要传感器接口
```

**发现**：domain/ 和 application/ 的测试隔离线不是"domain vs application"——而是"是否需要 ISensor 依赖注入"。KalmanFilter2D 不需要传感器 → 纯 domain 测试。PerceptionService 需要传感器 → 需要 mock。但这两个在编译期都没有 ROS2 依赖——它们的测试门槛相同。

### 1.3 真正重要的分层边界只有一个

**ROS2 依赖 vs 不依赖 ROS2。** 这是唯一影响测试启动成本、编译时间、和可移植性的边界。

```
当前四层中：
  domain/        — 不依赖 ROS2  ✅
  application/   — 不依赖 ROS2  ✅  ← 和 domain 同一阵营
  infrastructure/— 依赖 ROS2     ❌
  observability/ — 不依赖 ROS2  ✅  ← 和 domain 同一阵营
```

四个目录，两个阵营。那为什么要四个目录？

---

## 二、修正方案：两层 + 横切工具

```
include/ros2_robot_middleware/

core/                             ← 所有不依赖 ROS2 的代码
├── perception/
│   ├── cluster_detector.hpp      ← DBSCAN
│   ├── kalman_filter.hpp         ← EKF
│   ├── tracker.hpp               ← 多目标跟踪
│   ├── degradation_policy.hpp    ← 降级策略
│   ├── sensor_interface.hpp      ← ISensor<T> 接口定义
│   └── perception_pipeline.hpp   ← （原 PerceptionService 内联到这里）
├── planning/
│   ├── target_selector.hpp
│   ├── preempt_policy.hpp
│   └── astar_planner.hpp         ← 新增
├── execution/
│   ├── interpolator.hpp
│   └── pure_pursuit.hpp          ← 新增
├── monitoring/
│   ├── heartbeat_analyzer.hpp
│   └── recovery_policy.hpp
└── interfaces/
    ├── sensor_interface.hpp      ← ISensor<T>
    └── transform_provider.hpp    ← ITransformProvider

adapters/                         ← ROS2 适配层（唯一可 #include <rclcpp> 的层）
├── nodes/
│   ├── fusion_node.hpp/.cpp
│   ├── decision_node.hpp/.cpp
│   ├── motor_ctrl_node.hpp/.cpp
│   ├── health_monitor_node.hpp/.cpp
│   └── fleet_manager_node.hpp/.cpp
├── sensors/
│   ├── simulated_sensors.hpp
│   ├── sick_tim781_adapter.hpp
│   ├── bmi088_imu_adapter.hpp    ← 新增
│   └── sensor_factory.hpp
├── tf2/
│   └── tf2_transform_provider.hpp
├── prometheus/
│   └── prometheus_http_server.hpp/.cpp  ← 新增（从 health_monitor 拆分）
└── diagnostics/
    └── diagnostics_publisher.hpp        ← 新增

observability/                    ← 横切工具库
├── ring_buffer.hpp
├── metrics_registry.hpp
├── tracer.hpp
├── trace_points.hpp
├── log_event.hpp
├── log_worker.hpp
└── logging.hpp
```

### 为什么这样更好

**1. 分层数量从 4 → 2（+ 1 横切工具）。** 减少 50% 的认知负担。新加入的开发者只需理解一个规则："`core/` 不能 `#include <rclcpp>`，`adapters/` 可以。"

**2. application/ 的代码合并到 core/——不丢功能，只去冗余。** `PerceptionService::tick()` 变成一个自由函数 `perception_pipeline::tick()` 或直接作为 `PerceptionPipeline` 类放在 `core/perception/` 中。现有的测试 `test_sensor_hal.cpp` 无需改动——它测的仍然是"注入 mock sensor → 调用 pipeline → 断言输出"。

**3. adapters/ 按子域分组——不再是扁平的 10 个文件。** 当前 `infrastructure/` 下 10 个头文件平铺在一起——sensor、node、tf2 堆在一起。拆分后 `adapters/sensors/`、`adapters/nodes/`、`adapters/tf2/`——命名即文档。

**4. observability/ 保持独立——它是库，不是层。** 当前 position 不变。它被 `adapters/` 引用，不被 `core/` 引用——继续这样。

---

## 三、迁移计划

| 步骤 | 操作 | 影响范围 |
|:---:|------|------|
| 1 | `domain/` + `application/` → `core/` | 移动 12 个头文件，合并 PerceptionService 到 core/perception/ |
| 2 | `infrastructure/` → `adapters/` + 子目录 | 移动 10 个头文件 + 10 个 .cpp |
| 3 | `observability/` 保持不动 | — |
| 4 | 更新全部 #include 路径 | ~50 个文件 |
| 5 | 更新 CMakeLists.txt include 路径 | 2 行 |
| 6 | colcon build + 65 tests pass | — |

**风险**：低。改动是纯文件移动 + #include 路径更新。不涉及逻辑变更。

**收益**：分层更诚实——`core/` vs `adapters/` 的边界清晰表达"ROS2 依赖 = 测试门槛"这个唯一有工程意义的分层规则。

---

## 四、如果不动——当前分层是否可以接受？

**可以。** 当前四层没有造成实际的工程损害——编译是快的，测试是能跑的，新人是能理解的。DDD 分层只是一个"不是最优但能工作"的设计。上面的修正方案是 **nice to have**，不是 **must have**。

什么时候该做这个迁移？
- 项目增加了 5 个以上的新开发者 → 认知负担 `core/` vs `adapters/` 比四层低
- application/ 层开始膨胀到 >500 行 → 拆分有意义时
- 有外部开发者抱怨"不知道代码在哪" → 当前文件太多，导航困难

**当前阶段不做迁移的充分理由**：65 个测试通过、编译零 warning、迭代计划中有 6 个更高优先级的组件待开发。重构分层 → 收益是认知清晰度，代价是 50+ 文件的 include 路径更新。性价比不够。
