# CLAUDE.md — 本仓库 AI 协作规则

> 给 AI 编码助手的先验。生成/修改本仓库代码前先读完本文件；规则与人类开发者的
> review 清单同源（治理 AI 与治理人用同一套体系，见 docs/design 各 ADR）。

## 项目定位

ROS2 Jazzy 的 AMR 中间件参考架构（DDD 四层：HAL / Domain / Infrastructure /
observability）。C++17 · colcon · gtest · cppcheck 门禁 · 双 RMW CI 矩阵。

## 分层红线

- `domain/` 代码**零 ROS2 头文件**：不 include rclcpp/rosidl；时间戳用裸
  `int64_t stamp_ns`，时钟由 Infrastructure 层持有并注入。
- Infrastructure 层持时钟、ROS 句柄、DDS 配置；Domain 只收裸类型。
- 新传感器/执行器走 HAL 的 CRTP 模板（`SensorBase<Derived,T>` /
  `IActuator<Cmd,Fb>`），不自创抽象；ros2_control 插件放 `hal/ros2_control/`。

## 时间戳（强制，ADR：docs/design/20260824-timestamp-policy-adr.md）

- 打戳只在**最早可得点**；驱动侧透传上游 `header.stamp`，
  **禁止用 `this->now()` 覆盖真值**。
- HAL 结构体 `stamp_ns == 0` 视为未盖章；消费端过 `StampGate`（stamp==0 或
  超容差 = 保守拒绝）。新数据通道必须过门控，不允许"手里最新当现在"。

## 零拷贝

- compute 容器内热路径发布一律 `std::unique_ptr`（fusion/decision 已示范，
  见 src/infrastructure/compute_container.cpp 头部注释）。
- 传感器→fusion 的跨进程 DDS 是**故意保留**（故障隔离），不要"优化"掉。

## 节点形态

- **新节点继承 `amr::infrastructure::AmrNode`**（心跳/QoS/门控/时钟由基类
  托管），参照实现见 `imu_node`；参数在 `on_configure` 声明。
  ⚠️ 指标字段是封闭集合（MetricsRegistry 现有维度），新传感器没有现成
  per-sensor 字段——别等基类给你指标（D1 实验实证的过度承诺）。
- **消费方节点**（订阅传感器 topic 做处理/告警的形态）参照
  `temperature_monitor_node`（D1 二测范本：sensor_stream 订阅 + reliable_stream
  发布 + 滚动均值）；发布方参照 `imu_node`/`temperature_node`。
- **独立可执行节点**要进 CMake 的 `INDEPENDENT_NODES` 列表（一行 +
  `src/infrastructure/<name>_main.cpp`）；进 compute 容器才是「注册表一行」。
- **传感器注册的两条路径**（registry.hpp 头注释）：静态库下自注册宏会被
  链接器丢弃——可靠路径是幂等 `ensure_xxx_registered()` 在消费节点里显式
  调用，参照 `SimulatedUltrasonic`/`UltrasonicNode`（D1 验收全程范本）。
- `config/sensors.yaml` 是**参数速查文档，不随 launch 加载**——实际接线是
  ROS 参数（`sensors.lidar.type` 等），由各节点 declare_parameter。
- **QoS 一律 `amr::qos::` 词汇表**（sensor_stream / reliable_stream /
  latched_state / control_stream），**禁止手搓 `rclcpp::QoS(10)` 散设**。
- compute 容器组合由 `pipeline.nodes` 参数声明；新节点入管线 = 注册表一行 +
  配置一个名字，不改 main() 逻辑。

## 测试与 CI

- 新 Domain 类必须配 `quality/src/test_*.cpp` 并在 CMake 的
  `add_amr_test` 注册，无单测不合并。
- 基准数据 `benchmarks/results/*.jsonl` **只读**——不改历史数据，复跑存新文件。
- 编排脚本入口必须清场（pkill 残留进程）；shell 一律 `set -o pipefail`，
  **不用 `set -u`**（与 ROS setup.bash 的未定义变量冲突）。

## 文档与提交

- ADR/复盘 → `docs/design/YYYYMMDD-主题.md`；`mdDoc/` 是本地笔记（已 gitignore）。
- 提交信息：中文 conventional 格式（`feat/fix/chore(scope): 描述`）。

## 元防线（复审三连击教训）

- **修 bug 时 grep 同文件同类调用是否也需同修**——R1.1 的 --ignore-errors
  只加给了 runtime 侧、initial 侧同样问题没查前例
- **把别处的建议具体化为机制断言时，验证必须升级到命令级**——S-2 引用
  的 supervisor 订阅机制不存在（grep -c create_subscription = 0 即可避免）

## 禁止清单

- ❌ Domain 层出现任何 ROS 头文件
- ❌ 用 `now()` 覆盖上游时间戳真值
- ❌ 绕过 AmrNode/SensorBase 模板手搓平行实现
- ❌ 手搓 `rclcpp::QoS(10)`——必须走 `amr::qos::` 词汇表
- ❌ 修改 `benchmarks/results/` 历史数据
- ❌ 在编排脚本里 `pkill -f` 匹配含自身命令行的模式（用 `pkill -x` 或方括号技巧）
