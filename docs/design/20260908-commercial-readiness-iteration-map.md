# 商用可落地差距分析 — ros2_amr_framework（L7+ 视角 · 迭代点地图）

> 基准：HEAD = **aa1eb82**（2026-09-08；五轮审计后首次零已知 P1/P2）
> 性质：前瞻差距分析，非审计——问题不再是"宣称与工件是否一致"，而是
> "这个系统离商用运营还缺什么"。分四个剖面：故障点 / 性能 / 可维护性 / 可观测性。
> 证据口径：全部命令级核验（grep/read 于本仓库），无工件支撑的能力按"缺"计。
> 关系链：五轮审计（0830/0901×2/0907/0908）已把"代码正确性+宣称诚实"修到
> 自愈水平——**本文件回答的是下一个问题：正确之后，缺什么。**

---

## 0. 定位判断

这个仓库现在的真实状态：**导航能力已收敛（NAV2 4/4 vs 自研 0/4）、进程韧性有
55h 数据点、宣称与工件的自愈纪律已成机制（tag-guard/TSAN nightly/裁决文档）**。
五轮审计追的那类问题（代码反向、绕闸、叙事化）已收敛到"引入即被抓"的水平。

商用地落是**另一个轴**：从"证明能跑"到"证明能运营"。四个剖面里当前最薄的
不是任何一行代码，而是**软件与物理世界的边界**——电池、地图生命周期、底盘
看门狗语义、真机参数、认证路径。仿真验证的逻辑在这个边界上失效（下述 F 类
故障大半是仿真原理上无法复现的）。

---

## 1. 商用落地四大缺口块（更新版）

| 块 | 状态 | 缺什么（迭代点） |
|---|---|---|
| **B1 真机 bring-up** | 零推进（A1 挂账） | sick_tim781 真实点云适配（adapter 头已备 [sick_tim781_adapter.hpp](include/ros2_robot_middleware/hal/sensor/sick_tim781_adapter.hpp)）；真底盘 /cmd_vel 消费方与协议；真机 URDF/TF 树；AMCL/costmap 参数上机重调（ADR 已预告）；StampGate 容差按真实时标定 |
| **B2 安全工程** | 软件闸完整，硬件回路零 | P0-H 维持原判：软件 CollisionGuard 不能替代双通道安全回路（ISO 3691-4 语义）。迭代点不是写代码，是**安全概念文档**（SAFETY.md：软件闸的保护范围/边界/已知洞——倒车放行、2s respawn 窗口、参数漂移）+ 底盘安全 PLC/e-stop 对接契约。商用保单与责任划分取决于这份文档 |
| **B3 运营闭环** | **最大空白** | ① 电池系统全仓零实现（grep battery/charg/dock = 0）——电量模型、低电降级、回充对接、任务前电量闸；② 地图生命周期：rack_3c 是快照，无版本管理/重测绘 SOP/变更流程；③ 车队协调：fleet_manager 仅聚合，无任务分配、无交通管制（同巷道双机死锁无人解） |
| **B4 供应链/发布** | 零 | CI 无镜像构建/签名/发布 job（ITERATION.md:325 自认）；依赖无版本锁定（rosdep lock 无证据）；OTA 在真目标机上的断电/回滚/密钥轮换未测；**v2.3.0-custom-nav 的定制语义未文档化**（钉了什么、为何钉） |

---

## 2. 潜在故障点目录（F 类，按 商用风险×发生概率 排序）

### F1 guard respawn 2s 静默窗口（最高优先）
故障故事：cmd_vel_guard 崩溃 → respawn 2s 内 /cmd_vel 无输出 → 下游执行最后
一条指令持续前进。仿真里 scene_simulator 无指令超时（[scene_simulator_node.cpp:37-39](src/infrastructure/scene_simulator_node.cpp#L37-L39) 存最后指令 20Hz 无条件积分，亲验）；真机语义取决于底盘固件，**仓库无法验证也无契约**。
迭代点：把"cmd_vel 消费方必须在 X ms 无指令时归零"写成**接口契约**（SAFETY.md
+ scene_simulator 率先实现staleness 归零，让仿真语义=真机契约）；guard 死亡时
latch 一条零速（transient_local）使 respawn 窗口内下游收到的是"停"而非"无消息"。

### F2 节点挂死不可见（结构性，两形态都缺）
故障故事：NAV2 某 server 死锁（进程活着、不工作）——无人发现。事实：① supervisor
的 STARTING→RUNNING 确认仍是"存活即确认"（[supervisor_node.cpp:256](src/infrastructure/supervisor_node.cpp#L256) 自注 "v1 健康门：存活即确认；v2 换心跳确认"——v2 从未做）；② health_monitor
的 /health/report 单机形态零消费者（首轮审计 F-1 结论至今成立）；③ **NAV2 生产
launch 全谱系零 supervisor/health 接入**（grep 亲验：nav2_*.launch 无一引用）——
controller/planner/bt_navigator 崩溃只靠 BT 恢复行为，进程级无人管。
迭代点：v2 心跳门（supervisor 订阅 /health/report，五审前 F-1 的改法 b 终于
有了正当性）；NAV2 形态接入 supervisor（children 参数化即可，机制已备）。

### F3 定位丢失/跳变无看门狗
故障故事：AMCL 绑架机器人场景下发散，机器人按错误世界模型全速规划——guard
查的是雷达回波数，不看位姿质量。StampGate 管 perception 时间戳，不管定位
正确性。
迭代点：定位置信度/新息监控（AMCL neff、pose 跳变检测）→ 降级模式（限速
或停止）→ 告警。这是商用 AMR 的高频真故障，仿真里很难复现。

### F4 DDS 发现窗数据丢失（已有实证案例）
故障故事：soak 首个 goal 落在 DDS 发现窗内被丢（volatile QoS）——751m 勘误
里的"仅 1 条事件"就是这个机制的现场记录。商用场景：任务下发与导航启动竞态
= 任务静默丢失。
迭代点：goal/任务类消息 durable 或 latched 语义；启动顺序编排（消费方就绪
再派发）；发现风暴在 N 机车队下的隔离（domain id 分配、SNAP 配置）。

### F5 地图陈旧
故障故事：仓库改了货架，全局图还是 rack_3c——规划穿新障碍，全靠局部代价地图
兜底；若改动在传感盲区（镜面/高反射面）则连兜底都没有。
迭代点：地图版本管理（变更流程+版本号+与定位参数绑定）、重测绘 SOP、
"地图年龄"入健康上报。

### F6 电池（整个子系统缺位）
故障故事：电量 5% 时机器人还在执行 40 分钟任务 → 深放电趴窝在巷道中间，
阻塞整个车间。
迭代点：电量遥测接入（sensor 通道现成）→ 低电分级（拒绝新任务→返航→
原地安全停靠）→ 回充对接（NAV2 docking 已有开源实现可评估）。

### F7 OTA 真机未验
签名链在仿真里闭环，但真目标机断电窗口、A/B 分区回滚、密钥轮换零测试。
迭代点：真机 OTA 演练清单 + 失败回滚的自动化验证（这是 B1 真机批次的
组成部分，不单独排期）。

### F8 车队死锁
两机对向在窄巷道相遇：NAV2 单机行为树解不了跨机博弈。商用多机必须有
交通层（巷道预留/单向道/ centralized traffic control）。
迭代点：先做最小版——巷道级互斥（地图注记 + fleet_manager 签发通行令牌），
不求全功能调度。

### F9 退化模式注入缺失
两轮 soak 注入的全是 kill -9（进程死亡）。真实故障更多是**退化**：雷达部分
射线失效（回波数不降但局部盲）、轮子打滑（odom 漂移）、TF 抖动。fail-safe
链只在"全盲"场景验证过（min_valid_echoes=50 的阈值语义）。
迭代点：SimulatedScene 增加退化注入模式（部分射线丢失/位姿漂移注入），
验证"半盲"区间 guard 行为——min_valid=50 是全有全无的线，商用需要的是
梯度响应（减速带）。

### F10 日志风暴
故障时 20Hz 节点全开 RCLCPP_ERROR → journald 无上限 → 磁盘满 → 更大的
故障（含 OTA 状态目录写入失败）。零轮转配置（grep 亲验）。
迭代点：journald 限额（systemd unit 里已可配，四个 .service 文件顺手）+
故障态日志限速策略。

---

## 3. 性能迭代点

| # | 点 | 现状 | 迭代 |
|---|---|---|---|
| P1 | **控制链端到端 SLO 未定义** | 有 9091 指标与告警规则里的"控制环延迟"，但无 scan→guard→cmd_vel 的端到端预算与目标硬件基线 | 定义 SLO（如 P99 < 80ms）+ 在真机上出第一份基线报告；当前的 benchmarks/results 是桌面/WSL 数据，不能外推 |
| P2 | **NAV2 形态零性能画像** | 两轮 soak 跑的都是已退役旧管线（soak_run.sh grep nav2 = 0，亲验）；NAV2 全栈（costmap 20Hz+AMCL+Smac）在 4C8G 级边板的 CPU/内存足迹未知 | soak 轮 3 切到 nav2_localized + guard——这是"生产形态第一次被长时观测"，比任何新功能都重要 |
| P3 | 栅格规模外推界 | 160KB 快照@400² 已论证；但 rack_3c 只有 19×10m，50×30m 仓库 @3cm 分辨率 → ~16.7M 格 ≈ 16MB/次快照拷贝 + 膨胀计算 | 文档化规模上界（当前参数族的适用范围），大场景需分层/局部代价地图方案（NAV2 本身已提供，写清选型边界即可） |
| P4 | 真机实时性 | /cmd_vel 链路在通用 Linux 上无优先级配置（无 PREEMPT_RT、无 cgroup cpu 分配） | 真机批次做一次调度配置审计；四个 systemd unit 已有 CPUQuota/MemoryMax 的先例，补 CPUSchedulingPolicy=rr + 亲和性 |
| P5 | 启动时间画像 | 恢复画像只测了旧管线（拉起 1.50s 恒定）；NAV2 全栈冷启动（lifecycle 链）未测 | 真机验收项：上电到可接受任务的时间预算 |

---

## 4. 可维护性迭代点

| # | 点 | 现状 | 迭代 |
|---|---|---|---|
| M1 | **NAV2 接线零 CI 门禁** | N-2 绕闸那类回归（launch 接线错误）只有人工审计能抓；quality/ 38 测试零 NAV2 引用 | CI 加 60s 仿真冒烟：起 nav2_localized（scene 底座）→ 断言 /cmd_vel_raw 有流量、/cmd_vel 频率 ≥ 阈值、guard 在节点清单——**这一个测试能永久锁住五轮审计里最贵的那类问题** |
| M2 | 配置三源无 schema | launch 字面量、config/*.yaml、config/env/*.yaml（零消费者）并存；参数名打错=静默默认值 | 参数 schema 化（每节点 declare 集中导出）+ 启动时 strict 校验（未识别参数即 fatal）；env 三套要么接上要么删 |
| M3 | 单包巨石 | 一个包 38+ 测试文件、一个覆盖率分母、改 HAL 与改 NAV2 配置共享 CI 时延 | 按 DDD 边界拆包（hal/domain/infrastructure/nav2_config）——不为美学，为变更隔离与并行评审；也可先做 CMake 级分组 |
| M4 | 节点级测试薄 | cmd_vel_guard 包装层零测试（域 CollisionGuard 18 测）；supervisor infra 层零回归 | guard 节点做一次 lifecycle+remap 级测试（可以用 scene 仿真，10 行级） |
| M5 | 弃用强制力零 | -Wno-error=deprecated-declarations 永久豁免；B3 政策只靠人读 | 到期检查进 CI（grep 弃用符号的调用者），豁免带 TTL |
| M6 | 运营文档缺位 | 有 ADR/journal/技术手册，无 OPERATIONS.md（部署/升级/回滚 SOP）、无 SAFETY.md、告警 annotations 无 runbook 链接 | 写两份顶文档 + 每条告警规则挂 runbook id——商用交付里这两份比代码更被客户读 |
| M7 | 文档残留 | ARCHITECTURE 两张主 mermaid 仍是旧管线全貌（五审 N-7 半项） | 随下一个架构变更一并重画，别再单独排期 |
| M8 | 单人风险 | 全仓单一作者；纪律文化是最大资产也是单点 | CLAUDE.md 的 review 清单已在，补 CONTRIBUTING 与"新人 30 分钟跑通"路径验证 |

---

## 5. 可观测性迭代点

| # | 点 | 现状 | 迭代 |
|---|---|---|---|
| O1 | **告警真接线** | 规则文件质量高（事故史锚定）但 prometheus/alertmanager 零部署，env 已诚实改 false | toolkit compose 加 prometheus+alertmanager+grafana 三服务（一次性投入 ~1 天）；每机器人 label 方案；这是观测闭环从"只看不叫"到"会叫"的唯一剩余工作 |
| O2 | **Deadman 告警缺失** | 现有四组规则全是"指标异常"，没有"机器人静默"——商用最高优先级告警（整机失联）反而没有 | heartbeat 缺失 N 分钟 → 最高级告警（F2 的观测侧对应物） |
| O3 | 日志不可检索 | journald 本地、无结构化、无集中；车队形态下"昨晚 3 号机为什么停了"无法回答 | 结构化 JSON 日志（按事件非按行）+ 车队集中方案（哪怕先 rsync + loki 单机版） |
| O4 | 追踪占位 | LTTng 依赖在、tracepoint 零调用、无会话证据 | 二选一：实现 5 个关键 span（perception/plan/guard/cmd publish）并在真机 bring-up 时实际用一次；或删依赖。当前状态是死重 |
| O5 | **SLO/错误预算未定义** | 无 mission success rate、cmd_vel 可用率、MTTR 的定义与目标——soak 报告的"100% 可用率"是单指标叙事 | 定义 3 个 SLI + 错误预算策略；**soak 报告改为从入库数据生成**（report-from-data 管道）——这一个改动让"叙事 vs 工件"问题在制度上不可能复发 |
| O6 | 健康模型不分形态 | health_monitor 监视的是旧管线节点清单；NAV2 形态无健康发布 | 健康清单参数化按 launch 形态生成（与 F2 的 supervisor 接入同批做） |
| O7 | 指标集封闭 | MetricsRegistry 字段封闭（CLAUDE.md 已警告"别等基类给你指标"） | 真机批次重评：电量/温度/IO 错误率等商用必填字段需要扩维度，提前设计 label 方案避免重写 |

---

## 6. 分阶段路线（试点 → GA）

**试点前置（本迭代，仿真可完成）**
1. F1 指令超时契约（scene 先实现 + SAFETY.md 初版）
2. M1 NAV2 CI 冒烟（锁接线类回归）
3. P2 soak 轮 3 切 NAV2 形态（生产形态首份长时画像）
4. F2 supervisor/health 接入 NAV2 形态 + v2 心跳门
5. O1/O2 告警接线 + deadman
6. M6 OPERATIONS.md/SAFETY.md 初版

**试点期（真机批次，B1 为主轴）**
7. sick_tim781 + 真底盘 + URDF/TF（含 P1 性能基线、P4 调度审计）
8. F9 退化注入 + min_valid 梯度响应
9. F3 定位看门狗 + F10 日志轮转
10. F6 电池最小版（遥测+分级）——商用演示没有它过不了采购评估
11. B4 镜像构建/签名 CI + 真机 OTA 演练

**GA 前置**
12. F8 交通层最小版、F5 地图生命周期、O5 SLO 体系、B2 认证路径立项

---

## 7. L7+ 结语

五个审计周期把这个仓库的**正确性文化**修到了自愈——本轮 N-R1 的处理是终点
证明：引入即被抓、自纠到"发现回归测试本身裸锁"的深度。但要清醒：**这条
轴的边际收益已经开始递减**。五轮审计抓的问题（代码反向、绕闸、叙事化）现在
都有机制防线，同型问题再来一轮不会有新发现。

商用落地的不确定性集中在三条曲线的另外两条：**物理世界**（F1/F3/F5/F6——
仿真原理上无法复现的故障类）和**运营规模**（O5/SLO/车队——单机思维到系统
思维的跨越）。这两条曲线上，这个仓库目前的知识是零——不是因为做得差，
是因为还没开始。五轮审计证明了这个团队"知道自己在哪"的能力；商用阶段
考验的是"知道自己的地图边界在哪"的能力。

一个具体的建议：下一轮迭代把**审计-修复循环改成发布门禁**。现在的循环是
"外部审计发现问题→修复→再审"——这在试点前是正确的学习姿态；试点之后，
客户不会给你第五次机会。tag-guard 和 TSAN nightly 已经是门禁的雏形，
把 M1 的 NAV2 冒烟、O2 的 deadman、F1 的超时契约测试加进去，这个仓库就
从"能通过审计"变成"不需要审计"——那才是商用地落的真正判据。

---

**证据索引（本文件新取证）**：battery/charg/dock 全仓 grep = 0（命中均为
DeclareLaunchArgument/max_rotational_vel 误匹配）· nav2_*.launch 零
supervisor/health 引用（grep -l 空）· 零 logrotate/journald 限额（grep 空）·
soak_run.sh 零 nav2 引用（grep 空）· supervisor_node.cpp:256（v1 存活门
自注）· scene_simulator_node.cpp:37-39,58-62（无指令超时）· N-R1 修复
aa1eb82（:169 自持锁 + N_R1_InjectionVsSnapshot 变体 + 原竞态测试裸锁自纠）·
README 数字改"以 CI 实测为准"公式（:149,151,154）
**文档版本**：1.0（2026-09-08）
