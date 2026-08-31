# 决策记录审计 — 20260901-reaudit-fix-decision-record.md

> 审计对象：`docs/design/20260901-reaudit-fix-decision-record.md`（ADR，2026-09-01）
> 方法：ADR 的价值在"每条决策有证据、有理由"——故本审计只做两件事：① 复核其核验记录与
> 事实性理由是否为真（逐条独立取证，不采信双方叙事）；② 检查每条裁决**按字面可执行性**
> （一个引用了不存在机制的 ADR 条目，执行者只会静默即兴或返工）
> 本轮新增取证：supervisor_node.cpp 全文 pub/sub 普查（create_pub 辅助函数形态）、
> /health/report 与 /supervisor/report 的方向与消费方、git ls-files 私钥入库检查、
> test_supervisor_policy 测试计数、**本机 `ros2 security create_keystore` 实证**（单 CA 软链复现）、
> CLAUDE.md:30 与 ARCHITECTURE.md:151-160 数据流叙事交叉比对
> 关系链：复审报告 → 整改方案 → 方案审计 → 本 ADR → 本审计

---

## 1. 总判定

**核验文化到位，四项独立取证全部为真，零 Reject 的裁决成立；但 S-2 的修改案引用了
一个不存在且方向相反的机制，R2.1 的验证门锁错了回归点，R4.3 漏改一处同源假叙事。
三处修正后本 ADR 可作为执行依据。**

这份 ADR 最值得肯定的地方：它对方案审计的接受不是照单全收——核验表四行命令是真实
取证（本审计逐条复核全部属实，含 max_iterations=50000、"domain 侧已有 22 测"、
"keystore 不入库"三条数字类宣称）；冲突表的自我定性（"我的方案犯了它自己要根治的
同类错误"）准确。§3.2 的工具链前提（create_keystore 在 Jazzy 即单 CA 软链）经本机
实证**为真**——该条推迟裁决的证据链完整。

问题集中在两处"把建议具体化"的时刻：具体化产生了新的可检验断言，而断言没有回落
到代码。这与 P0-I、与方案层 R4.3 是同一家族——**具体化必须伴随验证**。

---

## 2. 核验记录复核（ADR §一，四行逐条）

| ADR 核验行 | 本审计复核 | 结论 |
|---|---|---|
| R4.3 句子与代码不符 | system.launch.py:58-64 零参数块（前序审计已证，未变） | ✅ 属实 |
| R1.1 根因=initial 缺 --ignore-errors | quality.sh:50-52 无 flag + `\|\| true`；runtime 侧带完整 `--ignore-errors empty,unused,mismatch,gcov` | ✅ 属实 |
| R3.2 A* 百毫秒级 | astar_planner.hpp:64 `max_iterations = 50000`；400×400 格 | ✅ 属实 |
| R5.1 拼接必坏 | entrypoint.sh:5 `NODE_EXEC="${NODE}_node"` | ✅ 属实 |

附带三条数字宣称复核：test_supervisor_policy.cpp 22 个 TEST ✅；`git ls-files
config/sros2/private` = 0 条（私钥不入库）✅；R1.2 预判 78.7% 与复审复算一致 ✅。

---

## 3. 必须修正（三处）

### 3.1 F-1：S-2 修改案引用的机制不存在，且方向相反

**ADR 原文**：「supervisor 观测 /supervisor/report 有 ERROR → 进程级策略接管——
用现有机制，不加新通道」

**代码事实**（本审计普查）：

1. `/supervisor/report` 是 supervisor 的**输出**：supervisor_node.cpp:288-289
   `create_pub<HealthReport>("/supervisor/report", latched_state)`，:281 周期发布
   聚合状态。**supervisor_node 全文件零订阅**——它不"观测"任何 topic，对子进程的
   管理是进程级的（:145 spawn、:208 重启预算耗尽 → FATAL）。
2. health_monitor 的上行通道是 `/health/report`（health_monitor_node.cpp:231-232），
   **单机形态下无消费者**——订阅它的是 fleet_manager（fleet_manager_node.hpp:51-53，
   仅 fleet_multi 形态）。
3. 结构性缺口：被卡死的 lifecycle 节点活在 compute_container 进程内——容器进程
   健在，supervisor 的进程级探测**原理上看不见**节点级死亡。任何真接管都需要新接线
   （supervisor 订阅健康状态，或 health_monitor 获得容器级动作权），"不加新通道"
   不成立。

**责任说明**：方案审计 S-2 原文只给了方向（"置 DEGRADED 或交 supervisor"）而未验证
通道存在——本审计与 ADR 各漏一半；ADR 的"细化"把方向变成了具体机制断言，具体化
本应触发回落到代码，没有触发。

**修正**（三选一，需显式重裁决）：

- **a. 诚实降级**：R4.2 第一阶段只做「ERROR 状态上 /health/report + ERROR 日志」
  （可观测、无接管），接管接线单列条目显式排期——零新通道，宣称与实现一致；
- **b. 接受新通道**：supervisor 增加对 /health/report 的订阅 + ERROR 策略（kill
  compute_container → respawn）——工作量诚实计入，e2e 验证门；
- **c. 不做升级**：维持 WARN，风险显式写入（恢复链路断而不挂）。

按字面维持原文的后果：执行者 grep /supervisor/report 发现是输出 topic，要么静默
即兴（更糟）要么返工——ADR 条目必须按字面可执行。

### 3.2 F-2：R2.1 验证门锁错了回归点

**ADR 原文**（执行清单 R2.1）：「motor_ctrl_node declare_parameter 默认 0→50 +
域测断言 `Params{} == 50`」

**事实**：P0-G 的实际回归点是 **node 侧 declare_parameter 覆写**（域默认 50 一直
是对的）。`Params{}.min_valid_echoes == 50` 只锁 domain 常量——node 默认值再度
回退 0 时该测试照绿。这正是 P0-G 第一次发生时的盲区原样保留。

**修正**：单一事实源——domain 头定义 `inline constexpr int kDefaultMinValidEchoes = 50;`
（或置于 CollisionGuard 内），collision_guard.hpp:66 的 Params 默认与
motor_ctrl_node 的 declare_parameter **都引用它**。域测断言该常量；node 侧因引用
同一常量而无法独立回退。验证门加一条 grep：motor_ctrl_node.cpp 中
`guard_min_valid_echoes` 的 declare 行出现 `kDefault` 而非字面量。

### 3.3 F-3：R4.3 改法 1 漏改一处同源假叙事

**事实**：同一"传感器→fusion 跨进程 DDS"叙事存在**两份**正式文档：
ARCHITECTURE.md:151-160 数据流表（ADR 已裁决修）与 **CLAUDE.md:30**（「传感器→fusion
的跨进程 DDS 是故意保留（故障隔离），不要"优化"掉」——零拷贝节）。只改前者，
两份文档从此互相矛盾；且 CLAUDE.md 是所有未来 AI 协作的先验，传播力更强。

**修正**：R4.3 执行清单补一行——CLAUDE.md:30 同步改为真话（如「真机形态规划为
跨进程 DDS（故障隔离），当前默认 launch 未接线——见 ARCHITECTURE.md 数据流注」）。

---

## 4. 理由瑕疵（裁决可维持，措辞需更正）

| # | ADR 位置 | 瑕疵 | 更正 |
|---|---|---|---|
| P-1 | §3.1 R1.1「真根因**锁定**为…」 | 现有证据是排除法 + 旁证（高置信），终验在 R1.3 去沉默之后 | 改「高置信假设；R1.3 的 grep 门即终验」——ADR 不该给未终验的根因发"锁定"证书 |
| P-2 | §2.1 理由 3「届时按 D1 验收的 8 分钟接入路径实现」 | D1 范本（temperature/ultrasonic）是**进程内 SensorFactory 注册**路径；topic-source 是订阅回填型新适配器形态，8 分钟是跨形态外推，未验证 | 删去时间数字，保留时机判断（真机驱动需求）即可 |
| P-3 | R4.4「发 v2.2.1（patch）」 | 内容含默认值反转（行为变更）+ 竞态修复——semver 语义偏 minor | v2.3.0 或 v2.2.1 均可，但补一句裁决理由，避免版本语义重演"三宇宙"式的随手 |

---

## 5. 小项

| # | 项 | 说明 |
|---|---|---|
| S-A | 节点侧 "(sim)" 注释 | ADR 独立补充 3 只点名 collision_guard.hpp:66；motor_ctrl_node.cpp:32 的 "(sim) treats an echo-less scan as gpu_lidar death" 是同一框定的另一处，R2.2 口径统一清单应显式包含 |
| S-B | 新发现：QoS 词汇表违反 | health_monitor_node.cpp:232 `rclcpp::QoS(10).reliable()` 手搓 QoS——违反 CLAUDE.md 禁止清单第 5 条（必须走 amr::qos:: 词汇表；同文件 supervisor 侧用 latched_state 是对的）。S-2/R4.2 触碰同一区域，顺带修为 `amr::qos::reliable_stream`（或按语义选词） |
| S-C | sros2 前提的反向教训 | 本审计初始怀疑「create_keystore 在 Jazzy 即单 CA 软链」为假（记忆中 sros2 生成双独立 CA），本机实证**为真**（/tmp 复现软链结构）。ADR 该条无需改动；记录在此说明该前提已获独立实证，推迟裁决的成本论证成立 |

---

## 6. 裁决汇总

| ADR 条目 | 本审计处置 |
|---|---|
| 一、核验记录（4 行） | ✅ 全部属实（本审计逐条复核） |
| §2.1 R4.3 Accept 改法 1 | ✅ 裁决维持；理由 3 按 P-2 更正；执行清单补 F-3（CLAUDE.md:30） |
| §2.2 R3.2 Accept 快照 | ✅ 维持 |
| §2.3 R3.1 Accept 两段式 | ✅ 维持；其独立补充（独立 ranges 缓冲 + 被堵 goal 最大化交叠）质量高，采纳 |
| §3.1 R1.1 Accept | ✅ 维持；"锁定"措辞按 P-1 降级 |
| §3.2 双 CA 显式推迟 | ✅ 维持——前提实证为真（S-C），私钥不入库属实，风险有界论证成立 |
| §3.3 R5.1 Accept 细化 | ✅ 维持 |
| §3.4 R4.4 v2.2.0 钉 6d5697f | ✅ 维持；版本号语义按 P-3 补裁决理由 |
| S-1/S-3/S-4/S-5/S-6 | ✅ 维持 |
| **S-2 升级动作** | ❌ **按字面不可执行（F-1）——需三选一重裁决** |
| **R2.1 验证门** | ❌ 锁错回归点（F-2）——改单一事实源常量 |
| 三、独立补充 1/2/3 | ✅ 采纳（补充 3 补 S-A 一处）；R1.6 CLAUDE.md 元防线可顺带承载 F-3 |

---

## 7. 结语

这份 ADR 把"接受审计"做成了再验证过程而非签收流程——四行核验命令、冲突表、
零 Reject 的理由陈述都是真的，这在这类文档里少见。它翻车的两处有一个共同点：
**当把别处的建议具体化为自己的机制断言时，验证没有跟着升级**。S-2 是把审计的
方向性建议变成"用现有机制不加新通道"（机制不存在），R2.1 是把行为测试收窄成
常量断言（锁错了点）。规律与 P0-I 完全同构——具体化即断言，断言即需取证。

修完 F-1/F-2/F-3 三处，本 ADR 达到"按字面可执行"标准，可进入 Wave R1。

---

**审计证据索引**：supervisor_node.cpp:145,208,281,288-289（零订阅，输出型 topic）·
health_monitor_node.cpp:129-132,231-232（QoS 手搓同址）· fleet_manager_node.hpp:51-53
（/health/report 唯一消费方，仅 fleet 形态）· astar_planner.hpp:64 ·
motor_ctrl_node.cpp:32 · collision_guard.hpp:66 · CLAUDE.md:30 · ARCHITECTURE.md:151-160 ·
git ls-files config/sros2/private（空）· test_supervisor_policy.cpp（22 TEST）·
本机 create_keystore 实证（单 CA 软链复现，2026-09-01）
**文档版本**：1.0（2026-09-01）
