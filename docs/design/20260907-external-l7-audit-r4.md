# 外部 L7 第四审报告 — ros2_amr_framework（NAV2 收敛 + 72h soak 周期核验）

> 审计基准：HEAD = **1697163**（2026-09-07 冻结；工作树 clean）
> 对比基线：140e27a（第三审基准），45 commits，62 文件 +2608/-127
> 取证方式：本地优先 + 三路并行取证（NAV2 收敛 / soak 与穿货架 / 治理宣称），
> 最重发现全部经主审命令级亲验（枚举值、diff、行号原文复核）
> 口径：与前四审一致——只认代码事实与入库工件；commit message / 注释 / 报告叙事
> 只作宣称接受核验；数据不入库 = 叙事不算证据
> 关系链：0830 首审 → 0901 复审 → 方案审计 → ADR → ADR 审计 → 0901 三审 → 本审

---

## 1. 总判词

**"这一周完成了项目史上最大的能力跃迁（NAV2 收敛 + 首个 55h soak），也埋下了
四审以来最重的一颗代码地雷：一个号称修复'动态障碍永久堵路'的安全修复，方向
写反了。证据链同时从工件退化为叙事——soak 与 A/B 的核心数字全部没有入库工件。"**

NAV2 收敛决策（bff7ab5）本身是本仓库迄今质量最高的架构治理工件：A/B 实证、
世界模型归因、退役路径明写"本决策不宣称完成"。生产链路（nav2_localized /
nav2_guarded）上安全闸真实串接、单一事实源保持。但四条宣称在落地层断裂：
"接线别绕过"被 2/4 的 launch 违反、"三份 launch 配 respawn"字面为假、A/B
跑台的执行器（ab_runner.py）从未存在、"CollisionGuard 22 测试矩阵"实际 18 个。

72h soak 的进程层结论（零僵尸、RSS 平稳）代码侧可信，但任务层存在一个未被
追问的空洞：**goals.csv 55 小时零记录 + 751m/55.1h ≈ 13.6 m/h，与"全程活跃
导航"自相矛盾**——报告把空流定性为"判定器 bug"，而其给出的根因陈述
（"patrol 的目标不走该记录路径"）在代码层面为假（patrol_3c.py:29-50 发布
/goal_pose，soak_monitor.py 自首个提交起订阅落盘）。最自洽的解释是
**55 小时零到站**——soak 证明的是进程韧性，不是任务成功。

治理层：三审清单 8 项 5 项未动，journal 在最需要记录的一周断更（最后条目
09-01），README 两次重写后新增 4 处失真（含"三轮外部审计整改全闭环"——
发布时点 5/8 未动）。**"commit message 松、README 松"模式第四次出现，
且这次加了一个新维度：config/env/*.yaml 写下了不存在的告警接线。**

---

## 2. 三审修复清单核销（§7 八项）

| # | 三审要求 | 判定 | 证据 |
|---|---|---|---|
| ① | v2.3.0 重打 + CI tag 守卫 | **未动（双项皆零）** | v2.3.0 → 5decc24 → 9a45e51 仍非 main 祖先（亲验 merge-base 失败）；ci.yml 零 tag 校验 job；`.github/` 在本 delta **零变更**。v2.4.0 干净可达、与 package.xml/CHANGELOG 三方对齐——但那是手气不是机制 |
| ② | README 再对账（告警 :52） | **未动（两次重写保留原句）** | README:63 仍写"告警规则未实现"，amr_alerts.yml 在库（亲验原文） |
| ③ | R4.1 / R4.7 显式裁决 | **未动** | CMakeLists.txt:180-183 仍双 EXCLUDE；verify 脚本改走 /tmp 自建 keystore——绕开而非修复；0600 全仓零自动化 |
| ④ | package.xml 去重 | **未动且恶化 3→8** | :15-22 与 :33-40 整块重复；亲验 ros_gz_sim/slam_toolbox/openssl 各 ×2，agent 计 8 对 |
| ⑤ | TSAN 跑一次 + 证据入 journal | **未动（占位诚实）** | ci.yml 零 tsan/cron；tsan-runbook.md:39 仍"（待首次夜跑回填）"——占位是诚实的，但四审无一实跑 |
| ⑥⑧ | 告警接线 + Alertmanager | **未动，且倒退为伪接线** | toolkit/compose 零 prometheus 引用；新增 config/env/{staging,production}.yaml 写 `alerts.enabled: true` + `alertmanager: localhost:9093`，全仓零消费者——比"零接线"更危险，制造"已接线"外观（亲验 grep 空） |
| ⑦ | health_monitor QoS 手搓 | **根治（冻结后 6h 完成）** | e4a8c6a（09-01 16:52）"27 处裸 QoS 清零"：亲验 `src/infrastructure/*.cpp` 手搓 `rclcpp::QoS(` 计数为零，仅词汇表本体 4 处；残留限测试/基准域（可接受） |

---

## 3. NAV2 收敛核验（本 delta 主题一）

**总判定：决策质量 A、生产链路落地 A-、宣称层落地 C+。**

| 项 | 判定 | 证据 |
|---|---|---|
| 决策记录本身 | **高质量工件** | 20260904-nav2-convergence-decision.md：A/B 证据表、四层世界模型归因、"本决策不宣称完成"（:3,62-65）、风险带关闭时戳（:79-82） |
| 生产链安全闸 | **真实落地** | nav2_localized:68,78,85-92 与 nav2_guarded:75,85,92-99：controller/behavior 双 remap /cmd_vel→/cmd_vel_raw + guard 节点 + respawn；cmd_vel_guard_node.cpp 引用 kDefaultMinValidEchoes 单一事实源（:41-43，P0-G 根治保持） |
| **"接线别绕过"不变量** | **2/4 违反** | nav2_scene.launch.py:77-87 无 remap、:97-105 无 guard（亲验全文 grep 仅 :6 docstring 与 :35 scene respawn）；nav2_demo 同构。nav2-stack.md §五把 nav2_scene 定义为**部署期生产流程**（建图巡航）、§七当 A/B Side A——CLAUDE.md:36-37 的绝对性规定在这两处不成立 |
| respawn 宣称 | **字面为假** | ADR:79-82"三份 nav2 launch 为 guard 与 scene_simulator 配 respawn"：guard respawn 仅 guarded/localized（:88/:95），scene respawn 仅 nav2_scene:35/ab_custom:21，互不重叠（亲验）——guarded/localized 里 scene_simulator 挂掉=里程计+雷达全失，恰是最该 respawn 的单点 |
| **A/B 可复现性** | **纸面** | ab_runner.py 全仓与全部 git 历史不存在（亲验 find + `--diff-filter=A`）；手册 nav2-stack.md:113,117 与 ab_custom docstring 三处引用之。"4/4 vs 0/4、96% 里程效率"无第三方可复现工件——决策记录 §6 验证门当前不成立 |
| 预建地图/AMCL | **真实落地** | maps/rack_3c.pgm+yaml 入库安装（CMakeLists.txt:179）；nav2_localized:39-63 map_server→AMCL→四件套按序拉起；pgm 头 382×201@0.05 与宣称 19.1×10.1m 相符 |
| NAV2 参数 | **版本化但无门禁** | nav2_params(.yaml/_localized) 入库；调优链可追溯（7cc57c2/582b3d1/0722839/b2b390c 均只改参数文件）；quality/ 38 个测试零 NAV2 引用，CI 无 NAV2 门——参数仅靠仿真手跑 |
| 文档一致性 | **表更新、图未更新** | ARCHITECTURE.md:55-146 两张主 mermaid 仍画旧管线为系统全貌、零 NAV2；:6-8 文档头"规划自研/2026-07-31"与 :281"SLAM/规划已入范围"直接互斥；nav2_params_localized.yaml:4 头注释仍是 SLAM 复制残留，与自身 :185-215 map_server/amcl 段矛盾；遗留 AMCL 资产（localization.launch.py + amcl.yaml）与新谱系平行共存未收敛标注 |
| 遗留管线定位 | **诚实** | 四份子系统文档头部加"A/B 基线"横幅（e029864）；四 nav2 launch 均不启 compute_container；但 system.launch.py docstring 仍自称 production process layout，无收敛注记 |

---

## 4. 72h soak 与穿货架核验（本 delta 主题二）

### 4.1 穿货架修复链

| 项 | 判定 | 证据 |
|---|---|---|
| 9351435 sticky LETHAL | **代码真实，零回归测试** | scan_to_grid.hpp:73 `set_free_if_not_lethal`（clearing 不降级 LETHAL）；test_scan_to_grid.cpp 四例全是修复前旧例，无一锁"LETHAL 不被后续射线清掉" |
| **5db0f02 sticky 衰减** | **❌ 方向写反（本审最重代码发现）** | :85 `if (cell < INSCRIBED) cell = FREE`：INSCRIBED=253（astar_planner.hpp:31 亲验），`253 < 253` 为假 → **INSCRIBED 恰好不可清**；而上一版 `< LETHAL(254)` 时 253 **可**清。diff 亲验：改动就是把可清改成不可清。commit 宣称修"动态障碍永久堵路"，实际把动态障碍的膨胀环固化成永久疤痕（A* `cost < INSCRIBED` 判不可走，:43）；:86 注释"INSCRIBED 可清"与实现直接矛盾。全 commit 仅 2 文件 11 行，无任何新测试 |
| f48c58e 静态屏障预注入 | **真实但污染仿真语义** | decision_node.cpp:100-115 预注入 3 排料架+2 机台，默认 false、simulation.launch:109-113 显式 true。预注入使 A* 规划期先验知道货架布局，**绕过被测的"感知→标记→避障"链路**——静态屏障恰把第一击"覆盖不足留缝隙"的缝隙在仿真里盖住；布局坐标硬编码 C++（真机语义 TODO） |
| 结构性结论 | **决策记录自认** | ADR §2.2："逐点修复每修一层另一层换姿态复发"——两击最终被判定结构性失败，自研栈退居 0/4 基线。**讽刺闭环：被判失败的那条修复链上，还有一个反向修复没被发现** |

### 4.2 72h soak（fee8f0f + 9eb4b30 + 1697163）

| 项 | 判定 | 证据 |
|---|---|---|
| 进程层韧性 | **代码侧可信** | supervisor waitpid 主路径真实（kill_child :154-173 + tick :245-265 + ECHILD 兜底）；"零僵尸/109 注入全吸收"与代码能力一致。扣：200ms 未回收路径记 WARN 后遗忘 pid，无兜底 sweep（:171-172）；kill 返回值不检查；**无回归测试**（22 个 domain 状态机测不覆盖 infra 层 waitpid） |
| **证据链** | **全部叙事化** | 报告数字仅指向容器路径 /soak_data/，`git log --all --diff-filter=A -- "*soak*"` 仅 6 个脚本——39,669 样本/109 注入/751m/Δ0.2MB 零入库。测量链缺陷（9eb4b30：收尾找不到 report 脚本/二值探针误读）披露于 commit 而未回写报告正文 |
| **goals.csv 空流** | **根因陈述为假，真因未追问** | 报告 §4 称"patrol 的目标不走该记录路径"——patrol_3c.py:29-50 每次到站 publish /goal_pose，soak_monitor.py:34 自首提交起订阅落盘（亲验）。空流唯一自洽解释：**初始 goal 落在 DDS 发现窗丢失 + 55h 零到站**。旁证：751m/55.1h≈13.6 m/h，与 §2"全程活跃导航"矛盾；同栈 A/B 实测"未离开起点口袋"（ADR:25-31）。证伪只需 10 分钟本地栈 |
| guard 配置 | **fail-safe 关闭未披露** | soak_run.sh:207：compute 以 `guard_min_valid_echoes:=0` 运行（亲验）——55h 验证的是全盲保护被关的栈，报告零披露 |
| 恢复画像 | **工件真、报告未对账** | soak_recovery_profile.py 可复现，中位恢复 4.77s/P90 5.52s。但 §1 表格仍写"控制可用率 100%（无 >3s 持续中断）✅"——**4.77s 中位中断直接推翻该行**，精确画像更新后表格单元没回改（主审独立发现）；79 vs 109 注入计数差无调和；画像脚本 :81-84 用 zip(sorted,sorted) 分位数错位配对，"拉起恒定 1.50s"可能是配对伪影 |
| 恢复链安全语义 | **无保护窗口** | guard respawn_delay=2s 期间 /cmd_vel 停发，scene_simulator 对指令无 staleness 归零（:37-39 存最后指令、:58-62 20Hz 无条件积分，亲验）——ADR §5.1"fail-safe 方向正确"依赖下游超时保护，仿真里不存在，真机取决于底盘固件。soak 恢复链（supervisor 拉 scene/compute/patrol）本身不含独立闸 |
| 环境叙事 | **漂移** | 报告标题"阿里云 ECS 4C8G"；deploy_soak_cloud.sh:3 写 e-c1m1.large（2C2G）、cloud 限内存 1400m |
| 脚本纪律 | **pipefail ✓ / 清场 ✗ / 顺序缺陷** | 三脚本 pipefail 齐；入口无清场（重复执行双实例双注入）；**cloud_soak_all.sh:45-52 `docker run bash /soak_entry.sh` 先于 :57 `exec cat > /soak_entry.sh`——入库脚本按字面首跑必失败**（亲验），即实际运行的与入库的不是同一份 |
| A2/B2 裁决 | **B2 诚实取消 ✓ / A2 关闭打折未披露** | B2"取消"理由可核验（管线退居基线→运行时装卸无生产收益），资产保留属实；A2 原验收"恢复次数=注入次数"在关闭时点数据上是 0/109（判据缺陷），"吞吐/时延曲线"从未产出，ITERATION.md:13-16 关闭条目只引数字不引折扣 |

---

## 5. 治理批核验（本 delta 主题三）

| 项 | 判定 | 证据 |
|---|---|---|
| B3 API 稳定性策略 | **政策真、执行彻底、强制力零** | 20260901-b3-api-stability.md 三级定义清晰；create_lidar(cfg,scenario) 全仓零残留调用者（e2e 3/3 验证）；但无 CI 检查弃用调用，CMakeLists.txt:22 `-Wno-error=deprecated-declarations` 无回收机制。附带：CHANGELOG [2.3.0] Deprecated 条目从出生丢主语（被弃用 API 名未出现），且该节是 tag 切割后补写 |
| C1 LTTng | **文档性接入** | package.xml tracetools exec_depend 真；但 CMakeLists.txt:17 find_package(tracetools) 结果零使用、AMR_TRACEPOINT 宏全仓零调用、fusion_node.cpp:314 悬空注释；无会话实跑证据。**ee6617 commit message 宣称"find_package 移除（__has_include 方案）"与工件矛盾**——`git log -S` 证明 CMakeLists 从未被该 commit 触碰（commit message 松第 4 例） |
| README（两次重写） | **结构升、宣称降** | ✓：双层形态谱系、覆盖徽章接 CI 实测（badge.json 81.7% 可复算）、fleet 措辞诚实、手写覆盖数字删除。✗ 新增 4 处：:150"55h+72h soak"（实为同轮 55.1h 连续+>68h 累计的进位包装）；:153"22 域测"（test_collision_guard.cpp=18 TEST_F，+3 e2e=21，取不到 22——违反自家"勿手写数字"新规）；:155"三轮外部审计整改全闭环"（发布时点 5/8 未动）；:168"Docker/RAUC 待落地"（toolkit/Dockerfile+compose 在库两月，反向过时）+ deployment-plan.md:180-182 同病 |
| journal | **断更** | change_journal.md 最后条目 2026-09-01；其后 v2.4.0 发布、NAV2 收敛、72h soak、B3 移除、README 两次重组零记录。三审称"journal 纪律是热的"——恰在最需要的一周变冷，纪律迁移到了 per-incident 文档而无人宣布 |
| 三审清单追踪 | **无载体** | ITERATION.md 审计节止于 08-28；三审核销状态只存在于 README 的宣称里（且宣称为假） |

---

## 6. 新问题清单（按严重度）

| # | 问题 | 严重度 | 证据 |
|---|---|---|---|
| N-1 | **5db0f02 安全修复方向写反**：INSCRIBED 膨胀环被固化成永久不可通行疤痕，注释与实现矛盾，零测试 | **P1** | scan_to_grid.hpp:85-86 + astar_planner.hpp:31,43 + diff 亲验 |
| N-2 | **安全闸不变量未闭环**：2/4 NAV2 launch 直出 /cmd_vel（含部署期建图生产流程与 A/B Side A） | **P1** | nav2_scene.launch.py:77-105、nav2_demo.launch.py:92-129（亲验） |
| N-3 | **soak 证据链叙事化 + goals.csv 空流真因未追问**（55h 零到站假设未被排除）+ guard=0 未披露 + §1 表格未与 4.77s 画像对账 | **P1** | 报告:3-8,19,25-28 + patrol_3c.py/soak_monitor.py 亲验 + soak_run.sh:207 亲验 |
| N-4 | **A/B 跑台不可复现**：ab_runner.py 不存在，决策依据数字无工件 | P2 | find + git 全史亲验；nav2-stack.md:113,117 |
| N-5 | **告警伪接线**：config/env 写 alerts.enabled/alertmanager 而全仓零消费者零实体 | P2 | production.yaml:14-17、staging.yaml:10-13（亲验 grep 空） |
| N-6 | README 4 处新失真 + 反向过时原句两审存活；package.xml 重复 3→8；journal 断更；ee66117 commit-工件矛盾 | P2 | README:63,150,153,155,168（亲验）；package.xml:15-40 |
| N-7 | ARCHITECTURE.md 主图/文档头未随 NAV2 收敛更新（头与正文互斥）；nav2_params_localized.yaml:4 头注释残留；mock_amcl/旧 localization.launch 平行共存 | P2 | ARCHITECTURE.md:6-8,55-146 vs :281 |
| N-8 | 三次安全修复（waitpid/sticky/静态屏障）+ 恢复判定器零回归测试；supervisor 错误路径僵尸残留；画像脚本分位数错位配对 | P2 | quality/ 零新测试；supervisor_node.cpp:171-172；soak_recovery_profile.py:81-84 |
| N-9 | NAV2 参数零 CI 门禁；"CollisionGuard 22 测试矩阵"数字不符（18）；ADR respawn 宣称字面为假；79 vs 109 计数未调和；环境叙事 4C8G vs 2C2G | P3 | ci.yml 无 nav2；ADR:45,79-82；deploy_soak_cloud.sh:3 |
| N-10 | cloud_soak_all.sh 顺序缺陷（run 先于写入入口脚本）+ 入口无清场——入库脚本≠实跑脚本 | P3 | cloud_soak_all.sh:45-57（亲验） |

---

## 7. 评分卡（对比三审）

| 维度 | 三审 | 本审 | 依据 |
|---|---|---|---|
| 架构与分层设计 | 8 | **8.5** | NAV2 收敛决策 + L3 分层真实落地 + 生产/开发形态谱系；扣：ARCHITECTURE 主图未随更 |
| Domain 算法与单测 | 8.5 | **8** | +6 场景域测；但 5db0f02 方向反转 + sticky 语义零测试（N-1） |
| Infrastructure 并发/内存 | 7 | **7** | waitpid 主路径真实；错误路径残留、无回归锁、TSAN 四审零实跑 |
| 安全工程 | 6.5 | **6** | 生产链闸真实 + 单一事实源保持；扣：2/4 绕闸、respawn 2s 无保护窗口、guard=0 soak、R4.1/R4.7 未动 |
| 可观测性 | 6 | **5.5** | 恢复画像脚本真；扣：告警伪接线（倒退）、LTTng 空接、soak 数据不入库、journal 断更 |
| 测试工程体系 | 6.5 | **6** | B3 e2e 3/3、真复现种子；扣：A/B runner 缺失、三次安全修复零回归锁、NAV2 无门禁 |
| 文档-代码一致性 | 5.5 | **5.5** | 极化：决策记录/终期报告/nav2-stack.md 是四审最佳工件；README 4 处新失真 + ARCHITECTURE 头/图 stale |
| 发布与治理 | 5 | **4.5** | v2.4.0 干净三方对齐；扣：tag 守卫仍零 + v2.3.0 死 tag 留存、package.xml 恶化、"全闭环"假宣称、journal 断更 |
| 部署运维就绪 | 3.5 | **4** | ECS 55h 实跑经验 + systemd unit 入库（ITERATION 背书）；扣：unit/env 零消费、cloud 脚本顺序缺陷、伪接线 |

**综合：约 6.5 → 约 6.5 / 10——但方差显著拉大：能力上限抬高（导航能力风险出清），
证据下限压低（数据叙事化 + 宣称层倒退）。** 若下一波把 N-1/N-2/N-3 关掉并工件
入库，综合分应在 7 以上；若只修宣称不修证据链，6.5 也守不住。

---

## 8. 落地距离（更新）

**单机商用试点：9-15 → 8-13 人月。** 变化项：

- **导航能力风险出清**（-2 人月量级）：自研栈 0/4 的世界模型缺陷曾被隐含计入
  距离；NAV2 4/4 + 生产形态落地把这个块从"要造"变成"要调"（参数上机重调仍在）
- **首个长时数据点**（进程层）：零僵尸/RSS 平稳的 55h——任务层成功仍未证明
  （N-3 未排除零到站假设）
- **新增垫脚石**：证据化工件入库（soak 数据、A/B runner）、安全闸不变量闭环、
  观测真接线——每项都是周级而非月级，但当前为零

四大约束块（真机 bring-up / 安全软件过程 / 认证叙事 / 供应链）依旧零推进，
仍占距离主体。阶段判定：**工程样机门槛边缘 → 仿真闭环完成、进入"证据化与
真机预备"阶段**——下一步的关键词不再是"能跑"，而是"可信地证明跑过"。

---

## 9. 修复优先序（下波）

1. **5db0f02 立即修正 + sticky 语义回归锁**——3 行单测一锤定音（inflate 标一点，
   射线穿过其 INSCRIBED 环，断言恢复 FREE；当前实现必红）。这是唯一"代码在
   反向跑"的活缺陷，且坐在被判失败但仍在跑的 A/B 基线里
2. **nav2_scene / nav2_demo 补 guard 接线，或显式豁免裁决**——"接线别绕过"
   要么闭环要么改口径，不能停在 CLAUDE.md 的绝对句和 2/4 的现实之间
3. **soak 证据化三件套**：数据工件入库（CSV 子集即可）+ goals.csv 空流真因
   终验（10 分钟本地栈可证伪零到站假设）+ 报告对账（§1 表格 × 4.77s 画像、
   79 vs 109、guard=0 披露、环境规格）
4. **ab_runner.py 入库**——A/B 是 NAV2 收敛的决策依据，验证门对第三方生效
5. 三审机制活兑现（第三轮提醒）：CI tag 守卫一行 + TSAN 一次实跑入 journal +
   告警真接线（或删伪接线三行）
6. README 四处宣称对账 + package.xml 去重（3→8 恶化）+ ARCHITECTURE 主图/
   文档头/nav2_params_localized 头注释
7. 三次安全修复补回归锁（waitpid/sticky/静态屏障）+ cloud_soak_all.sh 顺序修复
8. journal 复刊，或显式宣布"变更记录迁移至 per-design 文档"——无声断更本身
   就是三审结语的反例

---

## 10. 结语

四审连起来看，这个仓库分裂成了两张脸：**事后文档的脸是 L7 的**——决策记录、
终期报告、B3 政策，自曝缺陷、给折扣、留证伪路径，本轮最硬的工件全在这里；
**常驻面的脸是失控的**——README、tag 边界、launch 不变量、config 模板、
CI 机制，四审点名的同一批位置反复失守。三审说"把纪律从 journal 搬进 CI"，
六天后的事实是：journal 自己断了更，CI 一行未动，纪律搬进了越来越漂亮的
事后文档里。

最需要警惕的是 soak 这一笔：55 小时、109 次注入、751 米——如果零到站假设
为真，这份报告证明的是"进程死不了"，而不是"机器人能干活"。报告没有说谎，
它披露得很慷慨；但它把唯一的任务级证据流定性为"判定器 bug"后丢弃，没有
人追问"流为什么是空的"。**对一个以 fail-safe 为设计哲学的项目，最深的
fail-safe 应该装在结论层：当一条证据流为空时，先假设的是任务失败，而不是
测量故障。** 10 分钟的本地栈就能裁决——这是下一波的第一件事。

---

**取证索引（亲验项加 ※）**：※tag v2.3.0=5decc24 不可达 / v2.4.0 可达三方对齐 ·
※scan_to_grid.hpp:79-87 + 5db0f02 diff + astar_planner.hpp:30-43（INSCRIBED=253）·
※soak_run.sh:207（guard_min_valid_echoes:=0）· ※patrol_3c.py:29-50 / soak_monitor.py:29-42 ·
※nav2_scene.launch.py 全文（无 remap/guard）· ※respawn 拓扑（localized:88 / guarded:95 /
scene:35 / ab_custom:21）· ※ab_runner 全史不存在 · ※README:63,150,153,155,168 ·
※package.xml ×2 重复 3 条亲验 · ※scene_simulator_node.cpp:37-62（无指令超时）·
※cloud_soak_all.sh:43-60（run 先于写入）· ※src/ 手搓 QoS 计数=0 ·
supervisor_node.cpp:154-173,245-265（waitpid）/ :171-172（残留）·
nav2_localized:39-92 · cmd_vel_guard_node.cpp:41-59 · ARCHITECTURE.md:6-8,55-146,281 ·
config/env/production.yaml:14-17 · soak_recovery_profile.py:81-84 · CHANGELOG [2.3.0]/[2.4.0]
**未核项**：容器内 soak 原始数据（不在库，N-3 的零到站假设需其或本地复跑裁决）；
CI 运行日志（徽章链佐证全绿为宣称）
**文档版本**：1.0（2026-09-07）
