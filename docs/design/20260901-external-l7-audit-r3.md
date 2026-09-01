# 外部 L7 第三审报告 — ros2_amr_framework（R1-R5 整改收官核验）

> 审计基准：HEAD = **140e27a**（2026-09-01 09:3x 冻结；仓库在审计期间仍活跃提交，
> 冻结后新提交不在本期范围）
> 对比基线：6d5697f（第二审基准），13 commits，34 文件 +1645/-152
> 取证方式：**全本地**（网络配额受限，gh/CI 运行日志未核——"38 模块全绿 / cppcheck 0 错"
> 为 journal 宣称 + 三次 ci-bot 徽章提交链佐证，标记待网络恢复抽验）；覆盖率用 HEAD 工件复算
> 口径：与前两审一致——只认代码事实与工件；commit message / journal 叙事只作宣称接受核验
> 关系链：0830 首审 → 0901 复审 → 0901 整改方案 → 方案审计 → 决策记录 ADR → ADR 审计 → 本审

---

## 1. 总判词

**"修复-审计"闭环已被压缩到小时级，且这一轮的整改几乎逐条兑现了 ADR——但同型故障
在防线外复发：v2.3.0 的 tag 又打在了死 commit 上，距 v2.2.0 同型故障修复不到 24 小时。"**

这一轮最实质的变化是**度量诚实链路首次全程为真**：真分母生效（我按 HEAD 工件复算
3055/3702 = 82.52%），81.6% 的诚实低数字触发了 76.9% < 80% 的门禁冲突，冲突被摆上
桌面裁决（显式豁免 + 三行理由），而不是绕过。P0-G 用单一事实源常量根治、P0-B 用
快照锁根治且带双模式回归测试、恒真断言修成了真行为断言——整改质量整体是 ADR 级的。

复发的是**流程型故障**：tag 打完 22 秒后 amend（9a45e51 09:01:04 → fc7ea37 09:01:26），
tag 留在孤儿上；release commit 宣称"R1-R5 完整收官"而 R4.1/R4.7 实际未做；README 在
代码前进时零 diff。规律与前两审一致：**journal 里的纪律是热的，边界处的那几秒钟——
打 tag 的手、写 commit message 的手——没有防线。**

---

## 2. R1-R5 逐项核验

| 项 | 判定 | 证据与备注 |
|---|---|---|
| R1 度量诚实（P0-I） | **根治** | quality.sh：`--ignore-errors` 与 runtime 对齐 + 去 `\|\| true` + 失败即红 + 非空断言 + merge 失败红——ADR 全条落地。真分母 59→72 文件，86.7%→**82.5%**（复算 82.52% ✓）。R1.2 冲突裁决走路径②：supervisor_node 显式豁免 + 理由（domain 22 测覆盖逻辑；infra 需真实进程环境，mock = 测 mock）。附带诚实记录：豁免注释曾打断 bash 续行致 remove 段不执行——自己端到端跑出来修的，教训入 journal。journal 更正记录（"铁证：grep = 1 条 0/229"）✓；CLAUDE.md 元防线两行 ✓ |
| R2 P0-G 默认反转 | **根治** | `kDefaultMinValidEchoes = 50` 单一事实源（collision_guard.hpp），domain Params 与 motor_ctrl_node declare **同引一常量**——node 侧无法独立回退 fail-open（F-2 修法完整落地）。三处 "(sim)" 注释口径统一 ✓。小偏差：ADR 定的域测断言未写成测试，以 journal grep 代替——机制已锁，测试缺位 |
| R3 P0-B 竞态 | **代码根治，防线半步** | grid_mutex_ + 锁内 160KB 快照 + A* 在快照上执行（写侧 raytrace/inflate 全入锁，诊断读入锁）✓。test_grid_race 双模式压力测试（独立 ranges 缓冲 + 被堵 goal + 持续 2s）注册 CMake ✓。**TSAN 仍未进 CI**（ci.yml / run_tests.sh 零命中），journal 亦无 TSAN 实际运行证据——"修复前必红"停留在设计性质宣称 |
| R4 收尾杂项 | **部分完成** | 重启超时 30s + 放弃 ✓（S-2 诚实降级；注：注释称"置 ERROR 上 /health/report"，实际 ERROR 经由既有周期报告路径成立——语义真、归因松）；计数器原子写持久化 ✓（temp+rename，~/.ros/amr_ota）；P0-K 真话 ✓（ARCHITECTURE.md 注 + 表行标 roadmap + **CLAUDE.md:30 同步**——F-3 完整落地）；v2.2.0 钉 6d5697f ✓ 可达。**R4.1（system_secure install 路径）未动**：CMake 仍双 EXCLUDE、launch 与 verify 脚本零 diff；**R4.7（keystore 0600 自动化）未动**。CHANGELOG R4 行未列此二项（未假称），但 release commit 宣称"完整收官"过 Claim |
| R5 部署链 | **大体完成** | compose 单 compute 服务 + 交付物事实注释 ✓；entrypoint 直通名特例 ✓（compute_container 等 5 名）；package.xml 补 8 依赖 ✓ 但 **3 条重复**（ros_gz_sim / ros_gz_bridge / robot_state_publisher 各两次）；rosdep 重建验证无证据（离线不可核） |

## 3. 计划外新增（本批自发）

| 项 | 判定 | 备注 |
|---|---|---|
| Prometheus 告警规则（c99969e） | **内容真，接线零** | config/prometheus/amr_alerts.yml：四组规则（传感器失明=08-17 模式 / 节点死亡 / supervisor FATAL / 控制环延迟），阈值来自事故史，带 annotations——质量高。但 toolkit/compose 无任何 prometheus 引用，文件头自述"部署：rule_files 引入"=手动。"会叫"目前只在文件里 |
| 恒真断言修复（c99969e） | **根治** | 两处 EXPECT_GE(size(),0u) 改真行为断言：lowstep 场景 + 等待非空 + 失败消息；且揭示了"默认场景无障碍物"这一被恒真断言掩盖的事实并写明 |
| trace 死代码（c99969e） | **诚实化** | AMR_TRACING 编译期开关（默认 OFF）+ LTTng 替换计划注——从"100% 死代码"变为"显式 fallback" |
| 云端 72h soak（aea3360/140e27a） | **进行中** | supervised_scene.launch + SCENE_MODE + 一键部署 + systemd 自愈脚本；无报告（在跑，ECS 状态佐证）。**这是本项目第一个长时可靠性数据点的孕床** |
| CI ASAN 并行环境感知（799c81d） | ✓ | CI 满并行 / 本地 WSL OOM 纪律 |

## 4. 新问题清单

| # | 问题 | 证据 |
|---|---|---|
| N-1 | **v2.3.0 tag 打在死 commit 上（同型二连）** | tag → 9a45e51（09:01:04）→ 22 秒后 amend 出 fc7ea37（09:01:26）；`branch --contains 9a45e51` 空、非 HEAD 祖先；`git describe HEAD` = v2.2.0-13-g140e27a。距 v2.2.0 同型修复 <24h。根因：tag 后 amend——流程时刻无防线 |
| N-2 | README 反向过时（低报） | README:52 仍称"**告警规则未实现**（只看不叫）"，amr_alerts.yml 已存在；README 全程零 diff。宣称同步依然靠手，这次是往低报方向滑 |
| N-3 | package.xml 三条重复依赖 | ros_gz_sim / ros_gz_bridge / robot_state_publisher 各出现两次 |
| N-4 | release 叙事过 Claim | fc7ea37"R1-R5 完整收官"vs R4.1/R4.7 未做。CHANGELOG 逐行真实（未列未做项）——宣称纪律在 journal/CHANGELOG 严、在 commit message 松，**同模式第三次出现** |
| N-5 | TSAN 无 CI 亦无运行证据 | R3.3 可选项未做，journal 无 TSAN 实跑记录——竞态回归锁的"必红"性质未经一次实证 |
| N-6 | QoS 词汇表违反仍在 | health_monitor_node.cpp:133/:232 手搓 `rclcpp::QoS(10).reliable()`（S-B 指出后，P1 清扫同文件摸过未修） |
| N-7 | 告警规则零部署接线 | 见 §3——rules 文件与 toolkit/部署链无关联 |

## 5. 评分卡（对比 0901 复审）

| 维度 | 复审 | 本审 | 依据 |
|---|---|---|---|
| 架构与分层设计 | 8 | 8 | 无变化 |
| Domain 算法与单测 | 8.5 | 8.5 | 无变化 |
| Infrastructure 并发/内存 | 5.5 | **7** | P0-B 根治 + 回归锁；扣：TSAN 防线缺（N-5） |
| 安全工程 | 5.5 | **6.5** | P0-G 单一事实源根治、重启超时、计数器持久化；扣：install 侧 secure launch 仍断（R4.1）、0600 未机制化（R4.7）、DDS 默认关（已裁决） |
| 可观测性 | 5 | **6** | 告警规则存在且事故史锚定；扣：零接线（N-7）、Alertmanager 路由缺、日志轮转未动 |
| 测试工程体系 | 6.0 | **6.5** | 真分母 + 有据豁免 + 竞态回归锁 + 恒真断言真修；扣：TSAN CI、fusion CRITICAL 节点级覆盖仍缺、R2.1 断言以 grep 代测试 |
| 文档-代码一致性 | 5.0 | **5.5** | P0-K/CLAUDE.md/journal 更正全落地；扣：README 反向过时（N-2）、"完整收官"过 Claim（N-4） |
| 发布与治理 | 4.5 | **5** | v2.2.0 修复 + 版本对齐 + 豁免理由 + 元防线入 CLAUDE.md；扣：v2.3.0 tag 复发（N-1）、依赖重复（N-3） |
| 部署运维就绪 | 3.0 | **3.5** | compose/entrypoint/依赖补齐、云端 soak 在跑（首个长时数据点）；扣：rosdep 未证、R4.1 断、告警未接线 |

**综合：约 6.0 → 约 6.5 / 10。** 全部涨分仍来自"关掉已知缺口"，但部署链与长时证据
首次有实物在动。

## 6. 落地距离（更新）

四大约束块（真机 bring-up / 安全软件过程 / 认证叙事 / 供应链）依旧零推进——这不是
本迭代该解决的，但它是距离的主体。变化在边缘：72h soak 若出报告，将是第一个
长时可靠性数据点；告警规则接线后观测闭环才成立。

**单机商用试点：9-16 → 9-15 人月**（边际）。阶段判定：仿真闭环原型末期 →
**工程样机门槛边缘**（缺：真机、长时数据、安全过程三块垫脚石中的前两块正在动）。

## 7. 修复优先序（下波）

1. **v2.3.0 tag 重打 + CI tag 守卫机制化**——同型二连已经证明习惯无效，需要一行 CI：
   每个 `^v` tag 必须是 HEAD 祖先且版本与 package.xml 一致，否则红
2. **README 再对账**——告警已实现（:52 反向过时），顺带 fleet 措辞（:65）
3. **R4.1 / R4.7 显式裁决**——做或入 roadmap，并更正 release 叙事（"完整收官"→实际范围）
4. package.xml 去重 + `rosdep install` 重建证据入库
5. TSAN：哪怕 nightly / workflow_dispatch，**跑一次**并把红绿证据入 journal
6. 告警规则接线（toolkit prometheus mount 或部署文档）+ Alertmanager 路由最小例
7. health_monitor 两处 QoS 手搓改 `amr::qos::` 词汇表
8. soak 报告落地后评审（含 systemd 自愈的实际触发记录）

## 8. 结语

三审连下来的曲线是清晰的：代码缺陷清单在收敛（P0 十二项：根治 9、大体/半修复 2、
显式裁决 1），宣称质量在分化（journal 与 CHANGELOG 越来越硬，commit message 与
README 这两个"顺手写"的边界仍在漏），流程故障在防线外复发（tag 二连）。

这个仓库已经证明它能在两小时内走完"审计指控 → 独立核验 → ADR → 修复 → 验证门"
的全程。剩下的问题只有一个：**把这套纪律从 journal 搬进 CI**——tag 守卫、TSAN、
告警接线、README 对账，全是机制活。当最快的那个手（打 tag 的手）也会被红线拦住时，
这个项目才算真正拥有了它已经在纸面上拥有的东西。

---

**取证索引**：quality.sh（initial/断言/豁免注释）· coverage_full.txt@HEAD（72 文件复算
82.52%）· collision_guard.hpp（kDefaultMinValidEchoes）· motor_ctrl_node.cpp:38-41 ·
decision_node.cpp（grid_mutex_/快照）· test_grid_race.cpp · CMakeLists.txt:172-173,286-288 ·
health_monitor_node.cpp:164-186,271-283（超时/报告路径）· ota_agent_node.cpp（原子写）·
CLAUDE.md（:30 修正 + 元防线节）· ARCHITECTURE.md:150-162 · package.xml（8 增 3 重）·
docker-compose/entrypoint · config/prometheus/amr_alerts.yml · tag 对象 9a45e51/fc7ea37
时间戳（09:01:04 / 09:01:26）
**未核项（网络受限）**：CI run 日志（38 模块全绿 / cppcheck 0 错为宣称 + 徽章链佐证）、
rosdep 重建、TSAN 运行（本就无记录）
**文档版本**：1.0（2026-09-01）
