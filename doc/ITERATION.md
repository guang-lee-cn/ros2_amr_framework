# 迭代计划

> 以 14 章架构设计说明书为牵引，逐章盘点现状与缺口，规划迭代。

---

## 总览

| 章 | 章节名 | 现状 | 优先级 |
|:---:|------|:---:|:---:|
| 一 | 基础元信息 | ✅ P1 | P1 |
| 二 | 业务与需求背景 | ✅ requirements.md | P2 |
| 三 | 系统边界与上下文 | ✅ 补入 ARCHITECTURE | P1 |
| 四 | 总体架构设计 | ✅ ARCHITECTURE.md + ADR | — |
| 五 | 分模块详细设计 | ✅ 8 subsystem 文档 | — |
| 六 | 跨模块核心机制 | ✅ communication + configuration | P2 |
| 七 | 数据架构设计 | ❌ 缺 | P3 |
| 八 | 接口设计规范 | ✅ interfaces.md | P2 |
| 九 | 非功能专项设计 | ✅ non-functional.md | P1 |
| 十 | 部署与运维架构 | ❌ 缺 | P3 |
| 十一 | 风险与应对方案 | ✅ risks.md | P2 |
| 十二 | 测试架构设计 | ✅ quality/README | — |
| 十三 | 落地实施规划 | ✅ ITERATION.md | — |
| 十四 | 附录 | ⚠️ 未集中 | P3 |

---

## P1：优先完成（商用文档核心）

### 一、基础元信息（新增 ARCHITECTURE.md 开篇）

- [ ] 文档标题、版本号 v2.0.0
- [ ] 变更记录（基于 git log 摘要）
- [ ] 读者群体：技术团队 / 系统集成商 / 开源社区
- [ ] 术语表：DDS、spdlog、HAL、DDD、LifecycleNode、ADR、SHM、QoS
- [ ] 参考文档链接

### 三、系统边界与上下文（补入 ARCHITECTURE.md）

- [ ] Context 图——系统内部 ↔ 外部实体（LiDAR驱动/IMU驱动/Camera驱动/Robot底盘/MES/Prometheus/Grafana/GitHub Actions）
- [ ] 外部依赖清单（Fast-DDS、spdlog、rclcpp、tf2_ros、Prometheus、Gazebo）含许可证和版本
- [ ] 系统输入/输出表

### 四、总体架构设计（补充现有）

- [ ] 设计原则（7 条）：分层解耦、依赖倒置、实时与业务隔离、可观测内置、故障隔离、轻量化、DDD
- [ ] 技术选型对比表（Fast-DDS vs CycloneDDS、自研 ring buffer vs spdlog、DBSCAN vs scan-line）

### 九、非功能专项设计（收敛散落内容为独立文档）

- [ ] 性能基线表（端到端延迟 P50/P99、传感器频率上限、CPU 核数预期）
- [ ] 实时性保障（MultiThreadedExecutor + 热路径零 malloc + SHM 跨进程）
- [ ] 高可用设计（进程看门狗 + 降级策略 + LifecycleNode 恢复）
- [ ] 安全设计（SROS2 + DDS-Security per-node enclave）
- [ ] 可扩展性（YAML 驱动传感器选型 + 插件化适配器）
- [ ] 可观测性（交叉引用 guides/07-observability-design.md）
- [ ] 容量边界（最大 AMR 数量、传感器频率上限、RingBuffer 溢出处理）

---

## P2：近期完成（补架构完整性）

### 二、业务与需求背景（新增文档）

- [x] 项目定位：工业 AMR 中间件，面向机器人平台软件与通信协议栈领域
- [x] 核心功能清单（Must/Should/Could）
- [x] NFR 约束表（延迟、吞吐、可靠性、安全、资源）

### 六、跨模块核心机制（补充缺失模块文档）

- [x] `subsystems/communication.md` — DDS 域划分、QoS 分级、服务发现、跨设备通信（当前仅在 ADR-8 中提及）
- [ ] 并发与同步机制 → 并入 `九、非功能` 的实时性保障
- [ ] 配置管理机制 → `subsystems/configuration.md` — sensors.yaml、Fast-DDS XML profile、ROS2 params

### 八、接口设计规范（新增文档）

- [x] `doc/interfaces.md` — 完整 DDS Topic 表（类型/QoS/周期/方向）
- [x] 函数 API 约束（智能指针所有权规范、参数传递规则）
- [x] Action 接口规范（Goal→Feedback→Result 生命周期）
- [x] 接口版本兼容策略

### 十一、风险与应对方案（新增文档）

- [x] 架构风险矩阵：通信延迟超标、内存泄漏、DDS 消息丢失、日志阻塞
- [x] 技术风险矩阵：第三方库兼容性、编译器链接问题
- [x] 每条含：描述、影响等级、规避方案、应急预案

---

## P3：远期（工程完整性）

### 七、数据架构设计（新增文档）

- [ ] 数据流总图（消息从采集→处理→存储→上报）
- [ ] 存储分层（RingBuffer 内存缓存、spdlog 本地日志、Prometheus 时序、PerceptionObjects DDS）
- [ ] 数据生命周期（日志滚动删除、指标留存周期、内存防溢出）

### 十、部署与运维架构（新增文档）

- [ ] 目标硬件环境（WSL2 x86_64 开发、RK3588 ARM64 部署）
- [ ] 容器部署方案（Docker multi-stage build）
- [ ] 启动流程（system.launch.py → 6 进程启动序列）
- [ ] 运维操作（日志查看、Prometheus 指标查询、覆盖率报告）

### 十四、附录（整理已有内容）

- [ ] 关键代码片段（ISensor 接口示例、PerceptionService 注入示例、SensorFactory 创建示例）
- [ ] CMakeLists 结构说明
- [ ] 第三方组件官方文档链接

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
