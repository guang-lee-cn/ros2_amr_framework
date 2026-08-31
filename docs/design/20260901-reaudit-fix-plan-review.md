# 整改方案审计 — 20260901-reaudit-fix-plan.md

> 审计对象：`docs/design/20260901-reaudit-fix-plan.md`（Wave R1-R5，2026-09-01）
> 方法：只审事实不审偏好——方案中每一处技术断言对照代码 / CI 日志 / 本地实验复核。
> 本轮新增取证：CI job 日志（run 33406894635 / build-test-coverage fastrtps，job 99536588269）、
> lcov 2.0 最小 gcno 实验（--initial 与双 -a merge 各一次）、system.launch / entrypoint / compose 逐行核对、
> OccupancyGrid 内存布局核验（400×400×uint8_t = 160KB）
> 关系：上游为 `20260901-external-l7-reaudit.md`（复审报告）；本文档是其整改方案的执行前审计

---

## 1. 总判定

**框架通过，三处技术断言错误，其中一处若照写将制造新的失实宣称。**

方案做对了最难的部分：波次划分（R1 度量诚实先行——它定义"修复"二字的信誉）、每项挂验证门、
"不排期"显式裁决（P0-H / DDS security——P0-K 静默漏排的教训已吸收）、§4 验证纪律
（验证不引用自身产物 / 命令级 / CI 级 / 反例级）正是 P0-I 应当长出来的元防线。

但 §2 三处在执行前必须修正：R4.3 的拟写入句与代码事实相反（P0-I 同型复发风险）、R3.2 的
锁窗口论证数学错误、R3.1 的验证门是一盏无法变红的灯。修完 §2 三项，方案即可执行。

---

## 2. 必须修正（执行前）

### 2.1 R4.3：拟写入 ARCHITECTURE.md 的句子不成立

**方案原文**：「独立传感器进程 + DDS 话题消费 = system.launch 显式配置（真机形态）」

**代码事实**：system.launch.py:58-64 的 compute_container 节点**零参数块**，注释自认
"Sensor types declared via ROS2 params (default: simulated)"。即 system.launch 形态下
fusion 同样进程内实例化仿真传感器；它启动的 lidar/imu/camera 独立进程的话题**同样无消费者**。
这正是复审 P0-K 的原始内容，一个字都没有变。

**后果**：照写即把 P0-K 从「数据流表不成立」修成「数据流表换了一种不成立」——在宣称对账
的波次里制造新假宣称，与 R1 整治的元问题同型。

**两个可接受改法**（二选一，需显式裁决）：

1. **文档改真话**：表注改为「所有 launch 形态下 fusion 默认进程内实例化仿真传感器；独立
   传感器进程话题当前无消费者；真机话题接线未实现（roadmap）」——一句话，零代码；
2. **真接线**：给 fusion 增加 topic-source 传感器类型并让 system.launch 传参启用——
   注意这是**功能开发**（fusion 现无此模式），不是"显式配置"，工作量按新功能计。

验证门「文档检查」同步作废：按 §4 纪律，改法 1 的验证门是 `grep simulated launch/system.launch.py`
确认无参数覆盖 + 表述与之一致；改法 2 的验证门是 e2e 断言 fusion 输出随话题输入变化。

### 2.2 R3.2：锁竞争窗口的数学错误

**方案原文**：「锁竞争窗口微秒级，粗粒度锁足够（不搞双缓冲）」

**事实**：demo_grid_ 为 400×400 = 160,000 格（decision_node.cpp:91-95），`uint8_t` 存储
= 160KB（astar_planner.hpp:37）。复审与首轮审计的共同数据：goal 被堵时 A* 逼近 **200ms**。
粗粒度锁包住 `astar_.plan(demo_grid_, ...)`（decision_node.cpp:243）则 perception 回调
（raytrace :184 / inflate :211）最多阻塞 200ms——**整整一个 5Hz 周期**。窗口是百毫秒级，
不是微秒级。

**推理漏洞**：锁只护写入、不护 plan 读则数据竞争仍在；护住 plan 读则窗口 = A* 时长。
没有第三种锁法。而"微秒级"的唯一实现方式恰是方案否掉的方案——**锁内快照拷贝**
（160KB memcpy 约 10-50µs），拷贝上做 A*，perception 周期不受影响。快照成本可忽略，
这是标准的读侧复制模式，不是过度工程。

**修正**：R3.1 修法改为「`grid_mutex_` + 锁内 `grid_snapshot = demo_grid_` 拷贝，plan 在
快照上执行」；R3.2 改为论证快照的 160KB 拷贝成本（附实测数字），删除"微秒级粗锁"表述。

### 2.3 R3.1：验证门恒绿（无法变红的灯）

**方案原文**：「TSAN 跑 decision 用例零自家帧报告」

**事实**：现有 decision 单测全部单线程。TSAN 只在**真实并发交叠**发生时才可能报告——
单线程用例下修与不修都是零报告。这盏灯没有红的状态，违反方案自家 §4 第三条
（"能描述如果不修会怎样红的测试"）——与 P0-I 的"已验证"帽子同构：一个不能失败的验证
不构成验证。

**修正**：R3.1 验证门改为两段——

1. **并发压力测试**（先写，应当红或至少让 TSAN 报）：测试内起两线程，一线程循环
   `raytrace+inflate`，另一线程循环 `astar_.plan`，持续 ≥2s；修复前 TSAN 必报
   demo_grid_ 数据竞争（此测试本身就是 P0-B 的回归锁）；
2. 修复后同测试 TSAN 零报告 + 单测全绿。

---

## 3. 重要修正建议

### 3.1 R1.1 根因假设已被证据反驳（真根因已锁定到高置信）

**方案假设**：「可能：coverage 构建先 clean 了 gcno → initial 时无文件」

**反驳**：runtime geninfo 生成行级数据必须读取 .gcno（gcov 的 notes 文件）；CI runtime
捕获成功产出 59 文件行级数据 ⇒ 质量检查执行时 gcno **在场**。假设不成立。

**排除实验**（本地，lcov 2.0-1）：

| 实验 | 结果 |
|---|---|
| 最小 gcno 跑 quality.sh 原句 `lcov --initial --capture ... --rc geninfo_gcov_all_blocks=0` | exit=0，产物含完整零覆盖记录（DA:line,0） |
| `lcov -a f1 -a f2 --output-file out` 双文件 merge | exit=0，正常聚合 |

工具本身与语法均正常。

**高置信真根因**：initial 命令**未带 `--ignore-errors`**。同一 CI run 的日志含大量
mismatch 诊断（runtime 捕获靠 `--ignore-errors empty,unused,mismatch,gcov` 才存活——
这些 flag 是作者此前踩到同类致命错后加的，却只加给了 runtime 一侧）。lcov 2.0 下 mismatch
未豁免即致命错误 → initial 整体失败 → `>/dev/null 2>&1 || true` 吞掉 → 空文件 →
`-s` 跳过 merge。旁证：remove 步骤报警 `'*/compute_container*' is unused`——若 initial
有贡献，零覆盖的 compute_container 记录必在 remove 前的数据中。

**修正**：R1.1 修法直接写明——initial 捕获补 `--ignore-errors mismatch,gcov`（与 runtime
对齐）；配合 R1.3 去沉默后，若仍失败分钟级可见。保留 R1.1 原验证门（supervisor_node.cpp
以 0% 出现）不变，该门正确。

### 3.2 双 CA 分离被静默漏项

复审 §8 门槛 7 为「私钥机制化 + **双 CA 分离** + system_secure launch 修复」；R4.7 只含
0600 自动化 + R4.1 只含 launch 路径。identity CA 与 permissions CA 同钥软链（首轮 P0-F
原始发现）在方案中无条目、无裁决——一份主题为消灭静默遗漏的方案出现静默漏项。补一行：
做（keystore 生成脚本分两钥）或显式裁决不做（写明风险接受理由）。

### 3.3 R5.1 按字面执行会坏

两处独立问题：

1. entrypoint 拼接规则是 `NODE_EXEC="${NODE}_node"`（toolkit/scripts/entrypoint.sh:5）——
   `command: [compute_container]` 会解析成 `compute_container_node`，**不存在**。需加
   直通名特例或改拼接规则；
2. 「fusion_node/decision_node/motor_ctrl_node → compute_container」若按字面理解为三个
   服务各自改名，则三个容器各跑一份完整 compute_container = 三份全管线话题冲突。正确
   形态：**一个** compute 服务跑 compute_container，删除 fusion/decision/motor_ctrl 三行；
3. 顺带在裁决中写明：decision/motor_ctrl 无独立可执行（仅存在于容器内），这是交付物
   事实，不是配置问题。

### 3.4 R4.4 tag 目标语义

`git tag -fa v2.2.0 HEAD` 在 R4 执行时，HEAD 已包含 R1-R3 修复——tag 将覆盖 CHANGELOG
2.2.0 节未记载的内容。更干净的做法二选一：

1. **v2.2.0 钉在 6d5697f**（历史事实：2.2.0 就是带着瑕疵发布的，误宣称更正进 journal 与
   CHANGELOG 更正注记）；R1-R5 整改完成后发 **v2.2.1**；
2. 或全部修复并入 v2.3.0，v2.2.0 仍钉 6d5697f（同上）。

任一走法都要在 journal 留痕"force-move 已发布 tag"这一治理动作本身。验证门
（`git merge-base --is-ancestor`）保持。

---

## 4. 小项

| # | 项 | 建议 |
|---|---|---|
| S-1 | N-4（作者手 commit 覆盖 quality/data 与 ci-bot 两套口径）方案无对应条目 | R1.5 顺带立规矩：coverage 数据只允许 ci-bot 提交；作者侧改动 quality.sh 后首次 CI 覆盖即归一 |
| S-2 | R4.2 超时后仅「放弃重启 + WARN」 | 升级动作：置 HealthStatus DEGRADED（或交 supervisor 进程级策略），否则恢复链路仍是断的、只是不挂了 |
| S-3 | R4.6 计数器持久化 | 文件原子写（temp+rename，OTA 同型教训）；落可写状态目录（如 `~/.ros/amr/`），不在 install share |
| S-4 | R4.1 验证门「verify_dds_security.sh 绿」 | 必须从 **install 空间**跑 launch——source 空间绿不能证明 install 断裂已修复；核对脚本实际路径 |
| S-5 | R2.1 测试形态 | 「无覆盖 → 失明 → 硬停」用 CollisionGuard 域测断言默认参数即可，勿绕节点起 DDS——最小路径 |
| S-6 | R1.2 路径 ②（分母豁免） | 若走此路，豁免行必须带理由注释（R1.4 的原则推广到 supervisor） |

---

## 5. 修订汇总（对方案的逐条改法）

| 方案条目 | 处置 | 改法 |
|---|---|---|
| 一、核验表 | ✅ 维持 | 五行经独立复核全部属实（本文档 §1 方法栏） |
| R1.1 | 改 | 根因假设替换为 §3.1（补 --ignore-errors 对齐 runtime）；验证门不变 |
| R1.2-R1.5 | ✅ 维持 | R1.2 若走豁免路径补 S-6 |
| R2.1-R2.3 | ✅ 维持 | R2.1 测试形态按 S-5 收窄 |
| R3.1 | 改 | 修法加锁内快照（§2.2）；验证门换并发压力测试两段式（§2.3） |
| R3.2 | 改 | 论证对象改为快照拷贝成本（附实测），删"微秒级粗锁" |
| R3.3 | ✅ 维持 | — |
| R4.1 | 补 | 验证门限定 install 空间（S-4） |
| R4.2 | 补 | 超时升级动作（S-2） |
| R4.3 | **改** | 二选一显式裁决（真话文档 / 真接线按功能计），原句子作废（§2.1） |
| R4.4 | 改 | tag 目标与版本策略（§3.4） |
| R4.5 | ✅ 维持 | — |
| R4.6 | 补 | 原子写 + 状态目录（S-3） |
| R4.7 | 补 | 增加双 CA 分离条目或显式裁决（§3.2） |
| R5.1 | 改 | 单 compute 服务 + entrypoint 直通名；裁决写明无独立可执行事实（§3.3） |
| R5.2-R5.3 | ✅ 维持 | — |
| 三、不排期 | ✅ 维持 | 两项裁决显式、有理由 |
| §4 验证纪律 | ✅ 维持 | R3.1/R4.3 原稿违反此节，修正后自洽 |
| 波次顺序 | ✅ 维持 | R1 先行的理由成立 |

---

## 6. 结语

方案的价值在结构：验证门、显式裁决、度量先行——这些把"整改"从叙事变成了可检验的过程。
本次审计动它三刀（R4.3 / R3.2+R3.1 / R1.1 根因），没有一刀是口味问题，全部是
断言与事实的冲突。特别指出：R4.3 若不改，它将成为这份方案自己定义要根治的那类缺陷——
**验证自己宣称的纪律，必须从方案文本本身开始生效。**

---

**审计证据索引**：system.launch.py:58-64 · decision_node.cpp:91-95,184,211,243,376 ·
astar_planner.hpp:29,37 · entrypoint.sh:5 · docker-compose.yml:23-102 ·
CI job 99536588269 日志（lcov 2.0-4ubuntu2，mismatch 诊断段 / remove unused 段 /
86.7% Gate SKIP 段）· 本地 lcov 2.0-1 实验（--initial、双 -a merge 各 exit=0）
**文档版本**：1.0（2026-09-01）
