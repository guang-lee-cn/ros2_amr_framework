# 外部 L7 复审报告 — ros2_amr_framework（Wave 1-4 整改核验）

> 复审基准：HEAD = 6d5697f（2026-08-31），对比基线 a2abc70（首轮审计，见
> 20260830-external-l7-engineering-audit.md）；整改区间 13 commits，diffstat 34 文件 +1012/-125
> 口径与首轮一致：只认代码事实与 CI 产物；commit message / 整改文档 / 注释 / journal 叙事
> 不作为"已修复"的证据，只作为"宣称"接受核验
> 方法：三路独立复验（并发内存 / 安全信任链 / 度量治理与宣称），覆盖率类宣称用 **CI 自己
> 的产物**（origin/main = 62632fb，ci-bot 提交）交叉验证，不用作者本地产物；关键指控由主审
> 亲手复算（P0-G 默认值链、P0-I 分母构成与 3030/3495=86.70%、v2.2.0 tag 可达性）

---

## 1. 总判词

**"Wave 1-3 是教科书级的根治；Wave 4 —— 宣称对账的那一波 —— 本身产出了一条新的失实宣称。"**

整改的真实成色是两极的。安全兜底链上的代码缺陷（UAF、结构性死锁、验签不绑内容、独立进程饿死）以带复现证据、带验证门的方式真实关闭，ASAN/-Werror/cppcheck 三道门禁把"一个人的闭环"部分变成了"仓库的机制"，README 把假话改成了真话。治理从"人"向"机制"的移动是可测量的。

但本轮复审最重的发现是：**对自己整改宣称的验证标准，忽然低于对代码的标准。** P0-I 的 `--initial` 机制写进了脚本注释、CHANGELOG、journal、commit message 五处"已验证"叙事，却没有一处经过最便宜的验证——`grep supervisor_node coverage_full.txt`。CI 自己的产物（62632fb）证明它零效果，而它与 80% 门禁的数学不相容（真生效即 ~78.7% 必红）意味着任何一次认真的端到端跑都能发现真相。同一块"真数假全"的石头，三周内摔了第二次，这次带着"已验证"的帽子。P0-G 则把 fail-safe 配给了仿真、把 fail-open 留给了真机——与审计要求正好相反。P0-B 被 runbook 化后宣称"十项全部关闭"，P0-K 静默漏出排期成为 12 项 P0 中唯一无去向的一项。

一句话：**代码缺陷在收敛，"不装"的文化在本波出现了选择性失明。**

---

## 2. P0 逐项核验（12 项全量）

判定词：根治 / 大体根治 / 半修复 / 表面修复 / 未动 / 引入新问题

| # | 首轮发现 | 判定 | 核验要点 |
|---|---|---|---|
| P0-A | 感知主链路 stack-use-after-return | **根治** | 值语义改造完成；ASAN CI job（ci.yml asan-gate）36/36 零报告，旧代码实报 → 新代码干净的双证在案 |
| P0-B | demo_grid_ 跨线程数据竞争 | **未动（且被宣称关闭）** | decision_node.cpp 的 demo_grid_ 零锁如故；ASAN 进 CI 补的是 UAF 检测，对数据竞争无效；TSAN 仍是"手册态"未进 CI；执行计划宣称"十项全部关闭或显式裁决"——此项既未修也未裁决 |
| P0-C | health_monitor 重启结构性死锁 | **根治 + 引入活锁缺口** | 异步状态机真实落地（health_monitor_node.hpp:51-64，独立 callback group 链式推进）；test_health_restart 是教科书级行为验收（生产同形态 SingleThreadedExecutor，断言 activate 计数与心跳恢复）。缺口：重启序列无超时——transition 响应永不到来则挂死，且 P1 的 HEALTH_GATE 超时 watcher 原样缺席 |
| P0-D | OTA 验签不绑镜像内容 | **根治（域内）** | manifest = {version, sha256, size} 整体签名，verify 复算磁盘镜像实际字节；"合法签名+篡改内容"反例测试真实存在且断言到槽位文件级。三个保留项：① `run_update(target, /*signature_valid=*/true)` 硬编码旁路 domain 签名门，防线单点系于 fetch lambda；② 整镜像读入 std::string 的内存形态；③ verify→install 窗口 TOCTOU 无二次校验 |
| P0-E | 信任锚可运行时替换 | **大体根治** | 公钥构造期一次性读文件，运行时 param set 换锚已失效；launch 期注入窗口仍在（默认 DDS 不加密前提下非纯理论） |
| P0-F | 私钥随 install 分发 | **半修复 + 引入新断裂** | install 排除 sros2/private ✓（CMakeLists EXCLUDE）；本机 chmod 0600 ✓（整改当日 ctime 物证）。但修的是本机状态不是机制——换机器重建 keystore 0644 会回来；identity/permissions 双 CA 仍同钥软链。**新问题：launch 仍从 install 空间引用被排除的路径，system_secure.launch 在 install 形态从此必坏** |
| P0-G | 真机默认 fail-open | **表面修复，方向修反** | domain 默认翻转为 50（collision_guard.hpp:66）✓；但唯一生产消费者 motor_ctrl_node 紧接着 `declare_parameter("guard_min_valid_echoes", 0)` 覆写回 0。净效果：**fail-safe 配给了仿真**（simulation/supervised_sim launch 显式 50），**fail-open 留给了真机**（system.launch 无覆盖 → 默认 0 → 08-17 撞货架模式默认直行）。无任何测试断言默认值；注释本身就把 50 框定为 "(sim)" 设置。两路复验独立发现，主审亲手复核证据链闭合 |
| P0-H | 无硬件安全回路接口 | **未动（已显式裁决延后）** | 执行计划"明确不排期"清单含此项——诚实，但真机门槛不因此降低 |
| P0-I | 覆盖率分母漏零覆盖文件 | **表面修复 + 引入失实宣称** | 详见 §4（本轮最重发现） |
| P0-J | CI 徽章断链无告警 | **根治（对已知根因）** | ci.yml:156-160 push 失败置红 + rebase 重试一次；机制已被远端 62632fb（ci-bot）实证存活。残余盲区：无独立 staleness 断言（workflow 整体被 skip 仍无告警） |
| P0-K | 核心数据流表不成立 | **未动（静默漏排）** | 代码、文档、排期**三重零**：`git diff a2abc70..HEAD -- launch/ doc/ARCHITECTURE.md` 为空；执行计划四波均不含 P0-K，"明确不排期"清单也不含——既没修、没排、也没显式裁决不做。12 项 P0 中唯一无去向的一项，比"裁决不做"性质更差 |
| P0-L | motor_ctrl 独立进程碰撞保护旁路 | **根治** | 饿死问题按交付物形态修复（细节见并发路复验）；handle_goal 增 NaN 入口校验 |

**计分：根治 6（A/C/D/E/J/L，其中 C 带新缺口、D 带保留项、J 带盲区）· 大体→半修复 2（E/F）· 表面修复 1（G）· 未动 3（B/H/K，其中 H 显式、B/K 静默）· 另引入新问题 4 起（见 §3）。**

---

## 3. 本轮新引入问题（整改自带的回归）

| # | 问题 | 证据 |
|---|---|---|
| N-1 | **真机传感器失明默认被节点参数覆写回 0**（即 P0-G 方向修反的净效果） | motor_ctrl_node.cpp 参数声明默认 0 vs collision_guard.hpp:66 域默认 50 |
| N-2 | **system_secure.launch 在 install 空间必坏**：CMake EXCLUDE 排除了它引用的 keystore 路径 | CMakeLists install 规则 vs launch 引用路径 |
| N-3 | **健康重启序列无超时**：async 状态机修复死锁的同时引入"响应永不到来则永久挂起"的活锁面；HEALTH_GATE 断电回滚 watcher 仍无 | health_monitor_node.hpp 重启链路 |
| N-4 | **度量工件口径混乱**：6d5697f 把作者本地 WSL 产物（绝对路径）混入 docs commit，CI 随后在 62632fb 改回相对路径——两套口径来回覆盖 | 6d5697f vs 62632fb 的 coverage_full.txt diff |

另有一项**版本拓扑**问题：数字三宇宙对齐了（package.xml 2.2.0 + CHANGELOG `[2.2.0]` + tag v2.2.0），但 **v2.2.0 打在 history rewrite 后不可达的死 commit 上**（`git merge-base --is-ancestor` 不通过，`git describe HEAD` 仍是 v0.1.0-260-g6d5697f——主审亲手验证）；CHANGELOG 出现双 2.2.0 节，且 **Wave 1-3 的安全修复全部没进发布说明**。发布叙事与安全叙事脱节：修信任链的版本，发布说明里看不见。

---

## 4. P0-I 专项：机制零效果，且被 CI 自己证伪

这是本轮方法学上最重要的一条，单列。

**代码改动是真的**：quality.sh:50-52 加 `lcov --initial --capture`，合并逻辑在 :64-69。

**但机制未生效**，证据链（主审亲手复算）：

1. 修复进入仓库在 ed1eeff（08-31 22:35）；而 86.x%/59 文件首次出现于 212d598（08-31 18:11，ci-bot）——**比修复早 4 小时**。数字下降的真实来源是 4ba683e 新增 test_health_restart 带来的 gcda（health_monitor_node.cpp 以 53.4% 进分母），不是 `--initial`。
2. 含修复的 quality.sh 在 CI 完整跑过（run 33406894635，12m38s，success），产出的 62632fb 中 coverage_full.txt 仍为 59 文件、3030/3495 = 86.70%（主审按 CI 产物逐位复算 ✓）——`--initial` 在 CI 上零效果。
3. **supervisor_node.cpp（356 行，零测试）仍从分母消失**：它属 amr_supervisor 目标（CMakeLists:126-128）、全局 --coverage 必然生成 gcno、remove 列表不排除它——不在，只能是 initial capture 链路失败。分母里只有 domain 侧 supervisor_policy.hpp（86.6%），最容易被误读为"supervisor 进分母了"。
4. **与自家门禁数学不相容**：若 initial 真生效，仅 supervisor_node 一个文件就使总分母变为 3030/3851 ≈ 78.7% < 80% 门禁（quality.sh:106），CI 应当红。修复与门禁阈值根本不兼容，证明没有人端到端验证过它。
5. `|| true`（:52）+ `-s` 空文件跳过（:65）构成静默失败通道，与同文件 :33-36 刚立下的"度量链断则门禁红"原则直接自相矛盾。
6. fleet_manager 仍显式排除（:75），排除本身可以，但**豁免理由零声明**——同文件其他注释详尽，唯独排除 pattern 无一行解释。

**失实宣称进入五处正式记录**：quality.sh:47-49 注释、CHANGELOG 2.2.0 节、docs/change_journal.md、6d5697f commit message（"P0-I 直接验证"）。其中 6d5697f 的 coverage_full.txt 是作者本地产物手动混入，非 CI 同步——"徽章已自动同步"与"P0-I 已验证"两句话里，只有前半句是真的。

---

## 5. 评分卡变化（对比 2026-08-30 基线）

| 维度 | 首轮 | 本轮 | Δ | 依据 |
|---|---|---|---|---|
| 架构与分层设计 | 8 | 8 | — | 本轮无新增发现，红线持续守住 |
| Domain 算法与单测 | 8.5 | 8.5 | — | 无变化 |
| Infrastructure 并发/内存安全 | **3** | **5.5** | +2.5 | UAF/死锁/饿死三 P0 关闭（A/C/L），ASAN+UBSAN 独立 CI job、-Werror 零警告基线真实；扣分：demo_grid_ 竞态零防线（B），TSAN 纸面，重启活锁缺口 |
| 安全工程（OTA/DDS） | **4** | **5.5** | +1.5 | 验签绑内容+公钥钉住+install 排私钥真实落地且有反例测试；扣分：signature_valid=true 单点旁路、真机默认 fail-open 方向修反（G）、双 CA 同钥、默认 DDS 无 security、防降级计数器重启归零 |
| 可观测性 | 5 | 5 | — | 实质未动（trace 死代码/无告警规则/日志无轮转原样）；README 诚实化计入文档维度 |
| 测试工程体系 | 5.5 | **6.0** | +0.5 | 加分：test_health_restart 教科书级行为验收、NaN 测试走真实 DDS 路径、三道门禁机制化。扣分：P0-I"真数假全"二次摔倒且带"已验证"帽子、fusion CRITICAL/恒真断言/SetParamUnknown 三笔测试债零触碰、P0-B 无自动防线 |
| 文档-代码一致性 | **4.5** | **5.0** | +0.5 | 加分：README 四处对账实改且抽查与代码一致（告警"未实现"诚实标注是正确示范）。扣分：P0-K 三重零、失实句进 CHANGELOG/journal/注释三处、"同机多实例"仍乐观（launch 只起一台） |
| 发布与治理 | **3.5** | **4.5** | +1.0 | 加分：版本数字对齐+annotated tag、badge push 失败即红且实证存活、cppcheck 崩溃红、asan-gate/werror 机制化。扣分："十项全部关闭"的裁决记录本身失实（B 未关、I 半真）、package.xml 漏 8 依赖、contracts/msg 僵尸/compose/hal 纯度全数未动、`|| true` 在新代码里复发 |
| 部署运维就绪 | **2.5** | **3.0** | +0.5 | Dockerfile 语法级修复；但 compose 三个核心服务仍指向不存在的可执行（fusion_node/decision_node/motor_ctrl_node），up 必挂；systemd/soak/告警/环境分化仍零 |

**综合工程成熟度：约 5.5 → 约 6.0 / 10。** 位移真实但有限；构成上，涨分全部来自"把已知缺陷关掉"，零分来自"跨过新门槛"（真机/长时/安全过程/部署链均无进展）。

---

## 6. 宣称 vs 实际（本轮版，精选）

| 宣称 | 来源 | 判定 |
|---|---|---|
| "真分母 86.7%（54→59 文件）" | badge.json / coverage_full.txt | **半真**——数字可复算 ✓、下降真实 ✓；但下降来自新测试，`--initial` 机制 CI 实测零效果，supervisor_node 仍以"不存在"表达 0% |
| "supervisor/health_monitor 等不再从分母消失" | quality.sh:47 / CHANGELOG / journal | **不实**——CI 产物 62632fb 证伪 supervisor 部分；health_monitor 为真但归因错误 |
| "徽章已自动同步（P0-I 直接验证）" | 6d5697f commit message | **误导**——前半句真（机制活着），后半句假；且该 commit 里的 coverage_full.txt 是本地产物混入 |
| 看门狗两分法 + "行为测试实证" | README:50-52 | **属实**——异步状态机真、行为测试真且生产同形态 |
| "告警规则未实现（观测只看不叫）" | README:51-53 | **属实且诚实**——把假宣称改成真话的正确示范 |
| fleet_multi "单机演示形态：同机多实例" | README:64-66 | **半真**——治理未实现的承认 ✓；但 launch 只起一台、话题无 per-AMR 隔离，"多实例"名不副实依旧 |
| 版本三宇宙对齐 2.2.0 | package.xml/CHANGELOG/tag | **半真**——数字三处一致 ✓ + annotated tag ✓；但 tag 打在死 commit 上不可达，describe 仍 v0.1.0-260；CHANGELOG 双 2.2.0 节漏 Wave 1-3 安全修复 |
| toolkit Docker 修复 | Dockerfile:32-38 | **半真**——Dockerfile 真修了（语法级正确）；compose 三核心服务指向不存在的可执行，up 必挂 |
| "三审修复序十项全部关闭或显式裁决" | 执行计划 §5 / journal | **不实**——P0-B（demo_grid_）既未修也未裁决，TSAN runbook 是计划不是防线 |
| ASAN 进 CI / -Werror / cppcheck 崩溃红 | ci.yml:41 / CMakeLists:11 / static_analysis.sh:36 | **属实**——三项均落地，本轮抽查通过 |

---

## 7. 落地距离（更新）

**阶段判定不变：仿真闭环完备的原型末期，尚未跨过工程样机门槛。**

Wave 1-4 关掉的是"会炸的代码"清单的一半——这降低了试点期间的故障风险，但落地距离的**构成几乎没变**：真机 bring-up（sick_tim781 从未上硬件）、安全软件过程、部署运维链（systemd/告警/soak/环境分化）、供应链（package.xml 漏依赖、rosdep 不可重建）四块零推进。

| 目标 | 首轮估算 | 本轮估算 | 变化 |
|---|---|---|---|
| 单机商用试点 | 10-18 人月 | **9-16 人月** | 边际：已关闭的 P0 减少返工风险；四大约束块未动 |
| 可售量产 | 25-40 人月 + 认证周期 | 不变 | ISO 3691-4 双通道决策仍需前置 |

---

## 8. 剩余硬门槛（真机/客户部署前必须关闭，按序）

1. **P0-G 真机默认反转**：motor_ctrl_node 的参数默认改 fail-safe（50），空旷场景部署侧显式豁免——一行改动 + 一个断言默认值的测试，这是当前唯一还在"出厂即 08-17 模式"的项
2. **恢复链路活性闭环**：重启序列加超时 + HEALTH_GATE 断电 watcher + 防降级计数器持久化（重启归 8 的现状让"C++ 生产 agent 弱于自己的 shell demo"继续成立）
3. **P0-B 实修**：demo_grid_ 加锁/双缓冲，或论证降级 MutuallyExclusive；TSAN 从手册进 CI（哪怕 nightly）
4. **度量诚实最小闭环**：修 initial capture 或诚实降级（supervisor 以 0% 显式入列 + 豁免注释）；把 80% 门禁与真分母的冲突摆上桌面裁决；quality.sh initial 步骤去 `|| true` 加非空断言；journal/CHANGELOG 补 P0-I 误宣称更正记录
5. **P0-K 显式裁决**：改 ARCHITECTURE.md 一句话或改 launch 默认，消灭静默遗漏
6. **v2.2.0 tag 重打**到可达 commit + CHANGELOG 补 Wave 1-3 安全修复条目
7. **私钥机制化**：keystore 生成脚本强制 0600（而非本机手工 chmod）+ 双 CA 分离 + system_secure.launch install 路径修复
8. **默认 DDS security**（或显式风险裁决记录）
9. **compose 可执行名修复**（若 toolkit 仍在交付物）+ package.xml 补 8 依赖并跑通 `rosdep install` 重建验证

---

## 9. 结语

首轮审计说这个仓库"最可贵的是不装的文化，最危险的是宣称与实际的落差聚在安全兜底链"。本轮这两句话都需要修正：

- 代码侧的落差确实在收敛——安全兜底链的五个 P0 关掉了三个半，且质量高。
- 但"不装"出现了新的失效模式：**对代码缺陷极其诚实，对自己整改宣称的验证忽然放松**。P0-I 五处"已验证"叙事、P0-B 的 runbook 化关闭、P0-K 的静默漏排，全部发生在"宣称对账"这一波里。验证自己宣称的纪律，应当和对代码的一样硬——`grep` 一下、端到端跑一次、让 CI 红一次，都比五处叙事便宜。

能力从来不是这个仓库的短板（Wave 1-3 是证据）；把每一句"已修复"都当成一条待证伪的假设，才是下一波要修的东西。

---

**复审分工**：并发内存 / 安全信任链 / 度量治理与宣称，三路独立取证后交叉汇总；覆盖率宣称以 CI 自身产物（62632fb）为准
**关键指控主审亲手复核**：P0-G 默认值链（domain 50 / node 0 / sim 显式 50 / system 无覆盖）、P0-I 分母构成与 3030/3495=86.70% 复算、v2.2.0 tag 不可达
**与首轮报告的关系**：20260830-external-l7-engineering-audit.md 的 P0-A..L 逐项追踪至此；评分卡沿用同维度口径
**文档版本**：1.0（2026-09-01）
