# ROS2 AMR Framework

[![CI](https://github.com/guang-lee-cn/ros2_amr_framework/actions/workflows/ci.yml/badge.svg)](https://github.com/guang-lee-cn/ros2_amr_framework/actions)
[![Coverage](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/guang-lee-cn/ros2_amr_framework/main/quality/data/badge.json)](quality/data/badge.json)
[![ROS2](https://img.shields.io/badge/ROS%202-Jazzy-22303C?logo=ros)](https://docs.ros.org/en/jazzy/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue)](LICENSE)

AMR 参考架构：**NAV2 导航 + 自研安全域**双层形态。基于 ROS 2 Jazzy，展示生产级 ROS2 应用的 DDD 分层、HAL 抽象、可观测性、降级管理与工程化实践。分层应用架构 + 平台收敛层（`amr::qos` QoS 词汇表 · `AmrNode` 基类 · `pipeline.nodes` 声明式组合），不是一个可安装的通用中间件——收敛层让「正确的用法成为最省力的用法」。

> **定位**（2026-09-04 导航收敛后）：规划/控制/定位采用 NAV2（对标商用 AMR
> 主流路线），自研部分聚焦**安全域**（CollisionGuard 安全闸、supervisor
> 进程监管、健康降级链）与工程化底座——A/B 实证见
> [收敛决策记录](docs/design/20260904-nav2-convergence-decision.md)。
> 自研规划管线（fusion→decision→motor）保留为 A/B 基线与测试载体。

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  HAL 层 (amr::hal)  — 硬件抽象，插件注册                        │
│  ├─ sensor/isensor.hpp        ISensor<T> (LidarScan/ImuData/...)│
│  ├─ sensor/simulated_*.hpp    模拟传感器                        │
│  ├─ sensor/sick_tim781.hpp    真实 LiDAR 适配器                 │
│  ├─ sensor/sensor_factory.hpp Registry 驱动（插件化，零 if-else）│
│  ├─ actuator/iactuator.hpp    IActuator<Cmd,Fb> (双向)          │
│  └─ common/registry.hpp       静态插件注册表                    │
└──────────────────────────────────────────────────────────────┘
                    │ ISensor / IActuator
┌──────────────────────────────────────────────────────────────┐
│  Domain 层 (amr::domain)  — 纯 C++ 算法，零 ROS2               │
│  ├─ perception/   DBSCAN/PCL 聚类 · KF · Tracker · 降级策略     │
│  ├─ planning/     A* 路径规划 · 路径平滑 · 障碍标记 · 误差监控   │
│  ├─ execution/    PurePursuit + 梯形速度                        │
│  └─ monitoring/   心跳分析 · 恢复策略                           │
└──────────────────────────────────────────────────────────────┘
                    │ 注入
┌──────────────────────────────────────────────────────────────┐
│  Infrastructure 层 — ROS2 Node + DDS                          │
│                                                              │
│  导航生产链路（NAV2 + 自研安全闸）                              │
│  /scan_raw ─▶ map_server+AMCL ─▶ Smac 规划 ─▶ DWB 控制        │
│  controller ─/cmd_vel_raw─▶ cmd_vel_guard(自研 CollisionGuard)│
│                          ─/cmd_vel─▶ 底盘/场景仿真            │
│                                                              │
│  遗留计算管线（A/B 基线与测试载体）    基础设施 (独立)           │
│  ┌─────────────────────┐             health_monitor           │
│  │ fusion → decision   │             :9090 metrics            │
│  │  → motor (零拷贝)   │             amr_supervisor           │
│  └─────────────────────┘             :9091 perf (ON 构建)     │
└──────────────────────────────────────────────────────────────┘
```

**关键设计**：
- **双层导航**：NAV2 管「怎么走」（定位/规划/控制/恢复行为），自研安全闸管
  「允不允许走」（近障减速→硬停、全盲 fail-safe、stale 超时）——职责分离，
  闸只钳速度不否决目标
- **三段式定位**：部署期 SLAM 建图一次（nav2_scene）→ 运行期预建地图+AMCL
  定位（nav2_localized，修正量波动 RMS 68mm）
- **进程隔离**：传感器/安全闸独立进程，health_monitor 独立
- **降级与恢复**：传感器超时 → 5 级降级；两级看门狗——supervisor 进程级
  （kill -9 秒级按策略恢复）+ health_monitor lifecycle 级（异步四步重启，
  行为测试实证）。Prometheus 指标可观测；**告警规则未实现**（观测"只看不叫"，
  见 deployment-plan）

## Quick Start

```bash
cd ros2_ws
colcon build --packages-select ros2_robot_middleware
source install/setup.bash

# NAV2 生产形态（预建地图 + AMCL 定位 + 安全闸）
ros2 launch ros2_robot_middleware nav2_localized.launch.py
# Foxglove: ws://localhost:8765（/map 预建图、/plan、/scan_raw、
#           /robot_model、/obstacles）

# 开发建图形态（在线 SLAM，部署期用；场景可换 rack_3c/rack_4box/
# warehouse_open，可叠随机箱与移动障碍）
ros2 launch ros2_robot_middleware nav2_scene.launch.py \
    scene:=warehouse_open random_boxes:=6 movers:=2 mover_speed:=0.6

# 遗留自研栈（A/B 基线；详见 ab_custom 与决策记录）
ros2 launch ros2_robot_middleware system.launch.py

# 多 AMR（单机演示形态：同机多实例；跨机 fleet 治理未实现——
# fleet_manager 目前仅被动聚合心跳）
ros2 launch ros2_robot_middleware fleet_multi.launch.py

# 发导航目标（两形态通用）
ros2 action send_goal /navigate_to_pose nav2_msgs/action/NavigateToPose \
  "{pose: {header: {frame_id: 'map'}, pose: {position: {x: 17.0, y: 0.0}}}}"

# 单元测试
./quality/quality.sh
# 或直跑 ctest（前置：source /opt/ros/jazzy/setup.bash && source install/setup.bash，
# 再进 build/ros2_robot_middleware；缺 source 会报 typesupport 库加载失败）
cd build/ros2_robot_middleware && ctest

# 性能插桩模式（启用 AMR_PERF_PHASE 打点 + :9091 端点）
colcon build --packages-select ros2_robot_middleware \
  --cmake-args -DAMR_PERF_INSTRUMENTATION=ON
```

### 观测

```bash
# 运行后：
curl localhost:9090/metrics   # 健康/速率/降级/延迟
curl localhost:9091/metrics   # AMR_PERF_PHASE 阶段延迟 (ON 构建)
# Grafana: 导入 config/grafana/amr_dashboard.json
```

## Benchmarks & DDS 快速切换（benchmarks/）

- `./benchmarks/build.sh` 编译基准包（colcon 不向包目录内递归，须显式 base-path）
- `scripts/rmw_matrix.sh` —— **同一份代码双 DDS 一键对比**（CycloneDDS vs FastDDS）；
  CI 同步跑双 RMW 矩阵，「DDS 可替换」是被持续验证的事实
- 基准矩阵：IPC 时延 / 进程内零拷贝 / QoS（reliable vs best_effort）/ durability 晚加入 /
  故障恢复时间 / zenoh 端云（DROP/BLOCK）——方法见 `benchmarks/docs/ADR-001`
- JD 技能点全地图：`mdDoc/JD技能点地图-brainco.md`

## Tech Stack

| Component | Choice |
|-----------|--------|
| ROS 2 | Jazzy Jalisco (LTS, EOL 2029) |
| RMW | Fast-DDS / CycloneDDS 可切换（`RMW_IMPLEMENTATION`） |
| 导航 | NAV2（SmacPlanner2D + DWB + 行为树恢复） |
| 定位 | 部署期 slam_toolbox 建图 → 运行期 map_server + AMCL |
| 安全层 | 自研 cmd_vel_guard（域 CollisionGuard，fail-safe 矩阵） |
| 仿真 | SimulatedScene 纯 CPU 射线投射（随机/移动障碍）；Gazebo 受限（gz 雷达渲染死亡，平台无关） |
| 感知 | 自研 KF + Tracker + 降级；PCL/DBSCAN 聚类（策略模式，A/B 基线） |
| HAL | `amr::hal` 插件注册（ISensor + IActuator） |
| 观测 | Prometheus :9090 常开；:9091 仅 AMR_PERF_INSTRUMENTATION=ON 构建存在 |
| Build | colcon + ament_cmake |
| Test | GoogleTest（用例/模块数以 CI 实测为准，勿手写——数字漂移第 4 次教训） |
| CI | GitHub Actions (静态分析 → 构建 → 测试 → 覆盖率) |
| Language | C++17 |

## Docs

| Document | Description |
|----------|-------------|
| [Architecture Overview](doc/ARCHITECTURE.md) | 数据流/控制流/状态流 + 分层图 |
| [NAV2 Stack](doc/subsystems/nav2-stack.md) | 导航栈集成：launch 矩阵/话题契约/安全闸/建图流程 |
| [导航收敛决策](docs/design/20260904-nav2-convergence-decision.md) | NAV2 移交 + 安全域保留的 ADR（A/B 证据） |
| [Iteration Plan](doc/ITERATION.md) | P0-P3 迭代路线与完成状态 |
| [DDS Selection](doc/dds-selection-guide.md) | Fast-DDS vs CycloneDDS 选型 + benchmark |
| [Benchmark Lessons](doc/benchmark-lessons-learned.md) | DDS 测试踩坑与手把手流程 |
| [Driver Integration](doc/guides/11-driver-integration.md) | 硬件驱动接入 4 阶段指南 |
| [Deployment Plan](doc/deployment-plan.md) | 部署路线（roadmap：Docker/RAUC 真机待落地） |
| [Subsystem Docs](doc/subsystems/) | 传感器/融合/决策/执行/健康监控/可观测性 |
| [ADR](doc/adr/03-adr.md) | 架构决策记录 |
| [HAL Design](doc/guides/09-hal-design.md) | 硬件抽象层设计 |
| [Quality Guide](quality/README.md) | 质量门禁、测试规范 |

## Status

| Metric | Value |
|--------|-------|
| Build | [![CI](https://github.com/guang-lee-cn/ros2_amr_framework/actions/workflows/ci.yml/badge.svg)](https://github.com/guang-lee-cn/ros2_amr_framework/actions) |
| Coverage / Tests | 以顶部徽章（CI 实测 badge.json）与 [Actions](https://github.com/guang-lee-cn/ros2_amr_framework/actions) 为准——手写数字已删除（复审 §8.3-7） |
| 控制层 | NAV2 规划/控制 + 自研安全闸（A/B 实证 4/4 vs 0/4，收敛记录见 docs/design） |
| HAL | 插件注册，加传感器零改框架 |
| ROS 2 | Jazzy Jalisco (LTS, EOL 2029) |

## License

Apache 2.0 — see [LICENSE](LICENSE)
