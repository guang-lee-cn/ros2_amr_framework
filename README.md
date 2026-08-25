# ROS2 AMR Framework

[![CI](https://github.com/guang-lee-cn/ros2_amr_framework/actions/workflows/ci.yml/badge.svg)](https://github.com/guang-lee-cn/ros2_amr_framework/actions)
[![Coverage](https://img.shields.io/badge/coverage-84.5%25-brightgreen)](quality/data/coverage.txt)
[![ROS2](https://img.shields.io/badge/ROS%202-Jazzy-22303C?logo=ros)](https://docs.ros.org/en/jazzy/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue)](LICENSE)

AMR **感知-决策-执行全链路自研**参考架构。基于 ROS 2 Jazzy，展示生产级 ROS2 应用的 DDD 分层、HAL 抽象、可观测性、降级管理与工程化实践。分层应用架构 + 平台收敛层（`amr::qos` QoS 词汇表 · `AmrNode` 基类 · `pipeline.nodes` 声明式组合），不是一个可安装的通用中间件——收敛层让「正确的用法成为最省力的用法」。

> **定位**：感知、决策、控制全链路自研（对标 MiR/OTTO 等成熟 AMR 厂商路线），开源用于：① ROS2 传感器标准接入层 ② 工程规范演示 ③ 可扩展架构模板。

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
│  传感器进程 (独立)         计算容器 (单进程)   基础设施 (独立)  │
│  lidar_node  ──/scan──▶  ┌─────────────────────┐  health_monitor│
│  imu_node    ──/imu───▶  │ fusion → decision   │  :9090 metrics │
│  camera_node ─/image─▶   │  → motor (零拷贝)   │                │
│                          └─────────────────────┘  compute_container│
│  定位: ekf_node (robot_localization EKF) → /odom   :9091 perf   │
└──────────────────────────────────────────────────────────────┘
```

**关键设计**：
- **进程隔离**：传感器驱动独立进程（故障隔离），compute 单进程（零拷贝），health_monitor 独立
- **控制闭环**：odom 反馈（robot_localization EKF）→ PurePursuit → 误差监控自纠
- **动态避障**：感知物体 → 网格标记 → A* 重规划 → 路径平滑
- **降级**：传感器超时 → 5 级降级，看门狗重启，Prometheus 告警

## Quick Start

```bash
cd ros2_ws
colcon build --packages-select ros2_robot_middleware
source install/setup.bash

# 单 AMR（含 EKF 定位）
ros2 launch ros2_robot_middleware system.launch.py

# 多 AMR 集群
ros2 launch ros2_robot_middleware fleet_multi.launch.py

# Gazebo 仿真
ros2 launch ros2_robot_middleware simulation.launch.py

# 单元测试
./quality/quality.sh

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
| 定位 | robot_localization EKF（IMU + odom） |
| 规划 | 自研 A* + 圆弧路径平滑 + 动态避障 |
| 控制 | 自研 PurePursuit + 梯形速度 + 误差监控 |
| 感知 | 自研 KF + Tracker + 降级；PCL/DBSCAN 聚类（策略模式） |
| HAL | `amr::hal` 插件注册（ISensor + IActuator） |
| 观测 | Prometheus (:9090 + :9091) + AMR_PERF_PHASE + Grafana |
| Build | colcon + ament_cmake |
| Test | GoogleTest, 261 cases, 32 modules |
| CI | GitHub Actions (静态分析 → 构建 → 测试 → 覆盖率) |
| Simulation | Gazebo Harmonic + ros_gz_bridge |
| Language | C++17 |

## Docs

| Document | Description |
|----------|-------------|
| [Architecture Overview](doc/ARCHITECTURE.md) | 数据流/控制流/状态流 + 分层图 |
| [Iteration Plan](doc/ITERATION.md) | P0-P3 迭代路线与完成状态 |
| [DDS Selection](doc/dds-selection-guide.md) | Fast-DDS vs CycloneDDS 选型 + benchmark |
| [Benchmark Lessons](doc/benchmark-lessons-learned.md) | DDS 测试踩坑与手把手流程 |
| [Driver Integration](doc/guides/11-driver-integration.md) | 硬件驱动接入 4 阶段指南 |
| [Deployment Plan](doc/deployment-plan.md) | 商业部署：Docker + OTA + 版本锁定 |
| [Subsystem Docs](doc/subsystems/) | 传感器/融合/决策/执行/健康监控/可观测性 |
| [ADR](doc/adr/03-adr.md) | 架构决策记录 |
| [HAL Design](doc/guides/09-hal-design.md) | 硬件抽象层设计 |
| [Quality Guide](quality/README.md) | 质量门禁、测试规范 |

## Status

| Metric | Value |
|--------|-------|
| Build | [![CI](https://github.com/guang-lee-cn/ros2_amr_framework/actions/workflows/ci.yml/badge.svg)](https://github.com/guang-lee-cn/ros2_amr_framework/actions) |
| Coverage | 84.5% — [quality/data/](quality/data/coverage.txt) |
| Tests | 261 cases, 32 modules（2026-08-25 本地全绿） |
| 控制层 | 自研闭环：规划/避障/执行/自感知 |
| HAL | 插件注册，加传感器零改框架 |
| ROS 2 | Jazzy Jalisco (LTS, EOL 2029) |

## License

Apache 2.0 — see [LICENSE](LICENSE)
