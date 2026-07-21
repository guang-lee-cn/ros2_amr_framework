# 迭代计划

> 以 14 章架构设计说明书为牵引，逐章盘点现状与缺口，规划迭代。

---

## 总览

| 章 | 章节名 | 现状 | 优先级 |
|:---:|------|:---:|:---:|
| 一 | 基础元信息 | ✅ ARCHITECTURE 开篇 | P1 |
| 二 | 业务与需求背景 | ✅ requirements.md | P2 |
| 三 | 系统边界与上下文 | ✅ ARCHITECTURE 第四节 | P1 |
| 四 | 总体架构设计 | ✅ ARCHITECTURE.md + ADR + 类图 | — |
| 五 | 分模块详细设计 | ✅ 8 subsystem 文档 | — |
| 六 | 跨模块核心机制 | ✅ communication + configuration | P2 |
| 七 | 数据架构设计 | ✅ data-architecture.md | P3 |
| 八 | 接口设计规范 | ✅ interfaces.md | P2 |
| 九 | 非功能专项设计 | ✅ non-functional.md | P1 |
| 十 | 部署与运维架构 | ✅ deployment.md | P3 |
| 十一 | 风险与应对方案 | ✅ risks.md | P2 |
| 十二 | 测试架构设计 | ✅ quality/README | — |
| 十三 | 落地实施规划 | ✅ ITERATION.md | — |
| 十四 | 附录 | ✅ appendix.md | P3 |

---

## P1：优先完成（商用文档核心）

### 一、基础元信息 ✅

### 二、系统边界与上下文 ✅

### 四、总体架构设计 ✅

### 九、非功能专项设计 ✅

---

## P2：近期完成（补架构完整性）

### 二、业务与需求背景（新增文档）

- [x] 项目定位：工业 AMR 中间件，面向机器人平台软件与通信协议栈领域
- [x] 核心功能清单（Must/Should/Could）
- [x] NFR 约束表（延迟、吞吐、可靠性、安全、资源）

### 六、跨模块核心机制 ✅

### 八、接口设计规范 ✅

### 十一、风险与应对方案 ✅

---

## P3：远期（工程完整性）

### 七、数据架构设计 ✅

### 十、部署与运维架构 ✅

### 十四、附录 ✅

---

## 本迭代新增 (2026-07-19)

| 任务 | 描述 |
|------|------|
| ✅ ARCHITECTURE 图重构 | 概览图（分层）+ 系统运行时视图（红/蓝双色线）+ 类关系图精简 |
| ✅ 状态图优化 | `direction TB` 垂直严重度排列，去掉冗余互跳 |
| ✅ Mermaid 语法修复 | 去嵌套子图、direction、`===` 厚线 → GitHub 正常渲染 |
| ✅ /diagnostics 发布 | HealthMonitor → ROS2 标准 diagnostic_msgs，兼容 rqt_runtime_monitor |
| ✅ 静态分析 | cppcheck 集成 CI，阻塞级 error，skip style/info |
| ✅ ASan+UBSan+LSan | `./quality.sh asan` 一键跑，lsan.supp 压制 DDS/ROS2 噪音 |
| ✅ LSan 压制 | 只扫描业务代码，mute rclcpp/rmw/Fast-DDS/gtest 泄漏 |
| ✅ CI 顺序调整 | 静态分析 → 构建+测试+覆盖率（有问题不跑测试） |
| ✅ 子系统文档重构 | 8 篇统一 6 章模板（位置/结构/流程/接口/边界/参考），新增 15 张 Mermaid 图 |

---

## 技术远景：ROS2 应用层框架

> 2026-07-19。项目定位：**ROS2 感知-执行管线参考架构 → 应用层框架**。
> 目标：解决 ROS2 裸写项目的 7 个核心痛点，成为 ROS2 生态中感知传感器的标准接入层。

### 市场分析

#### ROS2 生态中的传感器 HAL 项目（8 个已知项目）

| 项目 | 语言 | 类型 | 与我们的差异 |
|------|:---:|------|------|
| [Clearpath Sensor Config](https://docs.clearpathrobotics.com/docs/ros/config/yaml/sensors/overview/) | YAML | 配置层 HAL | 只做配置，不做接口抽象。无线程安全合约，无观测集成 |
| [ros2_control Hardware Interface](https://control.ros.org/) | C++ | 执行器 HAL | 针对电机/关节，不覆盖感知传感器（LiDAR/Camera/IMU） |
| [mrpt_sensorlib](https://github.com/mrpt-ros-pkg/mrpt_sensors) | C++ | 传感器驱动基类 | 通用传感器 Node 基类，无 DDD 分层，无 ISensor<T> 接口抽象 |
| [adi_iio](https://github.com/analogdevicesinc/iio_ros2) | C++ | 专用 HAL | Analog Devices IIO 设备专用，非通用传感器框架 |
| [arora_hal](https://docs.rs/arora-hal/latest/arora_hal/) | Rust | Trait HAL | 验证了 `FakeHal` = 我们的 `Simulated*` 模式。但 Rust 生态，ROS2 C++ 不可用 |
| zuuu_hal (Pollen Robotics) | Python | 移动底盘 HAL | 特定硬件（Reachy/Zuuu），非通用传感器 |
| neuronav-slam-sdk | Python | SLAM 传感器层 | RGB-D 相机统一 API（RealSense/OAK-D），专注 SLAM 场景 |
| openzen_driver | C++ | 多传感器转发 | 将 OpenZen 库支持的传感器转发到 ROS2 Topic，非接口抽象 |

#### 我们的差异化：不是 HAL，是 HAL + DDD + 观测 + 降级 + 测试

| 维度 | 所有已知项目 | 我们 |
|------|:---:|:---:|
| 传感器接口抽象 | ✅ 不同程度做到 | ✅ 编译期类型安全 + 线程安全合约 |
| DDD 分层编译期强制 | ❌ 无 | ✅ domain/ 禁 ROS2 依赖 |
| 观测集成 | ❌ 无 | ✅ Traces/Metrics/Logs 三支柱 |
| 降级框架 | ❌ 无 | ✅ 5 级传感器降级 + 看门狗 |
| 测试夹具 | ⚠️ arora_hal 有 FakeHal | ✅ SimulatedLidar/Imu/Camera + 65 用例 |
| 线程安全文档 | ❌ 无 | ✅ 值拷贝 / mutex / 视图 三策略文档化 |

**结论**：传感器 HAL 是共识需求（8 个项目在做），但**没有一个 C++ 项目把它和架构分层、观测、降级、测试组合成完整方案**。这是我们的差异化——不是 HAL 功能本身，是架构完整性。

### ROS2 应用层 7 大痛点

| # | 痛点 | 来源 | 市场价值 | 时间尺度 |
|---|------|------|:---:|:---:|
| 1 | **传感器接入无标准** | 社区 8+ 项目各自做 HAL，互不兼容 | ⭐⭐⭐⭐⭐ | **短期** |
| 2 | **ROS2 Node 测试门槛高** | GitHub Issues + Discourse 搜索 | ⭐⭐⭐ | 中期 |
| 3 | **无内建可观测** | OTel 在 ROS2 几乎不存在 | ⭐⭐⭐⭐ | 中期 |
| 4 | **故障降级是手写 if-else** | NAV2/ros2_control 各自独立实现 | ⭐⭐⭐ | 短期 |
| 5 | **配置系统碎片化** | 4 种配置入口，加载顺序靠经验 | ⭐⭐ | 中期 |
| 6 | **项目脚手架缺失** | `ros2 pkg create` 生成空壳 | ⭐ | 长期 |
| 7 | **多 AMR 协调无标准** | fleet-level 健康聚合是空白 | ⭐⭐⭐ | 长期 |

### P0：传感器标准接入层（SMART）

**S**：定义统一的 `ISensor<T>` 接口规范 + 插件注册机制 + 配套测试工具，使第三方传感器适配器作为独立 `.so` 插件加载，不修改框架源码。

**M**：(1) 外部开发者不看框架源码，仅通过头文件和文档，30 分钟内完成新传感器适配 (2) `colcon build` 不重新编译框架代码 (3) 3 个真实传感器（LiDAR/IMU/Camera）的生产级适配器 + 模拟对应物。

**A**：ISensor<T> 接口已定义，SickTiM781Adapter 已验证。差距：SensorFactory if-else → 插件注册；适配器编译为独立 `.so`；Schema 定义。

**R**：直接命中 JD 第 1/3 条。市场验证：8 个开源项目在做同一方向，但无一完成"HAL + DDD + 观测 + 降级"的架构闭环。

**T**：
- M1（2 周）：SensorRegistry + 插件注册 + 3 真实传感器适配器
- M2（4 周）：独立 ROS2 package `amr_sensor_hal`，其他项目 `find_package` 即用
- M3（6 周）：社区贡献指南 + 外部开发者成功接入 1 个新传感器

### NAV2 / ros2_control 与我们的关系

```
ROS2 软件栈：

  ┌────────────────────────────────────┐
  │  NAV2（导航栈）                     │
  │  全局规划 → 局部规划 → 行为树       │
  ├────────────────────────────────────┤
  │  ros2_control（执行器框架）          │
  │  Hardware Interface → 电机/关节     │
  ├────────────────────────────────────┤
  │  ★ 我们的位置（应用层框架）          │
  │  ISensor HAL → 感知-决策-执行管线    │
  │  + Degradation + Observability     │
  ├────────────────────────────────────┤
  │  ROS2 中间件（rclcpp + Fast-DDS）    │
  │  DDS / Lifecycle / QoS / Action     │
  └────────────────────────────────────┘
```

我们和 NAV2 是**互补**（我们做感知→决策，NAV2 做规划→导航）。我们和 ros2_control 是**平行**（我们做传感器抽象，它做执行器抽象）。两者可联合使用。

### 迭代路线

#### 短期（P0：传感器标准接入层，2-4 周）

- [ ] SensorRegistry 插件注册机制，替代 `SensorFactory` 的 if-else
- [ ] 3 个真实传感器适配器（LiDAR/IMU/Camera）生产级实现
- [ ] `ISensor<T>` 接口规范文档 + 线程安全合约
- [ ] 适配器开发模板 + `REGISTER_SENSOR` 宏

#### 中期（P1：观测 SDK + 测试框架，4-8 周）

- [ ] 观测 SDK 独立 ROS2 package（`amr_observability`）
- [ ] 标准化 Degradation Framework（从 Fusion 内部提取为独立库）
- [ ] ROS2 Test Fixture 库（`TestNode<T>` + mock sensor + mock publisher）

#### 长期（P2：脚手架 + 集群，8-16 周）

- [ ] AMR 项目生成器（`ros2 pkg create --template amr`）
- [ ] 配置层统一（YAML Schema + 启动校验 + 热加载）
- [ ] Fleet SDK（多 AMR 健康聚合 + 任务调度）

---

## 已完成（不需要再动）

| 文档 | 对应章节 |
|------|:---:|
| `ARCHITECTURE.md`（数据流/控制流/状态流/DDD 分层/类图/进程模型） | 四 |
| `subsystems/sensor-pipeline.md` | 五 |
| `subsystems/fusion-pipeline.md` | 五 |
| `subsystems/decision-pipeline.md` | 五 |
| `subsystems/actuation-pipeline.md` | 五 |
| `subsystems/health-monitor.md` | 五 |
| `subsystems/observability.md` | 五、六 |
| `adr/03-adr.md` | 四 |
| `guides/06-dds-customization.md` | 六 |
| `guides/07-observability-design.md` | 六 |
| `quality/README.md` | 十二 |
| `README.md` | 一（摘要级） |

---

## 执行顺序

```
Day 1: P1   (约 2h)
  ├── 一、基础元信息 → 补入 ARCHITECTURE.md 开篇
  ├── 三、系统边界 → Context 图 + 外部依赖清单
  ├── 四、补充 → 设计原则 + 技术选型对比表
  └── 九、非功能 → 独立文档 doc/non-functional.md

Day 2: P2   (约 3h)
  ├── 二、业务背景 → doc/requirements.md
  ├── 六、通信模块 → subsystems/communication.md
  ├── 六、配置模块 → subsystems/configuration.md
  ├── 八、接口规范 → doc/interfaces.md
  └── 十一、风险 → doc/risks.md

Day 3: P3   (约 2h)
  ├── 七、数据架构 → doc/data-architecture.md
  ├── 十、部署运维 → doc/deployment.md
  └── 十四、附录 → doc/appendix.md
```
