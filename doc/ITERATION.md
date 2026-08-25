# 迭代计划

> 更新：2026-08-25 — 外部 L7 工程审计报告入库（见文末「外部 L7 工程审计」节），前序记录保持原样
> 历史更新：2026-07-31 — P1-P3 全部完成，控制层自研闭环收官，文档与代码同步

---

## 完成状态总览

| 阶段 | 内容 | 状态 |
|------|------|:---:|
| W1 Day 1 | HealthMonitor 拆分 + 删除 application/ 层 | ✅ |
| W1 Day 2 | A* 路径规划 + 6 测试 | ✅ |
| W1 Day 3 | Pure Pursuit + 5 测试 | ✅ |
| W1 Day 4 | 集成 DecisionNode / MotorCtrlNode | ✅ |
| W1 Day 5 | 端到端验证（system.launch） | ✅ |
| W2 | DDS benchmark + 选型文档 + 踩坑记录 | ✅ |
| — | 性能插桩框架（AMR_PERF_PHASE） | ✅ |
| P1a-P1g | 融合层升级 + EKF 闭环 + HAL 接口 | ✅ |
| P2 | HAL 重组 + observability 拆分 | ✅ |
| P3 | 控制层自研闭环 + 部署方案 | ✅ |
| — | 文档同步（README/CHANGELOG/ARCHITECTURE） | ✅ |

---

## 架构方向决策（2026-07-28/29 定稿）

### D1：HAL 层独立为 amr_hal 包

**决策**：将 `ISensor<T>` 从 `domain/perception/` 下沉为独立 ROS2 package `amr_hal`。

**理由**：
- 传感器 HAL（单向流入）和执行器 HAL（双向流）不能统一为同一接口，但应统一为同一层
- 商业收益：可分派独立团队并行开发，独立版本号，独立入库
- `ISensor<T>` 当前在 domain/ 不违规（零 ROS2 依赖），但放在 domain/ 让新人以为它是"算法"而不是"硬件抽象"

**结构**：
```
amr_hal/
├── CMakeLists.txt
├── include/amr_hal/
│   ├── sensor/
│   │   └── isensor.hpp       ← read() only（继承 IHardware）
│   ├── actuator/
│   │   └── iactuator.hpp     ← read(feedback) + write(cmd)
│   └── common/
│       ├── ihardware.hpp     ← init/shutdown/health/error_code（工具类，不是统一基类）
│       ├── error_codes.hpp   ← 错误码枚举
│       └── registry.hpp     ← 插件注册工厂（替代 SensorFactory if-else）
├── src/sensor/
│   ├── simulated_lidar.cpp
│   ├── simulated_imu.cpp
│   ├── sick_tim781_adapter.cpp
│   └── bmi088_imu_adapter.cpp
├── src/actuator/
│   └── (预留)
└── quality/src/
    └── test_*.cpp
```

### D2：不引入 ros2_control

**决策**：当前阶段不引入 ros2_control。

**理由**：
- ros2_control 定位是执行器 HAL，感知侧的 SensorInterface 后加且不成熟
- 我们的 ISensor<T> 在感知侧比 ros2_control 更干净
- 单平台场景下 ros2_control 增加组织成本（URDF/controller_manager/plugin），收益为零
- 未来如有 3+ 种底盘需求，actuator/ 目录的 IActuator 可适配 ros2_control 的 SystemInterface

### D3：感知用 Topic，执行用 Action — 不绝对

| 通信方式 | 适用场景 | 例 |
|------|------|------|
| Topic | 流式单向数据，可丢帧 | 传感器数据（LiDAR/IMU/Camera） |
| Topic + 平行反馈 | 高频内环，异步反馈 | cmd_vel + odom（PurePursuit 20Hz） |
| Action | 低频任务，需明确完成信号 | MoveToPose 导航目标 |

### D4：common/ 不放统一基类

sensor 和 actuator 在数据语义上没有公共抽象。强行统一 IHardware 基类不带来工程收益。common/ 只放工具类：

- 错误码枚举（error_codes.hpp）
- 插件注册工厂（registry.hpp）
- 速率监控工具（rate_monitor.hpp）
- 构建配置 + 质量门禁

---

## 迭代路线图（更新后）

### P0：商用能力闭环 — ✅ 完成

| 组件 | 状态 | 交付物 |
|------|:---:|------|
| **A: 路径规划** | ✅ | `domain/planning/astar_planner.hpp` + 6 测试 |
| **B: 运动控制** | ✅ | `domain/execution/pure_pursuit.hpp` + 5 测试 |
| **C: 感知增强** | 🟡 部分 | ✅ PCL 后端（3.2x 加速）· ⏳ 地面去除 |
| **D: 传感器接入** | 🟡 部分 | ✅ Registry 插件化 · ⏳ 真实 IMU/Camera 适配器 |
| **E: HealthMonitor** | ✅ | PrometheusHttpServer + DiagnosticsPublisher 拆出，493→379 行 |
| **F: 端到端** | ✅ | system.launch 启动，全链路验证通过 |

### P1：融合层架构升级

**决策**：保留自研 KF 用于对象跟踪（与 robot_localization 角色不同），DBSCAN 替换为 PCL，新增 robot_localization 用于机器人位姿估计。

| 任务 | 预计 | 状态 | 说明 |
|:---:|:---:|:---:|------|
| **P1a: DBSCAN → PCL 策略模式** | 1d | ✅ 完成 | `IClusterAlgorithm` 接口 + `PclClusterBackend`。PCL 比 DBSCAN 快 3.2x（bench_cluster 实测 630μs → 199μs） |
| **P1b: 新增 robot_localization EKF** | 1d | ✅ 完成 | `ekf.yaml` + launch 集成，/odom 输出验证通过。MotorCtrlNode 订阅闭环。与自研 KF 并存（物体跟踪 vs 机器人定位） |
| **P1c: Camera 占位清理** | 0.5d | ✅ 完成 | 删除 SimulatedCamera 900KB 无用数据产生，保留 camera_timeout 降级逻辑 |
| **P1d: 融合层 benchmark** | 0.5d | ✅ 完成 | `bench_cluster` — DBSCAN vs PCL 多簇对比，PCL 全面胜出 |

### P1e：计算容器进程模型评审 — ✅ 完成

**背景**：compute_container 同进程部署 fusion/decision/motor（零拷贝），sensor 独立进程（故障隔离），health_monitor 独立进程。该结构已对齐 MiR/OTTO 单机 AMR 部署方式。

| 任务 | 状态 | 说明 |
|:---:|:---:|------|
| HealthMonitor ↔ compute 心跳/看门狗交互流梳理 | ✅ | 心跳→超时→ChangeState 重启序列已确认 |
| 进程模型决策记录 | ✅ | compute 同进程（零拷贝）+ sensor 独立（故障隔离） |

### P1f：商业化定位闭环 — ✅ 完成

**背景**：当前 `current = (0,0,0)` 是代码假设位姿（开环模拟），接真实硬件必须换真实定位。

| 任务 | 状态 | 说明 |
|:---:|:---:|------|
| MotorCtrlNode.current ← /odom | ✅ | 订阅 robot_localization EKF 输出，无 odom 时 fallback 运动学积分 |
| 多路点路径支持 | ✅ | PurePursuit 重写——路径顺序累积选点 |
| 速度平滑 | ✅ | 梯形加减速 + 曲率限速 + 近目标减速 |
| 底盘适配器（actuator/） | ⏳ P3e | 真实底盘就绪后实现 IActuator |

### P1g：真实硬件接入流程 — ✅ 完成

**背景**：已定义完整驱动接入流程（阶段 0-4），固化为开发文档供后续接入任何传感器/执行器复用。

| 任务 | 状态 | 说明 |
|:---:|:---:|------|
| 驱动接入开发指南文档 | ✅ | `guides/11-driver-integration.md` — 阶段 0-4 全流程 |
| IActuator 接口先行定义 | ✅ | `hal/actuator/iactuator.hpp` — read(feedback) + write(cmd) |

### P2：amr_hal 独立 + 代码质量 — ✅ 完成（方案 B：包内重组）

| 任务 | 状态 | 说明 |
|:---:|:---:|------|
| hal/ 目录 + amr::hal 命名空间 | ✅ | `hal/sensor/` + `hal/actuator/` + `hal/common/` |
| ISensor<T> 下沉 | ✅ | domain/perception → hal/sensor/isensor.hpp |
| IActuator 接口 | ✅ | hal/actuator/iactuator.hpp |
| Simulated*/Sick 迁移 | ✅ | infrastructure/sensors → hal/sensor/ |
| SensorFactory → registry | ✅ | `hal/common/registry.hpp`，静态注册替代 if-else |
| observability → .cpp 拆分 | ✅ | metrics_registry.cpp（logging 保持 header-only，依赖模板类） |
| test_motor_ctrl 修复 | ⏳ 待做 | timer-based stepping 替代 spin_once 阻塞 |

> **决策**：选方案 B（包内重组），非独立包。理由：单人 3 传感器，独立包组织成本 > 收益；接口/实现边界已清晰，将来 `git subtree` 一行拆出。

### P2b：DDS 源码深读 + 博客

| 任务 | 预计 | 说明 |
|------|:---:|------|
| Fast-DDS RTPS StatefulReader | 2d | 可靠传输实现 + 调用链图 |
| Fast-DDS 发现协议 | 2d | PDP/EDP 源码分析 |
| CycloneDDS 对比分析 | 2d | 线程模型 + 内存管理差异 |
| 博客产出 | 1d | 第 1-2 篇源码分析博客 |

### P3：控制层自研闭环 — ✅ 完成

> **决策**：全自研路线，不引入 NAV2。理由：① 成熟大厂（MiR/OTTO/海康）均为自研导航栈，方向一致；② PurePursuit + 梯形速度 + odom 已是商用控制器骨架；③ 避免与 NAV2 正面竞争，展示全链路自研能力。

| 任务 | 状态 | 交付物 |
|:---:|:---:|------|
| P3a 路径平滑 | ✅ | `path_smoother.hpp` — 内切圆弧圆角，无 overshoot，直线密集采样 |
| P3b 跟踪误差监控 | ✅ | `track_error_monitor.hpp` — 横向误差→降速/停止，接入 MotorCtrlNode |
| P3c 动态避障重规划 | ✅ | `grid_updater.hpp` — 感知→膨胀标记→A* 重规划→平滑 |
| P3d 商业部署方案 | ✅ | `deployment-plan.md` — Docker + OTA + 版本锁定 |

**控制层自研闭环**：感知→障碍标记→A*→平滑→PurePursuit→误差监控。

### P3e：规模化 — 后续

| 任务 | 预计 | 说明 |
|:---:|:---:|------|
| slam_toolbox 建图 | 1d | 环境地图（自主定位前置） |
| robot_localization odom0 | 0.5d | 轮编码器输入（真实底盘就绪后） |
| 真实底盘适配器 | 1d | IActuator 实现（硬件在环） |
| Docker 化落地 | 1d | 从 deployment-plan 文档到实际 Dockerfile |
| 观测 SDK 独立包 | 2d | 可选，按需 |

---

## 源码阅读路径（更新）

```
P2：Fast-DDS 源码深读
  ├── Week 1: RTPS 层
  │   ├── StatefulReader.cpp        — 可靠传输 + 预/读
  │   └── RTPSParticipantImpl.cpp   — 节点发现协议
  └── Week 2: 线程模型 + 设计模式
      ├── SHM Transport             — 零拷贝实现
      └── CycloneDDS 对比            — 线程/内存模型差异

方法：每天一个函数，画调用链，200 字笔记
```

---

## Commit 清单（最新到 main）

```
686ec21  docs: reflect final architecture
07baf91  docs: ITERATION — P3 complete, P3e scoped
390eddf  docs(P3d): commercial deployment plan
5fe2e5b  feat(P3c): dynamic obstacle avoidance
6f5e0da  feat(P3b): lateral tracking error monitor
5cde9cc  feat(P3a): corner-rounding path smoother
```

---

## 质量门禁

| 门禁 | 当前 | 目标 |
|------|:---:|:---:|
| 编译 | ✅ 零 error | — |
| 测试通过率 | 14/14（test_motor_ctrl 本地超时，CI 排除待修复） | 15/15 |
| CI | ✅ 全绿 | — |
| class 上限（.h <= 150 / .cc <= 250） | ✅ 已拆分 | — |
| clang-tidy + ASan | 待集成 | P3e |
| 覆盖率 | 78.8% | >= 80% |

---

## 未决事项/待讨论

- [ ] test_motor_ctrl 修复（timer-based stepping 替代 spin_once 阻塞）
- [ ] P3e：真实底盘适配器 + slam 建图 + Docker 落地（依赖硬件环境）
- [ ] Fast-DDS 源码深读 + 博客（P2b）是否启动

---

# 外部 L7 工程审计（2026-08-25）

> **审计立场**：外部 Staff/L7 工程师，按生产级机器人软件标准评判；只认正式文档与代码证据，TODO/计划文档不计入完成度。
> **方法**：三线并行深挖（① 代码质量与架构 ② 测试体系与 CI ③ 运维/可靠性/安全）+ 关键声明命令级人工复核。以下所有 P0/P1 级发现（私钥泄露、覆盖率矛盾、261 测试数、坏宏、静默降级）均已逐条复现验证。
> **体量校准**：单人 10 周（2026-06-15 → 08-25）、242 commits、约 1 万行 C++ 产品代码 + 5093 行测试（36 文件 / 261 用例）+ 18 篇设计文档/ADR。所有评分已按此分母校准——按单人 10 周计，工程文化在职业工程师群体属前 1%；但审计标准是"生产就绪"，不是"个人项目优秀"。

## 1. 总评：6.5 / 10

| 维度 | 评分 | 一句话 |
|---|:---:|---|
| 架构与分层 | **8.5** | DDD 分层可验证地落地，domain 层 33 个头文件零 ROS 依赖，红线未破 |
| 代码质量 | 7.5 | 算法实现教科书级（Joseph-form EKF、防穿角 A*），但 27 处违反自家治理规则 |
| 测试 | 6.5 | 单元测试质量真优秀，但 e2e 为零、覆盖率度量链不可审计 |
| 可靠性工程 | 7.5 | Supervisor 策略内核 + 22 单测是全仓最硬的部分；但 72h soak 未跑、故障注入只有 kill 一种 |
| 安全 | **3** | 私钥已泄露进已推送的 git 历史；DDS security 从未跑通；OTA 签名恒真 |
| 发布工程 | 4 | 版本号三方漂移、无依赖锁定、CI 无镜像发布；CHANGELOG 是唯一亮点 |
| 文档诚实度 | 7 | ADR 记录被拒方案与事故复盘，罕见；但度量 badge 和 quality/README 已腐烂 |

## 2. P0 事故：SROS2 私钥泄露进已推送的 git 历史（先于一切处理）

`git cat-file -p 8c4873b:install/.../config/sros2/private/ca.key.pem` 可完整恢复 CA 私钥明文；共 **11 个私钥文件**（CA 三件套 + 8 个 enclave key）随历史进入 `origin/main`。HEAD 状态干净（`0177fca` 做过清理，`.gitignore` 现在正确），但 blob 永久可恢复，该 keystore 整体应视为已泄露。

处置要求（复现命令：`git log --all --name-only --diff-filter=A | grep 'key\.pem'`）：
1. 轮换全套 sros2 密钥（CA + 8 enclave）；
2. `git filter-repo` 重写历史 + 强推 + 通知所有 clone 方；
3. gitleaks / pre-commit 防再犯检查进 CI。

现状：仓库内无任何轮换文档或应对痕迹——**该泄露至今未被发现**。

## 3. 审计认可的部分（保留并放大）

1. **Domain 层纯净度是结构性的**：全部 domain 头文件 grep ROS 头文件零命中；时钟由 infra 注入；纯 header 策略库使 24/32 个测试模块毫秒级运行。`src/infrastructure/motor_ctrl_node.cpp:66-91`（callback group 饿死）、`decision_node.cpp:368-371`（"锁内绝不调异步接口"）是多数商业 ROS2 团队都没有的并发素养。
2. **B1 Supervisor 是全仓工程质量最高的闭环**：domain 侧五相状态机 + Kahn 拓扑排序（环/重名/未知依赖全拒绝，`domain/monitoring/supervisor_policy.hpp:194-227`），infra 侧独立进程组 + SIGTERM 宽限 + 组杀防孙进程泄漏（`supervisor_node.cpp:139/331-352`），22 单测锁定退避曲线/预算→FATAL/迟到事件免疫，ADR 有 kill -9 实测恢复数字（1.75s / 2.3s）。
3. **单元测试深度**（不是广度）：KF 测试断言 Joseph 协方差对称到 1e-12、Mahalanobis 拒绝后状态不变（`test_kalman_filter.cpp:37-62`）；A* 含"snap 关闭保旧行为"回归锁（`test_astar.cpp:179`）；断言密度 2.46 条/用例，无空壳测试。
4. **文档文化**：注释密度 17.7% 且几乎全是"为什么"级（`collision_guard.hpp:16-38` 直接链接 2026-08-17 盲驶事故复盘）；ADR 记录被拒方案及拒绝理由；CHANGELOG 诚实记录"test_control_loop 曾漏注册从未编译运行"。
5. **双 RMW CI 矩阵是真的**（`.github/workflows/ci.yml:28-33`）：fastrtps/cyclonedds 并行跑全部 261 用例——绝大多数同类项目只敢口头宣称"DDS 可替换"。

## 4. 关键发现

### P1-a 度量链不可信（在 Google 会直接挡 launch）

- `README.md:4` 的 84.5% 徽章是**手写静态 shields.io 链接**，CI 不上传覆盖率到任何地方。
- 同 commit 的 `quality/data/coverage_full.txt` 明细中，**37 个有数据的文件覆盖率全部在 11.4%–61.9%**（astar 14.1% / KF 16.5% / motor_ctrl 17.3% / fusion 11.8%）——鉴于 A* 有 13 个专项测试，14% 明细同样不合理，说明是解析或口径损坏；无论哪边错，**84.5% 无法从仓库数据复算**。
- 历史上已三次发生数字漂移（`doc/iso-output-quality-gate.bak.md:13` 自证）；`quality/README.md:19` 至今写 "52 cases, 8 modules" 并引用不存在的 `./test.sh`。度量腐烂是系统性惯性，不是偶发。
- 门禁旁路：`quality/quality.sh:99-101` lcov 失败静默放行不挡 CI；cppcheck 仅 error 级阻塞；`.clang-tidy` 存在但从未接入 CI；ASan 模式有配置但不进 CI；launch 文件不 lint——`launch/system_secure.launch.py:81` 双逗号语法错误（`ast.parse` 直接崩），证明其**从未被运行过**。

### P1-b "声明能力 > 实际能力"落差清单

| README/文档声明 | 实际 |
|---|---|
| "加传感器零改框架"（`registry.hpp:6-9`） | `AMR_REGISTER_SENSOR` 宏传文档示例的字符串字面量无法编译且全仓零使用；注册走手写 `register_builtin_sensors()`；类型擦除退化为 `void*` + 裸 `new`（`registry.hpp:37/79`） |
| "Prometheus 告警"（README / deployment-plan.md:153-161） | 全仓零 alert rule / Alertmanager 配置，观测体系只"看"不"叫" |
| trace 传播设计（`doc/guides/12-observability-trace-propagation.md`） | 纯设计文档：`msg/PerceptionObjects.msg` 无 trace 字段，tracer 仍是设计里说要替换掉的 thread_local 方案 |
| DDS security（38 个 .pem、enclave 权限齐备） | 唯一启用安全的 launch 有语法错误从未跑通；生产 launch/compose 均未开启安全变量 |
| OTA | 决策层（三不变量 + 防降级计数器，`domain/ota/ota_coordinator.hpp`）真实且好；但签名校验 `/*signature_valid=*/true` 恒真（`ota_agent_node.cpp:100`），物理层是往 /tmp 写 version.txt |
| `docs/test/integration-plan.md` 的 IT-01..15 | 纯计划，零自动化；**全项目没有任何一条"车真动、真停、真避障"的断言式 e2e 测试** |

### P1-c 静默降级是产线安全隐患

- `include/ros2_robot_middleware/hal/sensor/sensor_factory.hpp:66/81/87`：配置拼错驱动名（如 `sick_tim781x`）会**无声 fallback 到仿真传感器**——机器人在现场"看见"假数据。正确行为是 fail-fast 拒绝启动。
- `src/observability/metrics_registry.cpp:29-44`：shm 失败静默降级到进程内单例且**无任何日志**，跨进程指标无声消失，排障黑洞。
- `src/infrastructure/diff_drive_system.cpp:59-63`：`std::stod` 未捕获，URDF 参数非数字会抛异常击穿 controller_manager。

### P2-a 自我治理未收敛

自家 CLAUDE.md 禁裸 `QoS(10)`，实际 **27 处**分布于 9 个文件；AmrNode 基类仅 6/11 节点采用；无 `.clang-format`（2/4 空格、pragma once/include guard 混排）；`motor_ctrl_node.cpp` 的 `execute()` 187 行内三段复制结果块、`elapsed_time` 恒 0、`percent_complete` 起点算错（total_dist 用原点到目标而非起点到目标）、20Hz 常量三处硬编码（252/362/411 行）；两个固化 typo（`vhf_`、`kNdes`）；`kalman_filter.hpp` 全文件无 namespace 直接污染全局。

### P2-b 发布工程断层

package.xml 0.3.0 / CHANGELOG 2.1.0 / git tag v0.1.0 三方漂移；依赖无版本约束、基础镜像浮动 tag；CI 无镜像构建/签名/发布 job；ADR 承认真机需要 systemd 兜底但**全仓没有一个 .service 文件**。

## 5. 落地目标距离评估

对照本仓自己的路线图（`docs/design/20260806-commercial-readiness-gap.md` G1-G7 + `docs/design/architecture-overview.md` §6.1）：

| 里程碑 | 现状 | 距离 |
|---|---|---|
| **仿真级单机闭环**（感知→规划→执行→监控） | **已达成**。AMCL/map_server 已集成（G1 关闭），VFH + 碰撞护栏有实现有测试（G2 代码完成、仿真验证受 gz gpu_lidar 引擎 bug 卡点） | 0.5 个迭代：IT 场景自动化 + 跑通一次完整 e2e 断言 |
| **可对外开源的参考架构**（README 自述定位） | 差的不是功能，是**可信度**：修 P0 泄露、覆盖率复算链、声明-现实落差、文档腐烂 | 1 个迭代（约 1-2 周全职当量） |
| **单机商用试点**（G3/G4 + 真机） | G3 数据基线只有 15min smoke，72h soak 未跑；G4 恢复行为未见实现；**全部验证停留在 Gazebo，真机只有 sick_tim781 适配器存在但从未验证**；安全三件套（密钥/DDS security/告警）全缺 | 2-3 个月全职当量，且必须先过安全 P0 |
| **车队调度/功能安全商用**（G5/G6/G7） | fleet_manager 雏形；VDA5050 未动；ISO 13849/双通道未动 | 季度级以上，未实质启动 |

**一句话**：设计成熟度领先实现与验证成熟度约一个身位。架构与工程文化已够到商业团队水准，但"落地"卡在三件事——**安全基建从没真正开过、度量链不可信、验证停留在仿真且无 e2e 断言**。

## 6. 根因分析（L7 视角）

单人 10 周 242 commits 的高速迭代下，形成"**文档/声明先行于实现，实现先行于验证**"的稳定模式。ADR 和设计文档质量越高，落差越隐蔽——读者（包括未来的自己）会把设计文档误当成实现现状。覆盖率徽章手写、quality/README 腐烂、integration-plan 停留纸面、secure launch 从未运行，是同一根因的四个症状：**仓库缺少"让文档无法说谎"的机制**（CI 生成 badge、数字自动对账、launch lint、e2e 必须绿才能合并）。反证是 supervisor 这条线——它是唯一"ADR→实现→单测→实测数字→CHANGELOG"全程闭环的链路，证明作者完全有能力做到，缺的是把这条链路变成所有链路的默认。

## 7. 行动优先级与迭代清单衔接

**如果只做五件事**：

1. **今天**：轮换 sros2 全套密钥 + `git filter-repo` 重写历史 + 强推 + gitleaks 进 CI（P0）。
2. **本周**：修覆盖率链——CI 上传 lcov、badge 由 CI 生成、修复 `coverage_full.txt` 解析、删除 lcov 失败静默放行旁路（`quality.sh:99-101`）；quality/README 数字对账。
3. **本周**：`sensor_factory` 静默 fallback 改 fail-fast；`system_secure.launch.py` 修语法并让所有 launch 文件 `ast.parse` 进 CI lint。
4. **本迭代**：IT-01..15 中最关键的 3 个场景（到点停车、遇障停车、恢复）做成断言式 e2e 进 ctest——"车真的停了"是本项目目前最值钱也最缺失的一条断言。
5. **下迭代**：正式 72h soak 出报告入库 + 补 systemd unit + 版本号三方对齐 + 依赖锁定。

**与迭代 2 清单（`docs/design/20260824-iteration2-audit-goals.md`）的映射**：

| 审计行动 | 迭代 2 对应项 | 说明 |
|---|---|---|
| 1 密钥轮换 + 历史重写 | **无对应 → 建议新增 P0 项** | 迭代 2 清单缺口，最高优先 |
| 2 覆盖率链修复 | 无直接对应（B3 仅覆盖 API 版本化） | 建议并入 B 类 |
| 3 fail-fast + launch lint | 无对应 | 半天级，可挂 E 类 |
| 4 e2e 断言式测试 | 与 A2 互补（A2 是长稳，不含功能断言） | 建议新增 A5 |
| 5 72h soak + systemd + 版本对齐 | **A2 直接对应**（harness 已备，缺正式跑）；systemd/版本对齐为新增 | 外部审计印证 A2 方向 |

审计对迭代 2 清单的印证：**B1 被评为全仓最硬闭环**（方向正确的实证）；**C1（tracer→LTTng）与审计"trace 纸面"发现一致**；A 类（真机 bring-up）被确认为"单机商用试点"里程碑的核心依赖，优先级判断成立。

**结语**：按 Google 标准，本仓库的分层架构、算法正确性和文档文化可以过 Design Review；但过不了 Security Review（私钥）、也过不了 Launch Review（不可复算的度量、缺失的 e2e、恒真的签名桩）。距离 README 第一定位（开源工程规范演示）只差一次"把声明兑现"的清扫，距离商业落地还隔着安全、真机、数据基线三座山——G1-G7 差距清单写得足够诚实，路线是对的。
