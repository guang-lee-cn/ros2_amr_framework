# 外部 L7 第五审报告 — ros2_amr_framework（四审 W1-W3 整改复核）

> 审计基准：HEAD = **2985015**（2026-09-08 冻结；工作树 clean）
> 对比基线：1697163（第四审基准），4 commits（W1 三 P1 / W2 治理 / W3 未了项 +
> 徽章 chore），25 文件 +589/-42
> 取证方式：全本地，主审逐项亲验（每个 P1 与机制项均命令级复核）
> 关系链：0830 首审 → 0901 复审 → 方案/ADR/ADR 审计 → 0901 三审 → 0907 四审 → 本审
> 性质：整改复审核验（同 0901 复审轮），非全量审计

---

## 1. 总判词

**"四审 11 项发现关掉 9 项、三审挂账四轮的机制活（tag 守卫/TSAN）终于进了 CI——
但 W1 的连带修复亲手把 P0-B 数据竞争模式带回来了：静态屏障注入是全文件
唯一不持锁的 demo_grid_ 写者，而它就坐在三审快照修复的正上方。"**

这轮的整改质量整体是四审以来最高的一波：三个 P1 全部按处方落地（裁决单测红绿
闭环、4/4 launch 接闸、勘误置顶留痕原文保留），且完成了两次自我超越——
自己发现了"衰减语义会侵蚀一次性注入"的次生交互、自己披露了 ab_runner
"/tmp 三度被清"的证据链病根。治理侧首次出现**机制**而非承诺：tag-guard
进了 CI、v2.3.0 重打可达且 force-move 留痕、journal 复刊、伪接线改成
如实的 `enabled: false`。

扣分项集中在两处：一处是新引入的 **N-R1 数据竞争**（详见 §3，一行可修）；
另一处是 README 的两处数字宣称（"55h+72h"、"22 域测"）在两轮 README
修改中被绕开——恰好都是"顺手写"的位置，与四审结论第五次同型。

---

## 2. 四审发现核销表

### 2.1 P1 三项（W1，6940f04）

| # | 四审发现 | 判定 | 证据 |
|---|---|---|---|
| N-1 | 5db0f02 sticky 方向写反 | **根治，红绿闭环** | scan_to_grid.hpp:90 `< INSCRIBED → < LETHAL`（回归注释语义，亲验 diff）；裁决单测 ScanToGridStickySemantics（INSCRIBED 必衰减 + LETHAL 必 sticky，实现在 5db0f02 版本必红——commit 记录红态实证 actual 253 vs 253）；**连带修复**：静态注入 on_configure 一次性 → 每感知 tick 重刷（自愈屏障，抵消 clearing 侵蚀）——次生交互是作者自己发现的。白盒锚点用全局前向声明 friend，域头零 gtest 依赖（设计权衡有注记）。**但连带修复引入 N-R1（§3）** |
| N-2 | 2/4 launch 绕闸 | **根治（4/4 接闸）** | nav2_scene:79-82,88-90 controller/behavior remap /cmd_vel→/cmd_vel_raw + guard 节点 respawn（亲验 diff）；nav2_demo 同构（grep cmd_vel_guard ×2）；冒烟记录"带闸巡航 SUCCEEDED + 5 次干预零误拦"。小注：guard 参数字面量 50 与 kDefault 重复（节点默认已引用单一事实源，launch 覆写属冗余无害） |
| N-3 | soak 证据链三缺陷 | **勘误根治，数据仍不入库** | 报告置顶勘误（原文保留）：①§4 假根因更正为"零到站"（goals.csv 55h 仅 1 条事件=初始 goal，佐证假设）；②751m 撤回为"震荡位移非导航"，结论范围收窄到 supervisor 生命周期/内存/僵尸；③guard=0 补披露。**透出新事实：新一轮 72h soak 在跑（勘误时 36h，零事件同因）**。数据工件仍只在容器内 |

### 2.2 三审未了项（W3，2985015）

| # | 项 | 判定 | 证据 |
|---|---|---|---|
| ① | tag 守卫机制化 + v2.3.0 重打 | **根治（机制级）** | ci.yml tag-guard job：全部 `v*` tag 逐个 merge-base 可达性检查 + 三宇宙对齐（package.xml==CHANGELOG 最新节==最新 tag），fetch-depth:0 配置正确（亲验）；v2.3.0 → 20b34c4 可达（亲验 merge-base 通过），force-move 治理动作留痕 journal。四轮催办后终于从"提醒"变成"红线" |
| ⑤ | TSAN 跑一次 + 证据 | **大体关闭** | tsan-nightly job（cron 02:00 北京，schedule+dispatch）入库；runbook §5 回填首跑证据（2026-09-08 本地，decision 8 测零 TSAN 报告，含 P0-B 并发压力锁，setarch -R 经验入注）。扣：nightly 首个 cron 结果未出（今晚）；"修复前 TSAN 必红"仍未 retrospective 实证（P0-B 修复早于 TSAN 首跑） |
| ③ | R4.1/R4.7 显式裁决 | **根治（裁决链完整）** | 20260908-r43-dds-security-adjudication.md：R4.1 不修、system_secure 降级开发态参考（文件头注记落地，亲验）；R4.7 0600/双 CA 推迟真机批次挂账——带理由带风险接受，非沉默漏项 |
| ⑥⑧ | 告警接线/伪接线 | **诚实化处置** | production/staging yaml `alerts.enabled: false` + 裁决注记（"此前构成已接线假象"自认）+ alertmanager 清空回填位；README:63 "告警规则已入库未接线"（原句"未实现"两轮存活后修正）。真接线 roadmap。**处置路径正确：先消灭假象，接线另排** |
| ② | README 告警句 | **已修**（见上） | — |

### 2.3 四审 P2/P3 项

| # | 项 | 判定 | 证据 |
|---|---|---|---|
| N-4 | ab_runner 不存在 | **根治** | toolkit/ab/ab_runner.py 入库（103 行）：双模式 goal 注入（nav2 action / custom 话题 ×3）、到达判据 /odom 0.3m+静止 2s、站间冷却 2.5s（docstring 记录 0.5s 连发的 0.57s 假成功教训）、基线数字入 docstring。docstring 自曝"/tmp 副本三度被清理"——证据链病根的诚实披露 |
| N-5 | 告警伪接线 | **已处置**（见 ⑥⑧） | — |
| N-6 | README 4 处失真 + package.xml 恶化 + journal 断更 | **部分** | ✓ 全闭环句→准确表述（W2）；✓ 告警句（W3）；✓ package.xml 去重（亲验 4 项各 ×1）；✓ journal 断更周补记+复刊。✗ **:151 "55h+72h soak" 原句未动**（新一轮在跑不构成对旧轮的进位改写）；✗ **:154 "22 域测" 原句未动**（test_collision_guard.cpp 仍 18 TEST_F，亲验）；✗ deployment-plan.md:180-182 反向过时未动 |
| N-7 | ARCHITECTURE 主图/头 + 头注释残留 | **半修** | ✓ 文档头（v2.4.0/2026-09-08/导航形态行，亲验）；✓ nav2_params_localized:4 头注释；✓ system.launch docstring。✗ **两张主 mermaid 仍未重画**——fusion→decision→motor 仍是"系统全貌"，NAV2 生产链不在图上 |
| N-8 | 安全修复零回归锁 | **部分** | ✓ sticky 语义裁决单测（N-1）；✓ BlockedGoal flaky 加固（窗口内重复发布+环境归因记录）；✗ supervisor waitpid 错误路径残留与回归锁未动（supervisor_node.cpp 本 delta 零变更） |
| N-9 | respawn 宣称/数字宣称 | **代码侧补齐** | guarded/localized 的 scene_simulator 补 respawn（亲验 +1 行）——四审"最该 respawn 的单点"关闭，ADR 宣称从字面为假变为真；✗ 22 测/79 vs 109/环境规格 4C8G vs 2C2G 未动 |
| N-10 | cloud_soak_all.sh 顺序缺陷 | **未动** | scripts/ 本 delta 零变更 |

---

## 3. 新发现问题

### N-R1（P1）：静态屏障注入是全文件唯一无锁的 demo_grid_ 写者——P0-B 数据竞争模式复发

**事实链**（全部亲验）：

1. N-1 连带修复把注入从 on_configure（单线程初始化，无竞争窗口）移入
   on_perception 回调顶部：decision_node.cpp:179
   `if (static_obstacles_) inject_static_obstacles();`
2. `inject_static_obstacles()`（:162-175）执行 ~26 次 `grid_updater_.inflate
   (demo_grid_, ...)` 写操作，**函数体内无任何锁**；
3. 同文件其余所有 demo_grid_ 访问全部持 grid_mutex_：:215 raytrace、
   :246-247 单点 inflate（对比鲜明——一个格子的写都上锁）、:284-285 plan 侧
   160KB 快照拷贝（P0-B 修复本体）；
4. compute_container.cpp:73 是 **MultiThreadedExecutor**——perception 订阅
   回调与 plan 定时器回调可真并发；
5. :179 的无锁写 vs :284 的持锁快照读 = std::vector 存储的并发无同步读写，
   UB。撕裂数据的可见后果：A* 在半注入屏障上规划。

**为什么全绿测不出来**：W1 验证清单（scan_to_grid 5/5 + decision 8/8 + e2e 3/3）
全是功能断言；test_grid_race 锤的是"raytrace 线程 × plan 线程"，注入路径
不在其覆盖（亲验 grep 零引用）；TSAN 首跑跑的是 decision 8 测（单回调形态），
同样不含注入并发。

**触发范围**：`static_obstacles:=true` 形态（simulation.launch 显式开启；A/B
基线路径）。默认 false 形态不触发——这是它没在 e2e 里炸出来的原因，也是
"仿真验证通过"再次弱于并发验证的例证。

**修复**（一行级）：注入调用搬进 :215 的 grid_mutex_ 临界区（raytrace 之前），
或函数内自持锁；回归锁把注入线程加进 test_grid_race 的双线程变体
（注入 × plan 快照）。**修 bug 时 grep 同文件同类调用是否也需同修**——
CLAUDE.md 元防线第一条，本案恰是搬移代码时未对齐目标区域的锁约定。

### N-R2（P2）：README 两处数字宣称在两轮修改中被绕开

:151 "55h+72h soak"与 :154 "22 域测"——四审 N-6 点名，W2/W3 两轮 README
修改都路过此文件（各改 2 行与 5 行），两处原句未动。22 = 18 TEST_F + 3 e2e
凑不出；"55h+72h"需等新一轮跑完才成立而勘误已撤回 751m 叙事。README 的
Tech Stack 表自己写着"用例数以 CI 实测为准，勿手写——数字漂移第 4 次教训"，
这两行就是第 5 次。

### N-R3（P3）：次生观察

- ARCHITECTURE 主图未重画（N-7 遗留半项，工作量原因可理解，但"图展示结构"
  的文档规约下，系统全貌图与生产链不符比表格失真更严重）；
- 新 72h 轮在跑：若收尾时数据仍不入库，四审 N-3 的"证据叙事化"将以同一
  形态复发第二轮（勘误只修了话，没修证据习惯）；
- tsan-nightly 今晚首个 cron——红绿结果应回填 runbook（机制上线≠机制验证）。

---

## 4. 评分卡（对比四审）

| 维度 | 四审 | 本审 | 依据 |
|---|---|---|---|
| 架构与分层设计 | 8.5 | 8.5 | 无变化（4/4 接闸属安全维度；主图仍未更） |
| Domain 算法与单测 | 8 | **8.5** | N-1 根治 + 裁决单测红绿闭环 + 自愈屏障（次生交互自发现）；friend 锚点属可接受权衡 |
| Infrastructure 并发/内存 | 7 | **6.5** | **N-R1：唯一无锁写者，P0-B 模式复发**；TSAN nightly 上线（+）抵不住一个活竞态（−） |
| 安全工程 | 6 | **6.5** | 4/4 launch 接闸 + respawn 补齐 + guard=0 披露 + R4.1/R4.7 显式裁决；2s 无保护窗口与倒车放行维持原状 |
| 可观测性 | 5.5 | 5.5 | 伪接线诚实化（+）；真接线仍 roadmap、数据仍不入库（−） |
| 测试工程体系 | 6 | **6.5** | TSAN 首跑+nightly、裁决单测、BlockedGoal 加固、ab_runner 可复现；扣：注入路径零并发覆盖 |
| 文档-代码一致性 | 5.5 | **6** | 勘误置顶留痕、三处头注/头注释修、journal 复刊；扣：README 两处数字、主图 |
| 发布与治理 | 4.5 | **5.5** | **tag-guard 机制化（四轮催办后的第一根红线）**+ v2.3.0 重打留痕 + 去重 + 裁决文档化；扣：README 数字两处 |
| 部署运维就绪 | 4 | 4 | 本波无部署面变化；cloud 脚本缺陷未动 |

**综合：约 6.5 → 约 6.5（净持平，构成改善）。** 关闭量大于引入量（P1 3 关
1 引、三审机制 4 关、治理分项 +1.0），但新引入的是并发 UB 级缺陷——
**N-R1 修复 + 注入并发锁入 test_grid_race 后，综合 7.0 成立**；若 README
两处数字顺带对账，四个审计周期第一次出现"零已知 P1/P2 宣称失真"状态。

---

## 5. 落地距离

**8-13 人月维持。** 本波全部是信任修复（宣称对账/机制红线/证据链勘误），
无能力增量——这是正确的一波（四审后最缺的是可信度），但距离数字不动。
新一轮 72h soak（进行中）若以入库工件收尾 + NAV2 上机预调启动，下轮可
下调至 8-12。四大约束块（真机/安全过程/认证/供应链）状态不变。

---

## 6. 修复优先序（下波）

1. **N-R1 注入锁修复**（一行搬移）+ test_grid_race 增加注入×快照双线程
   变体——当前唯一的活 P1，且是"元防线第一条"（同类调用 grep）的教科书案例
2. README :151/:154 两处数字对账（顺带 deployment-plan.md:180-182）——
   第 5 次数字漂移，建议这次真把"勿手写数字"落成 lint 或 CI 检查
3. 新 72h 轮收尾三件事：数据工件入库（CSV 子集）、goals 零事件如实入报告、
   tsan-nightly 首夜结果回填 runbook
4. ARCHITECTURE 两张主 mermaid 重画（NAV2 生产链为全貌 + 自研管线标 A/B 基线）
5. cloud_soak_all.sh 顺序缺陷 + 入口清场（N-10，第三次挂账）
6. 未了小项批：supervisor 错误路径僵尸回归锁、mock_amcl/旧 localization
   平行资产收敛标注、guard respawn 2s 无保护窗口（scene_simulator 加指令
   staleness 或显式裁决）、79 vs 109 计数调和、环境规格统一

---

## 7. 结语

四轮审计的曲线在这波出现了拐点信号：**机制第一次跑赢了提醒**——tag-guard
从三审的"一行 CI 建议"变成红线，v2.3.0 从死 tag 变成可达且留痕的治理动作，
伪接线从"看起来已接线"退回如实的 false。连带修复的自发现（衰减侵蚀注入）
和证据链病根的自披露（/tmp 三度被清）都说明复盘文化在向下渗透。

但 N-R1 是一记警钟：**修复本身也是代码变更，也欠同等的并发审视**。把初始化
代码搬进回调、给共享栅格加一个写者——这类"顺手的连带修复"正是三审 R3 波
用锁与快照封死的那扇门重新打开的缝隙。CLAUDE.md 元防线第一条写的就是它：
修 bug 时 grep 同文件同类调用。下一步只需要一行搬移 + 一个测试变体，
这是四个审计周期里离"零已知 P1"最近的一次。

---

**取证索引（亲验项加 ※）**：※scan_to_grid.hpp:87-93 修复 diff + 裁决单测 ·
※decision_node.cpp:162-179（无锁注入）vs :215/:246-247/:284-285（持锁写者）
+ compute_container.cpp:73 MultiThreadedExecutor · ※test_grid_race/test_decision
零注入覆盖 · ※nav2_scene 双 remap+guard diff · ※ci.yml tag-guard/tsan-nightly
（fetch-depth:0）· ※v2.3.0→20b34c4 可达 · ※package.xml 4 项各 ×1 ·
※README:63,151,154,169 现句 · ※production.yaml alerts.enabled:false ·
※20260908-r43 裁决文档 + system_secure 头注 · ※ab_runner.py 实物 ·
※tsan-runbook §5 首跑记录 · ※journal 尾部复刊 · ※guarded/localized scene
respawn +1 · ※ARCHITECTURE 头 diff（仅 3 行，主图未动）
**未核项**：tag-guard/tsan-nightly 的 CI 运行时绿（静态验证；nightly 首夜
今晚）；新一轮 72h soak 数据（容器内，未入库）
**文档版本**：1.0（2026-09-08）
