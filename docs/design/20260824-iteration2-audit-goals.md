# 迭代 2 目标：L7 自审驱动的落地清单（2026-08-24）

> 来源：宣称 vs 证据审计（「面向商用/可插拔/可扩展/应用软件框架」逐词判定）+ delta-over-ROS2 审计。
> 原则：每一条都来自审计发现的**具体缺口**，且是面试口头承诺的实体化——说了就要长出代码。
> 依赖标注：〔Orin〕= 等开发板到货；〔无依赖〕= 随时可做。
> **2026-08-25 外部 L7 审计**（`doc/ITERATION.md` 文末节）：B1 被评为全仓最硬闭环、A2/C1 方向被印证；
> 同时新增本清单未覆盖的 **P0 私钥泄露**、覆盖率链修复、fail-fast、e2e 断言四项——见该报告 §7 映射表。

## A 类 · 闭合「商用」验证差（宣称里最软的词，最高优先）

| # | 目标 | 验收标准 | 依赖 |
|---|------|---------|------|
| A1 | **真机 bring-up**：EtherCAT 伺服 + ros2_control 硬件插件（DiffDriveSystem 第二季） | 真电机速度/位置闭环日志 + 与仿真闭环同构的验证记录 | 〔Orin〕+ 二手 EtherCAT 驱动 |
| A2 | **72h soak + 故障注入**：连续负载、周期性 kill -9、内存/泄露监控 | soak 报告：吞吐/时延曲线、恢复次数=注入次数、RSS 平稳 | 〔无依赖〕（仿真负载即可先行） |

> **A2 进展（2026-08-25）**：harness 已落地并 smoke 实证（注入2/恢复2，
> MTTR 125-150s）——见 `docs/design/20260825-a2-soak-runbook.md`。
> 剩余：72h 正式跑（`nohup ./scripts/soak_run.sh &`）+ 出报告归档。
| A3 | **ARM64 + PREEMPT_RT 复测** | bench5 三环境对照表填满（脚本已备） | 〔Orin〕 |
| A4 | **RAUC 真机集成**：OtaCoordinator 的 SlotOps 接 RAUC 后端 | 真分区升级+回滚演示替换目录级模拟 | 〔Orin〕 |

## B 类 · 闭合「框架」差距（IoC 缺口）

| # | 目标 | 验收标准 | 依赖 |
|---|------|---------|------|
| B1 | **supervisor 实体化**：崩溃监管/依赖序重启/健康门 | 声明式配置驱动；kill -9 任一节点按策略恢复（health_monitor/OtaCoordinator 零件复用） | 〔无依赖〕 |

> **B1 进展（2026-08-25）**：**完成**——`amr_supervisor` 进程级监管 + 全栈 rollout
> （supervised_sim.launch.py，11 子进程）。实证：kill -9 compute 1.75s 恢复且
> scan 无恙；kill -9 gz 逆拓扑级联 + oneshot 重放车 + 2.3s 全链回位。
> soak 注入白名单已扩 compute（supervised 形态限定）。
> 见 docs/design/20260825-b1-supervisor-adr.md 与 change journal 当日两条。
| B2 | **运行时插拔**：迁 rclcpp_components 容器（或 pipeline.nodes 加 load/unload） | ros2 component list 可见、运行时装卸节点成功——补齐「落后于 stock」那一格 | 〔无依赖〕 |
| B3 | **API 稳定性**：amr::qos / AmrNode 版本化与弃用策略（微缩 REP） | CHANGELOG 驱动的接口变更记录 + 一条真实弃用演练 | 〔无依赖〕 |

## C 类 · NIH 清偿（换生态件，审计认账项）

| # | 目标 | 验收标准 | 依赖 |
|---|------|---------|------|
| C1 | tracer → **ros2_tracing/LTTng**（自研 tracer 降级为 fallback） | callback 级 trace 事件可采（顺带闭合回调↔线程观测） | 〔无依赖〕 |
| C2 | scene_simulator 标注权宜 + 评估 Webots/Gazebo 修复路径 | 决策记录（ADR）：保留/替换二选一有数据支撑 | 〔低优〕 |
| C3 | 算法层 nav2/PCL 替换评估（Domain 零 ROS 使插件化替换可行） | ADR：哪些换生态件、哪些保留自研及理由 | 〔低优〕 |

## D 类 · 可扩展性实证（宣称里唯一无法自证的词）

| # | 目标 | 验收标准 | 依赖 |
|---|------|---------|------|
| D1 | **第三方扩展验收测试**：一个不读内部代码的「用户」（真人或仅凭 README/CLAUDE.md 的 AI）从模板写新节点+新传感器 | 记录全部摩擦点并修复；时长可复测（「X 分钟接入」成为数据） | 〔无依赖〕 |
| D2 | 跨机部署 + 发现治理（discovery server 实操，双机） | 跨机话题通 + 发现流量对比数据（顺带闭合服务发现 L2- 短板） | 〔Orin〕或双 PC |

## E 类 · 小补（半天级）

- E1 序列化类型开销实验（mw-bench：大数组 vs 嵌套结构 × loaned 与否）〔无依赖〕
- E2 OtaCoordinator 事件带载荷重构（std::variant + visit 落地 C++17 补课）〔无依赖〕
- E3 RT 线程封装（jthread + 亲和 + SCHED_FIFO + PTHREAD_PRIO_INHERIT）〔Orin〕

## 优先顺序（依赖解锁前）

```
本周可做：A2(soak) > B1(supervisor) > D1(第三方扩展验收) > C1(LTTng) > B2 > B3 > E1/E2
Orin 到货后：A1 → A3 → A4 → D2 → E3
```

## 与面试的关系

- 每一项对应一次面试口头承诺（「90 天计划」「Orin 计划」「认账后的替换」）——**这份清单就是承诺的验收单**；
- A 类闭合后，「面向商用」可以从「设计纪律商用级、验证仿真级」升级为「真机验证级」；
- D1 是唯一能自证「可扩展」的实验——其他词都有代码证据，这个词只有他人使用证据。
