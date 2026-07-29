# ISO 战略输出

> 生成日期：2026-07-27
> 模式：重构
> 下游：strategy.md（架构设计阶段）

---

## Insight 结构化记录

| 维度 | 结论 |
|------|------|
| 触发事件 | 从"找工作驱动"→"商用产品驱动"→收缩至"**AMR 端到端验证 + DDS 通信深度攻关**"，以产出可复用的工程方法论为核心目标 |
| 用户/客户 | 主：GitHub ROS2 开发者；次：技术面试官/用人经理。核心痛点：ROS2 生态中 DDS 是最大黑盒，选型靠口碑、故障靠猜 |
| 当前做法 | ROS2 社区依赖 `sensor_msgs` 标准化绕过传感器适配；ros2_control 做了执行器 HAL，ros2_medkit 做了故障管理；**观测 SDK + DDS 选型方法论 + 系统集成是真实空白** |
| 不做会怎样 | 项目停留在"能做 demo 但拿不出系统级深度"的水平，无法对标 L7+ 标准 |
| 长期愿景 | 短期：跑通 AMR 端到端 + DDS 性能调优 + 选型方法论文档；中期：Fast-DDS/CycloneDDS 源码深度剖析系列博客；长期：商业开源项目 |
| 竞品/参考 | 无明确对标物。前置问题：缺乏系统化深度阅读习惯（已识别并嵌入每日计划） |

### 重构前置：现状评估摘要

- **架构**：DDD 四层（domain/application/infrastructure/interfaces），但 application/ 层形同虚设
- **健康资产**：感知管线（DBSCAN+EKF+Tracker）100% 完成 + 65 测试用例；传感器接口抽象 100%；观测模块 100%；ROS2 节点适配层 100%
- **代码缺口**：路径规划 0%，运动控制 0%，HealthMonitor 493 行违反 SRP
- **检索修正**：ros2_control SensorInterface + ros2_medkit 已覆盖 HAL+故障管理的部分功能；差异化集中在观测 SDK、5 级降级策略和 DDD 分层

---

## Strategy 终稿

### 一句话定位

**ROS2 DDS 通信层的系统化知识库**——以 AMR 为验证场景，产出选型方法论、性能调优手册和源码深度剖析。

### 核心目标（SMART）

| # | 目标 | 衡量标准 |
|---|------|---------|
| G1 | AMR 模拟器端到端跑通 | Gazebo/rosbag → 传感器 → 感知 → 控制 → RViz 中按路径移动 |
| G2 | 产出 DDS 选型与性能调优方法论 | 1 篇完整文档，Fast-DDS vs CycloneDDS，含 benchmark 数据 |
| G3 | 产出源码深度剖析系列 | ≥2 篇博客，每篇聚焦一个主题，≥2000 字 + 代码片段 |

### 边界

| ✅ 做 | ❌ 不做 |
|------|--------|
| rosbag/Gazebo 模拟数据驱动 AMR | 真实机器人硬件调试 |
| Fast-DDS vs CycloneDDS 选型+benchmark | 其他 DDS 实现 |
| 源码剖析：通信机制、线程模型、内存管理、设计模式 | DDS-Security 安全规范 |
| DDD 分层保留 domain/infrastructure | application/ 层（删除） |
| HealthMonitor SRP 拆分 | HealthMonitor Gen3 重构 |
| 传感器 HAL 保持现有接口 | 重新设计 HAL |
| 中英双语产出 | — |

### MVP 范围

AMR 端到端 demo 能跑 + DDS 选型初稿：
- Gazebo/rosbag 播放 LiDAR 点云
- 感知管线 + A* 路径规划 + Pure Pursuit
- RViz 中可视化小车移动
- 切换 Fast-DDS/CycloneDDS 记录延迟

### 成功标准（已修正，去掉不可控的社区反馈指标）

| 标准 | 验收方式 |
|------|---------|
| AMR demo 视频 | 录屏：小车从起点到终点，无 crash |
| DDS 选型文档 | 写完发 Discourse（无论回复数） |
| 源码剖析博客 | 写出来发出去（知乎/GitHub） |
| 代码质量 | clang-tidy + ASan 零警告 |

### 关键假设

| 假设 | 风险 | 验证 |
|------|:---:|------|
| Gazebo/rosbag 可模拟真实传感器数据 | 低 | W1 Day 1 |
| Fast-DDS vs CycloneDDS 差异可测量有意义 | 中 | W2 Day 3 |
| 源码阅读不需要内核/FPGA 知识 | 低 | 已验证 |
| 全天投入（~6h/天，每周 6 天） | — | 已确认 |
| WSL2 Gazebo/RViz 可正常渲染 | 中 | W1 Day 1，备选纯命令行 |

### 每日节奏

```
09:00-11:30  深度工作（源码/编码）   2.5h
11:30-13:30  午休+远离屏幕           2h
13:30-16:00  实现工作（编码/文档）   2.5h
16:00-16:30  笔记（200字当日总结）   0.5h
16:30-17:00  规划次日+收尾           0.5h
──────────────────────────────────
合计 6h，17:00 后强制下线，周日休息
```

---

## 盲点检查结果

| 维度 | 结论 | 处理 |
|------|------|------|
| 关键依赖方 | 零外部依赖；社区反馈不可控 | 成功标准改为"做完发出即达标" |
| 存量数据/兼容性 | 无历史数据；代码复用度 80%+ | 删除 application/ 层，其余直接复用 |
| 合规与安全 | NA | — |
| 用户真实需求 | 保 AMR demo 视频 | 确认为第一优先级 |
| 决策者/执行者分离 | 不分离；确认长期判断 | 泛机器人+DDS 是长期竞争力 |
| 失败回滚 | 各阶段止损线已设定 | W1 Day3 仿真调不通→降级 rosbag；W2 Day3 差异不明显→分析"为什么差异小" |

---

## Operation 规划

### 4 周里程碑

| 周 | 阶段 | 里程碑 | 精力分配 |
|:---:|------|------|------|
| W1 | AMR 端到端 | RViz 录屏，小车移动到目标点 | 编码 70% / 笔记 30% |
| W2 | DDS 选型初稿 | benchmark 数据 + 对比分析文档 | 实验 50% / 写作 50% |
| W3 | 源码深读 I | 第一篇源码分析博客 | 阅读 60% / 写作 40% |
| W4 | 源码深读 II + 收尾 | 选型方法论终稿 + 第二篇博客 | 阅读 40% / 写作 60% |

### W1 详细计划

| 日 | 任务 |
|:---:|------|
| Day 1 | HealthMonitor 拆分（PrometheusHttpServer + DiagnosticsPublisher 独立类）+ 删除 application/ 层 |
| Day 2 | A* 路径规划 `domain/planning/astar_planner.hpp` + `test_astar.cpp` |
| Day 3 | Pure Pursuit `domain/execution/pure_pursuit.hpp` + `test_pure_pursuit.cpp` |
| Day 4 | 集成：DecisionNode 调 A* + MotorCtrlNode 调 Pure Pursuit |
| Day 5 | Gazebo/rosbag 端到端 + RViz 录屏 |

### 风险清单

| # | 风险 | 概率 | 影响 | 应对 |
|---|------|:---:|:---:|------|
| R1 | WSL2 Gazebo/RViz 渲染问题 | 中 | 阻塞 W1 | 降级 rosbag 回放 + RViz2 离线；最坏纯命令行 |
| R2 | DDS benchmark 差异不明显 | 中 | W2 缺乏说服力 | 分析原因——这本身是有价值的结论 |
| R3 | 源码太深读不懂 | 高 | 阻塞 W3/W4 | 每天只读一个函数；第三层读不懂就换；不硬磕 |

### 验收

| 里程碑 | 验收标准 |
|--------|---------|
| M1（W1 末） | RViz 录屏 30 秒，无 crash |
| M2（W2 末） | benchmark 表 ≥3 指标 × ≥2 QoS × 2 DDS 实现 |
| M3（W3 末） | 博客 ≥2000 字，≥5 代码片段，调用链图 |
| M4（W4 末） | 选型文档终稿 + 第二篇博客发出 |

### 源码阅读路径

```
入门：Fast-DDS RTPS 层
  1. StatefulReader.cpp — 可靠传输实现
  2. RTPSParticipantImpl.cpp — 节点发现协议

进阶：线程模型 + 内存管理
  3. Fast-DDS SHM Transport
  4. CycloneDDS 对比分析

方法：每天只读一个函数，画调用链，200 字笔记
```
