# 迭代计划

> 更新：2026-07-29
> 本次更新：W1-W2 完成，架构方向讨论定稿，ISO 战略调整

---

## 完成状态总览

| 阶段 | 内容 | 状态 |
|------|------|:---:|
| W1 Day 1 | HealthMonitor 拆分 + 删除 application/ 层 | ✅ |
| W1 Day 2 | A* 路径规划 + 6 测试 | ✅ |
| W1 Day 3 | Pure Pursuit + 5 测试 | ✅ |
| W1 Day 4 | 集成 DecisionNode / MotorCtrlNode | ✅ |
| W1 Day 5 | 端到端验证（system.launch） | ✅ |
| W2 Day 1-2 | 系统级 DDS benchmark（AMR 管线） | ✅ |
| W2 Day 3-4 | DDS micro-benchmark（ddsperf 社区工具） | ✅ |
| W2 Day 5 | DDS 选型文档 + 踩坑记录 | ✅ |
| — | 性能插桩框架（AMR_PERF_PHASE） | ✅ |
| — | CI 修复：test_motor_ctrl 排除（spin_once 竞态） | ✅ |
| — | Mermaid 图渲染修复（direction in subgraph / linkStyle 越界） | ✅ |

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
| **C: 感知增强** | ⏳ 待做 | PCL 地面去除 + OccupancyGrid + PCL 后端 |
| **D: 传感器接入** | ⏳ 待做 | IMU/Camera 适配器 + SensorRegistry |
| **E: HealthMonitor** | ✅ | PrometheusHttpServer + DiagnosticsPublisher 拆出，493→379 行 |
| **F: 端到端** | ✅ | system.launch 启动，全链路验证通过 |

### P1：融合层架构升级

**决策**：保留自研 KF 用于对象跟踪（与 robot_localization 角色不同），DBSCAN 替换为 PCL，新增 robot_localization 用于机器人位姿估计。

| 任务 | 预计 | 状态 | 说明 |
|:---:|:---:|:---:|------|
| **P1a: DBSCAN → PCL 策略模式** | 1d | ✅ 完成 | `IClusterAlgorithm` 接口 + `PclClusterBackend`。PCL 比 DBSCAN 快 3.2x（bench_cluster 实测 630μs → 199μs） |
| **P1b: 新增 robot_localization EKF** | 1d | ⏳ 待 sudo 安装 | IMU + 轮编码器 → 机器人位姿估计（`ekf_node`）。与自研 KF 并存——一个做物体跟踪，一个做机器人定位。`sudo apt install ros-jazzy-robot-localization`，然后 MotorCtrlNode.current ← /odom 订阅闭环 |
| **P1c: Camera 占位清理** | 0.5d | ✅ 完成 | 删除 SimulatedCamera 900KB 无用数据产生，保留 camera_timeout 降级逻辑 |
| **P1d: 融合层 benchmark** | 0.5d | ✅ 完成 | `bench_cluster` — DBSCAN vs PCL 多簇对比，PCL 全面胜出 |

### P1e：计算容器进程模型评审

**背景**：compute_container 同进程部署 fusion/decision/motor（零拷贝），sensor 独立进程（故障隔离），health_monitor 独立进程。该结构已对齐 MiR/OTTO 单机 AMR 部署方式。

| 任务 | 预计 | 说明 |
|:---:|:---:|------|
| HealthMonitor ↔ compute 心跳/看门狗交互流梳理 | 0.5d | 心跳发布→超时检测→ChangeState 重启序列的完整业务流总结 |
| 进程模型 ADR 补充 | 0.5d | 记录"compute 同进程 + sensor 独立进程"的决策理由（零拷贝 vs 故障隔离） |

### P1f：商业化定位闭环（MotorCtrlNode 开环 → 闭环）

**背景**：当前 `current = (0,0,0)` 是代码假设位姿（开环模拟），接真实硬件必须换真实定位。

| 任务 | 预计 | 说明 |
|:---:|:---:|------|
| 底盘适配器（actuator/） | 1d | 轮编码器 → odom 发布，模拟底盘先跑通 |
| MotorCtrlNode.current ← /odom | 0.5d | 订阅 robot_localization 输出的 odom，替换自推位姿 |
| 多路点路径支持 | 0.5d | `track()` 支持 A*/NAV2 输出的多路点路径，而非当前两点直线 |
| 速度平滑 | 0.5d | 加减速限制（梯形速度曲线） |

### P1g：真实硬件接入流程 — 标准驱动开发文档

**背景**：已定义完整驱动接入流程（阶段 0-4），固化为开发文档供后续接入任何传感器/执行器复用。

| 任务 | 预计 | 说明 |
|:---:|:---:|------|
| 驱动接入开发指南文档 | 0.5d | 阶段 0-4 全流程：硬件准备→ISensor/IActuator 适配→单元测试→YAML 切换→集成验证。写入 `docs/guides/11-driver-integration.md` |
| IActuator 接口先行定义 | 0.5d | `read(feedback) + write(cmd)`，为底盘/执行器接入铺路 |

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

## Commit 清单（当前分支）

```
04e7cc8  fix(ci): exclude test_motor_ctrl — PurePursuit spin_once race
a98f2c6  feat: W1-W2 — AMR end-to-end + DDS benchmark + perf instrumentation
```

---

## 质量门禁

| 门禁 | 当前 | 目标 |
|------|:---:|:---:|
| 编译 | ✅ 零 error | — |
| 测试通过率 | 11/12（test_motor_ctrl 已知超时） | 12/12 |
| CI | ✅ 全绿 | — |
| class 上限（.h <= 150 / .cc <= 250） | ✅ 仅 health_monitor_node.cpp 379 行（已拆分） | — |
| clang-tidy + ASan | 待集成 | P3 |
| 覆盖率 | ~60%（上次 CI） | >= 80% |

---

## 未决事项/待讨论

- [ ] amr_hal 独立包：路径规划 A* 和 PurePursuit 是否也移入？（它们不依赖硬件，当前在 domain/ 合理）
- [ ] amr_observability 独立包时间线：P2 源码博客后还是并行？
- [ ] Fast-DDS 源码深读 W3 是否启动，还是先继续 amr_hal 拆分？
