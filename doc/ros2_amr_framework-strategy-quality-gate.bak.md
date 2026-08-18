# 重构策略文档（strategy.md 产出）

> 生成日期：2026-08-01
> 模式：重构（绞杀者增量）
> 上游：`iso-output.md`（C1 质量门禁 → C3 商业化落地）
> 下游：子系统实现（operations/cpp.md）、集成测试（operations/integration.md）
> 本文件对应 strategy.md §10 输出物结构。

---

## 1. 需求边界确认表

| 需求ID | 需求描述 | 状态 | 风险/依赖 | 替代方案/分期计划 |
|--------|---------|:---:|----------|-------------------|
| R1 | check_limits.sh + 命名 lint 接入 CI 阻塞 | ✅ | 无 | — |
| R2 | 红线清零：health_monitor(379)/fusion(275)/motor(264) SRP 拆分 | ⚠️ | 依赖现有测试兜底 | 拆分范围缩小至抽公共方法，保行为不变 |
| R3 | test_motor_ctrl 修复（timer-based stepping） | ⚠️ | 已知 spin_once race | 限时 1 天，超时用集成测试覆盖 |
| R4 | test_control_loop 提交并通过 | ✅ | 文件已存在（untracked） | 补全断言后提交 |
| R5 | 覆盖率 ≥ 80% | ⚠️ | 存量盲区未知 | 按 coverage_full.txt 逐文件补测 |
| R6 | clang-tidy + ASan 接入 CI | ⚠️ | 存量问题量未知 | baseline 模式，存量进 backlog |
| R7 | quality.sh 自动生成测试数/覆盖率报告 | ✅ | 无 | — |
| R8 | 包名漂移决策 + rename | ⚠️ | 触碰 include/launch/CI/文档 | 决策先行，执行放 P4，grep 校验 |
| R9 | Docker + compose 跑通仿真闭环 | ⚠️ | WSL2 NAT/GPU/权限 | 降级为部署脚本+文档 |
| R10 | IActuator 接口 + 模拟实现单测 | ✅ | 无 | 契约先行，单测锁定 |
| R11 | 文档同步（README/ITERATION/ARCHITECTURE） | ✅ | 依赖 R7 | — |
| R12 | DDD 4→2 层迁移 | ❌ | ISO 排除（C2 未选） | 维持现状，回退机制保留 |

## 2. 可行性分析

### 2.1 业务现状
- 代码可运行（110 测试、CI 绿），但质量门禁缺失：3 处无豁免红线、命名 24/110 达标、test_motor_ctrl 被 CI 排除、覆盖率 78.8%、clang-tidy/ASan 零接入、文档数字三方矛盾。
- 核心矛盾：**"宣称生产级，门禁跑不满"**。真实硬件接入时质量缺口放大。

### 2.2 可复用的设计
- 门禁工具集（check_limits/命名 lint/quality.sh）→ 任何 ROS2/C++ 项目可套用
- domain 算法库（A*/PurePursuit/平滑/误差监控）→ 零 ROS2 依赖，可整包迁移
- IActuator 契约 → 真实底盘/ros2_control 适配入口
- 红线拆分四步法（接口锁定→测试兜底→抽类→回归）→ 方法论 SOP

### 2.3 演进路线
护栏（P1）→ 测试（P2）→ 静态分析+自动报告（P3）→ Docker（P4）/ IActuator（P5）。

## 3. 设计愿景

- **终局**：质量门禁可机器验证、能落在真实底盘上的 ROS2 AMR 参考架构。退出标准：新成员不靠口头解释，靠 CI+文档独立改代码、加传感器、不踩坏测试。
- **可复用沉淀**：门禁工具集 + AMR 控制栈 + 红线拆分 SOP。
- **差异点**：全链路自研 + 门禁即一等公民。对手难复制的是被 CI 强制执行的工程纪律，不是算法。

## 4. 架构设计

### 4.1 分层架构
现状 DDD 四层（hal/domain/infrastructure/observability）基本不动，**新增质量门禁层**（CI 阻塞护栏，不产生业务逻辑）+ IActuator 契约 + 硬件接入点。依赖方向由外向内，hal/domain 零 ROS2 依赖编译期强制。

### 4.2 数据流转（五色规范）
- 数据流（红实线）：传感器→Fusion→Decision→Motor→底盘
- 控制流（蓝虚线）：heartbeat→HealthMonitor，状态→FleetManager
- 状态流（绿）：HealthReport 上报
- 异常流（橙）：传感器超时→5 级降级
- 配置流（紫）：sensors.yaml / params.yaml 启动注入

### 4.3 业务流程
主流程：提交→build 零 warning→check_limits→命名 lint→单测全绿→覆盖率≥80%→ASan 零错误→CI 绿。异常：任一红→revert；motor 超时→集成测试覆盖。

### 4.4 部署拓扑
WSL2/Linux 主机：amr-core 容器（6 节点 + EKF，进程 1-6）+ amr-sim 容器（Gazebo + bridge），容器间 DDS UDP，容器内 SHM 零拷贝。Prometheus/Grafana 可选。

### 4.5 子系统边界

| 子系统 | 职责 | 输入 | 输出 | 依赖 |
|--------|------|------|------|------|
| 质量门禁（新） | CI 阻塞护栏 | 提交代码 | 通过/拦截判定 | 无 |
| hal | 硬件抽象 | 硬件/模拟数据 | ISensor/IActuator | 无 |
| domain | 纯算法 | 传感器数据、目标 | 路径、控制量、判定 | hal 接口 |
| infrastructure | ROS2 适配 | domain/hal 输出 | 消息、动作、指标 | hal + domain |
| observability | 横切 | 打点事件 | 指标/日志 | 无 |
| 部署（新） | Docker compose | 镜像 | 容器 | 全部 |
| 契约（新） | IActuator 数据契约 | — | 单测验证 | hal |

### 4.6 关键决策记录

| # | 决策 | 选项对比 | 结论 | 代价 |
|---|------|---------|------|------|
| D1 | 迁移策略 | 绞杀者 vs 大爆炸 | 绞杀者（110 测试是安全网） | 双轨并存、周期长 |
| D2 | 红线拆分手法 | 机械抽方法 vs SRP 拆类 | SRP 拆功能类 | 类数量增加、需补测 |
| D3 | 静态分析接入 | baseline vs 全量清零 | baseline（防范围蔓延） | 存量进 backlog |
| D4 | 包名 rename 时机 | 立即 vs 测试全绿后 | 测试全绿后（P4） | 需 grep 全量校验 |
| D5 | IActuator 落地 | 等硬件 vs 契约先行 | 契约先行 | 模拟与真实可能有偏差 |

## 5. 接口契约

- **冻结**：DDS Topic/Action/Service（doc/interfaces.md）、msg/srv/action 7+1+1、ISensor<T>/IActuator 签名。变更需 ADR。
- **新增**：IActuator 数据契约 `WheelCmd`/`WheelFeedback`（见 interfaces.md §IActuator 契约）。
- **门禁契约**：check_limits（h>150/cc>250，豁免 模板400/算法300）、命名 lint（TEST_F 100%）、覆盖率 ≥80%、ASan 零错误、clang-tidy 新代码零警告。

## 6. 迭代计划

| 迭代 | 周期 | 交付物 | 依赖 | 人力 | AI占比 | 验收标准 |
|------|:---:|--------|------|:---:|:---:|---------|
| I1 门禁地基 | D1-2.5 | check_limits+命名 lint CI；红线清零 | 无 | 2.5d | 70% | 门禁零违规、原测试绿 |
| I2 测试全绿 | D3-5.5 | motor_ctrl 修复；control_loop 提交；覆盖率≥80% | I1 | 3d | 60% | 全量测试绿、覆盖率达标 |
| I3 静态分析收口 | D6-7.5 | clang-tidy baseline；ASan job；自动报告 | I2 | 2d | 50% | ASan 零错误、报告自动生成 |
| I4 Docker 落地 | D8-9.5 | rename 执行；Dockerfile+compose | I3 | 2d | 60% | compose up 无手工步骤 |
| I5 硬件就绪 | D8-9.5 | IActuator 契约落地+模拟实现+单测；文档收尾 | I3 | 1.5d | 60% | IActuator 单测覆盖契约 |

**关键路径**：I1→I2→I3→I4。I3 是枢纽。
**并行机会**：I4 与 I5 无依赖可并行。
**技术预研**：WSL2 Docker 网络（MVP 首日）、ASan CI 噪音（I3 前置）、motor timer stepping（I2 前置）。

---

## 交接说明

- 子系统实现阶段加载 operations/cpp.md，**仅允许引用本文件的 §4.6 决策与 §5 契约**，禁止回看全局需求描述。
- 每迭代产出经确认后进入下一迭代。
- 实现中与架构假设冲突 → 输出偏差分析，回退至对应 Step。
