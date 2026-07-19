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
