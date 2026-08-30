# 迭代 2 目标：L7 自审驱动的落地清单（2026-08-24）

> 来源：宣称 vs 证据审计（「面向商用/可插拔/可扩展/应用软件框架」逐词判定）+ delta-over-ROS2 审计。
> 原则：每一条都来自审计发现的**具体缺口**，且是面试口头承诺的实体化——说了就要长出代码。
> 依赖标注：〔Orin〕= 等开发板到货；〔无依赖〕= 随时可做。
> **2026-08-25 外部 L7 审计**（`doc/ITERATION.md` 文末节）：B1 被评为全仓最硬闭环、A2/C1 方向被印证；
> 并新增 **P0 私钥泄露**、覆盖率链修复、fail-fast、e2e 断言四项。
> **08-28 复审（同文档 §8）**：四项已全部闭环，总评 6.5 → 7.5；剩余尾款 = A2 的 72h 正式跑、
> DDS security 启用、OTA 真签名、systemd、版本对齐——见 §8.3。

## P0 · 私钥泄露处置（外部审计新增，先于一切）— ✅ 已处置（2026-08-25）

SROS2 私钥 11 文件（CA 三件套 + 8 enclave key）随 `8c4873b` 进入已推送历史。
处置完成记录：
- 全量备份 bundle → **密钥轮换**（新 CA + 8 enclave，private/enclaves 在 gitignore 内）
  → `git filter-repo` 重写 244 commits（`--invert-paths '*/sros2/*key.pem'`）
  → 强推 origin/main → 远端 git 侧验证清零
- **防再犯**：gitleaks-action 全历史扫描进 CI（secrets-scan job，先于构建）
- 残留：GitHub 平台 GC 前悬空 commit 仍可经 API 直链访问（需 Support 工单
  请求 gc）；密钥已轮换烧毁，残留风险有界
- **泄露 #2**（2026-08-26，gitleaks 首次全量扫描抓出）：colcon build//log/
  环境转储含 live AI key，同法处置（两轮补写重写）——见 journal 当日条目；
  持有人轮换受影响 key 为必做动作
- 详见 change journal 2026-08-25 P0 条目

## A 类 · 闭合「商用」验证差（宣称里最软的词，最高优先）

| # | 目标 | 验收标准 | 依赖 |
|---|------|---------|------|
| A1 | **真机 bring-up**：EtherCAT 伺服 + ros2_control 硬件插件（DiffDriveSystem 第二季） | 真电机速度/位置闭环日志 + 与仿真闭环同构的验证记录 | 〔Orin〕+ 二手 EtherCAT 驱动 |
| A2 | **72h soak + 故障注入**：连续负载、周期性 kill -9、内存/泄露监控 | soak 报告：吞吐/时延曲线、恢复次数=注入次数、RSS 平稳 | 〔无依赖〕（仿真负载即可先行） |
| A3 | **ARM64 + PREEMPT_RT 复测** | bench5 三环境对照表填满（脚本已备） | 〔Orin〕 |
| A4 | **RAUC 真机集成**：OtaCoordinator 的 SlotOps 接 RAUC 后端 | 真分区升级+回滚演示替换目录级模拟 | 〔Orin〕 |
| A5 | **e2e 断言式测试**（外部审计建议新增）：到点停车/遇障停车/恢复 3 场景进 ctest | 「车真动、真停、真避障」有断言证据，不再是纸面 integration-plan | 〔无依赖〕 |

> **A5 进展（2026-08-26）**：**完成**——`test_e2e_behavior` 三场景全绿
> （scene_simulator 闭环，CI 容器可跑）：到点真停（IT-06）、遇障绕行不穿透
> （IT-08，实测 0.601m > 物理界 0.57）、断源降级+恢复（IT-07 族）。
> 开发过程挖出并修复 4 个真 bug（PurePursuit 死区绕圈/快照丢戳/缓存判活/
> 降级冻结），见 change journal 2026-08-26 条目。

> **A2 进展（2026-08-25）**：harness 已落地并 smoke 实证（注入2/恢复2，
> MTTR 125-150s）——见 `docs/design/20260825-a2-soak-runbook.md`。
> 剩余：72h 正式跑（`nohup ./scripts/soak_run.sh &`）+ 出报告归档。

## B 类 · 闭合「框架」差距（IoC 缺口）

| # | 目标 | 验收标准 | 依赖 |
|---|------|---------|------|
| B1 | **supervisor 实体化**：崩溃监管/依赖序重启/健康门 | 声明式配置驱动；kill -9 任一节点按策略恢复（health_monitor/OtaCoordinator 零件复用） | 〔无依赖〕 |
| B2 | **运行时插拔**：迁 rclcpp_components 容器（或 pipeline.nodes 加 load/unload） | ros2 component list 可见、运行时装卸节点成功——补齐「落后于 stock」那一格 | 〔无依赖〕 |
| B3 | **API 稳定性**：amr::qos / AmrNode 版本化与弃用策略（微缩 REP） | CHANGELOG 驱动的接口变更记录 + 一条真实弃用演练 | 〔无依赖〕 |
| B4 | **覆盖率链修复**（外部审计 P1-a）：CI 上传 lcov、badge 由 CI 生成、修 coverage_full 解析、删 lcov 失败静默放行 | 84.5% 等数字可从仓库数据复算；badge 与明细对账 | 〔无依赖〕 |

> **B1 进展（2026-08-25）**：**完成**——`amr_supervisor` 进程级监管 + 全栈 rollout
> （supervised_sim.launch.py，11 子进程）。实证：kill -9 compute 1.75s 恢复且
> scan 无恙；kill -9 gz 逆拓扑级联 + oneshot 重放车 + 2.3s 全链回位。
> soak 注入白名单已扩 compute（supervised 形态限定）。
> 见 docs/design/20260825-b1-supervisor-adr.md 与 change journal 当日两条。

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

> **D1 进展（2026-08-26）**：**完成**——两轮干净上下文子代理实测：一测
> 19.5min/8 摩擦（含审计点名的坏宏 + 新发现的静态库链接器丢弃注册对象）→
> 修复 → 二测 8m48s/2 摩擦/一次全过（范围翻倍）。「X 分钟接入」成为数据。
> 见 docs/design/20260826-d1-extension-acceptance.md。

## E 类 · 小补（半天级）

- E1 序列化类型开销实验（mw-bench：大数组 vs 嵌套结构 × loaned 与否）〔无依赖〕
- E2 OtaCoordinator 事件带载荷重构（std::variant + visit 落地 C++17 补课）〔无依赖〕
- E3 RT 线程封装（jthread + 亲和 + SCHED_FIFO + PTHREAD_PRIO_INHERIT）〔Orin〕

## 优先顺序（2026-08-28 复审后刷新）

```
已完成：P0×2 / A5 / B1 / B4 / D1（复审 §8.1 核验通过）
A2 尾款：72h 正式跑出报告入库（随时启动，建议机器空闲期、跑期不重建）
剩余无依赖：C1(LTTng) > B2 > B3 > E1/E2
复审新增尾款（ITERATION.md §8.3 优先序）：
  ~~DDS security 启用~~✅(08-30 四段断言) > ~~OTA 真签名~~✅(08-30 ed25519 fail-closed) > 告警规则 > systemd/版本对齐 > 27 处裸 QoS 清零
Orin 到货后：A1 → A3 → A4 → D2 → E3（真机线 =「商用验证级」门槛）
```

## 与面试的关系

- 每一项对应一次面试口头承诺（「90 天计划」「Orin 计划」「认账后的替换」）——**这份清单就是承诺的验收单**；
- A 类闭合后，「面向商用」可以从「设计纪律商用级、验证仿真级」升级为「真机验证级」；
- D1 是唯一能自证「可扩展」的实验——其他词都有代码证据，这个词只有他人使用证据。
