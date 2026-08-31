# 整改方案决策记录（ADR · 2026-09-01）

> 输入：`20260901-reaudit-fix-plan.md`（我的方案） + `20260901-reaudit-fix-plan-review.md`（方案审计）
> 方法：四项关键技术断言逐条独立核验（命令级取证，非照抄审计结论），
> 然后逐条裁决 Accept / Accept-with-modification / Reject，附理由。
> 目的：供第三方审计——每条决策有证据、有理由、有取舍，不是盲从。

## 一、核验记录（独立取证，非引用审计）

| 审计指控 | 我的核验命令 | 结果 | 结论 |
|----------|-------------|------|------|
| R4.3 句子与代码不符 | `sed -n 55,70p launch/system.launch.py` | compute_container 零参数块，注释自认 "default: simulated" | **属实** |
| R1.1 真根因=initial 缺 --ignore-errors | `grep -A3 initial quality/quality.sh` | :50 无 --ignore-errors，:52 有 `\|\| true`，runtime :54 有完整 flag | **属实** |
| R3.2 A* 最坏百毫秒级 | `grep max_iterations astar_planner.hpp` → 50000 次迭代 × 400×400 格 | 50000 迭代最坏确实百毫秒级 | **属实** |
| R5.1 entrypoint 拼接必坏 | `head -8 toolkit/scripts/entrypoint.sh` → `NODE_EXEC="${NODE}_node"` | compute_container → compute_container_node 不存在 | **属实** |

## 二、逐条决策

### §2.1 R4.3 ARCHITECTURE.md 句子不成立 → **Accept**

**审计正确**。我的原句「独立传感器进程 + DDS 话题消费 = system.launch 显式配置（真机形态）」
与代码事实相反——system.launch 的 compute_container 同样零参数、同样进程内实例化仿真传感器。

**裁决**：采用审计的改法 1（文档改真话）。

**理由**：
1. 改法 2（真接线）是功能开发——fusion 现无 topic-source 传感器类型，需要新增
   ISensor 实现 + 参数路由 + e2e 验证，工作量按新功能计（≥1 天），放在度量诚实波
   里范围蔓延
2. 改法 1 零代码、零风险、直接消灭 P0-K 的「三重零」（代码/文档/排期都没动）
3. topic-source 接线已有明确时机：D1 扩展范本（temperature/ultrasonic）已验证
   HAL 插件路径——真传感器（sick_tim781）上硬件时自然驱动此需求，届时按
   D1 验收的 8 分钟接入路径实现

**验证门更新**：`grep simulated launch/system.launch.py` 确认无参数覆盖 +
ARCHITECTURE.md 表述与之一致。

### §2.2 R3.2 锁窗口数学错误 → **Accept**

**审计正确**。我写「锁竞争窗口微秒级」时想的是锁本身的 mutex acquire/release
开销（~100ns），忽略了锁保护的**临界区**包含 A* 全程——50000 迭代最坏 200ms。
这是推理错误：锁开销微秒 ≠ 锁窗口微秒。

**裁决**：采用审计修正——锁内快照拷贝模式。

**理由**：
1. 160KB（400×400×uint8_t）memcpy 实测量级 10-50µs——快照确实便宜
2. 双缓冲（atomic pointer swap）是备选，但对 0.5Hz plan × 5Hz perception 的
   竞争频率而言是过度工程；快照模式更简单、可测、无 ABA 问题
3. 每 2s 一次 160KB 栈/堆分配的内存 churn 在 compute_container 的 heap
   profile 里不可见（实测过的 :9091 指标可验证）

### §2.3 R3.1 验证门恒绿 → **Accept**

**审计正确且尖锐**。TSAN 在单线程测试下零报告是真空真零——现有 decision
单测全部单线程，修与不修都是零报告。这盏灯没有红的状态。

**这暴露了我的推理盲区**：我写了 §4 验证纪律「能描述如果不修会怎样红」却
在 R3.1 违反了它。与 P0-I 的「已验证」帽子同构——**方案文本本身违反了方案
自己立的规矩**。审计的结语一针见血。

**裁决**：采用审计的两段式验证门（并发压力测试先红后绿）。

### §3.1 R1.1 真根因 → **Accept（含独立补充）**

**审计正确**。gcno 在场的推理成立（runtime 捕获必须读它），我的「gcno 被清」
假设不成立。真根因锁定为 initial 缺 `--ignore-errors mismatch,gcov` +
`|| true` 吞掉失败——与 runtime 侧的 flag 不对齐是**踩坑经验只修了一边**。

**独立补充**：这不只是漏加 flag——是**同一家族错误的两次独立发生**：
- runtime 侧的 --ignore-errors 是此前踩到 mismatch 致命错后加的
- initial 侧遇到同一问题却没检查前例

**元教训**：修 bug 时应当 `grep` 同文件里同类调用是否也需同修。此教训
固化到 CLAUDE.md（或操作习惯）。

### §3.2 双 CA 分离静默漏项 → **Accept**

**裁决**：显式裁决「真机前置项，本轮不修」。

**理由**：sros2 工具链（create_keystore）在 Jazzy 的行为就是单 CA 软链。
独立双 CA 需要手工构造 keystore 结构（分两次 openssl 生成 + 手工目录），
且签名链路（governance/permissions）需重验。工作量小时级但需要 DDS
security 全链路回归——放在真机 bring-up（A1/Orin）时一并做。风险当前
有界：keystore 不入库、不分发，仅在本地开发/测试。

### §3.3 R5.1 compose 会坏 → **Accept（含细化）**

**审计正确**。三处独立问题（拼接规则、三容器冲突、无独立可执行事实）均属实。

**裁决**：
- compose 改为**一个 compute 服务**（command 直传 compute_container）
- entrypoint 增加直通名特例（compute_container 不拼 _node 后缀）
- 在 compose 文件头部注释写明「fusion/decision/motor_ctrl 无独立可执行」

### §3.4 tag 目标语义 → **Accept-with-modification**

**审计正确**。我原方案的 `git tag -fa v2.2.0 HEAD` 在 R4 时点会把 R1-R3 修复
打进 v2.2.0 但 CHANGELOG 不含——语义不干净。

**裁决**：v2.2.0 钉在 **6d5697f**（历史事实——2.2.0 就是带着瑕疵发布的，
更正记录进 journal）；R1-R5 完成后发 **v2.2.1**（patch：度量修复 + 安全
默认值修正 + 竞态修复）。

**补充理由**：force-move 一个已 push 的 tag 是治理动作（consumer 可能已
fetch）——钉在历史 commit 上不动它，比移到 HEAD 更尊重下游。

### §4 小项 → 逐条裁决

| # | 裁决 | 理由 |
|---|------|------|
| S-1 覆盖数据只 ci-bot 提交 | **Accept** | 消灭 N-4（双口径覆盖）的制度根因 |
| S-2 超时升级动作 | **Accept-with-modification** | 置 HealthStatus ERROR + 通知 supervisor（而非仅 WARN）。supervisor 观测 /supervisor/report 有 ERROR → 进程级策略接管——用现有机制，不加新通道 |
| S-3 原子写+状态目录 | **Accept** | temp+rename 模式；写到 `~/.ros/amr_ota/`（不在 install share） |
| S-4 install 空间验证 | **Accept** | 关键洞察：source 绿 ≠ install 绿——P0-F 的 EXCLUDE 正是 install 侧问题 |
| S-5 域测断言默认值 | **Accept** | 不绕节点起 DDS——最小路径断言 `Params{}.min_valid_echoes == 50` |
| S-6 豁免行带理由 | **Accept** | 推广到所有排除 pattern |

## 三、方案审计未覆盖的我的独立补充

1. **R1.1 的元防线**：不只修 initial 的 --ignore-errors——CLAUDE.md 补一行
   「修 bug 时 grep 同文件同类调用是否也需同修」（§3.1 的两次独立发生教训）

2. **R3.1 并发压力测试的具体形态**：审计说「两线程，一线程 raytrace+inflate，
   一线程 plan」——我补充约束：两线程各自持独立的 `ranges` 缓冲（避免测试
   自身引入共享噪声），且 plan 线程用被堵 goal（最大化 A* 迭代次数 → 
   最大化交叠窗口）

3. **R2.1 的注释方向**：不只改 motor_ctrl_node 默认值——domain 注释
   （collision_guard.hpp:66 "(sim)" 框定）同步改为 "(fail-safe default;
   open-field deployments opt out explicitly)"

## 四、最终执行清单（按波次）

```
Wave R1 度量诚实（最先——定义"修复"的可信度）
  R1.1 initial 补 --ignore-errors mismatch,gcov + 去 || true + 非空断言
  R1.2 门禁阈值与真分母冲突 → 裁决（预判：supervisor 0% 进分母后 ~78.7%
       < 80%，走路径 ① 补 supervisor 单测——domain 侧已有 22 测，infra 侧
       需 mock 进程操作；或路径 ② 显式豁免+注释）
  R1.3 验证：grep supervisor_node coverage_full.txt ≥ 1 条
  R1.4 fleet_manager 排除行补理由
  R1.5 journal 补 P0-I 误宣称更正 + S-1 规矩（coverage 只 ci-bot 提交）
  R1.6 CLAUDE.md 补「同类调用 grep」防线

Wave R2 P0-G 方向反转
  R2.1 motor_ctrl_node declare_parameter 默认 0→50 + 域测断言 Params{} == 50
  R2.2 domain 注释口径统一（"(sim)" → "(fail-safe default)"）

Wave R3 P0-B 竞态根治
  R3.1 grid_mutex_ + 锁内 160KB 快照拷贝 + plan 在快照上执行
  R3.2 并发压力测试（两线程 + 被堵 goal）作为 TSAN 验证门
  R3.3 TSAN 进 CI（可选 job，schedule + dispatch）

Wave R4 收尾杂项
  R4.1 system_secure launch install 路径 + install 空间验证
  R4.2 重启序列超时 + 升级动作（HealthStatus ERROR → supervisor 接管）
  R4.3 ARCHITECTURE.md 改法 1（真话文档）
  R4.4 v2.2.0 钉 6d5697f 不动；R1-R5 完成后发 v2.2.1
  R4.5 CHANGELOG 合并双节 + 补 Wave 1-3 安全修复 + P0-I 更正
  R4.6 防降级计数器持久化（原子写 ~/.ros/amr_ota/）
  R4.7 keystore 0600 自动化；双 CA 分离裁决「真机前置项」已记录

Wave R5 部署链
  R5.1 compose 单 compute 服务 + entrypoint 直通名
  R5.2 package.xml 补依赖
  R5.3 rosdep 重建验证
```

## 五、冲突点汇总（审计 vs 我的方案）

| 冲突点 | 审计立场 | 我原方案 | 裁决 | 理由 |
|--------|---------|---------|------|------|
| R4.3 句子 | 与代码不符，两个改法 | 原句照写 | **审计胜** | 代码事实高于叙事 |
| R3.2 锁窗口 | 百毫秒级，快照拷贝 | 微秒级粗锁 | **审计胜** | 数学错误，已独立复算 |
| R3.1 验证门 | 恒绿不构成验证 | TSAN 零报告 | **审计胜** | 单线程下修与不修都绿 |
| R1.1 根因 | 缺 --ignore-errors | gcno 被清 | **审计胜** | gcno 在场推理成立 |
| R4.4 tag | 钉历史 commit + v2.2.1 | tag -fa HEAD | **审计胜（含我补充）** | v2.2.0 钉 6d5697f 更尊重下游 |
| 双 CA | 应有条目或裁决 | 静默漏项 | **审计胜** | 补显式裁决「真机前置项」 |
| R4.3 改法选择 | 二选一 | — | **选改法 1**（真话文档） | 改法 2 是功能开发，不放在度量诚实波 |
| S-2 升级机制 | DEGRADED 或 supervisor | — | **supervisor 接管** | 用现有机制不加新通道 |

**零 Reject**——审计没有口味问题，全部是断言与事实的冲突。这不是审计太强，
是我的方案在 R4.3/R3.1/R3.2 三处犯了它自己要根治的同类错误（宣称未经
验证）。方案审计的价值正是把这道防线从代码层延伸到了方案层。
