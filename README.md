# ROS2 AMR Framework

[![CI](https://github.com/guang-lee-cn/ros2_amr_framework/actions/workflows/ci.yml/badge.svg)](https://github.com/guang-lee-cn/ros2_amr_framework/actions)
[![Coverage](https://img.shields.io/badge/coverage-77.6%25-green)](test/coverage/coverage.txt)
[![ROS2](https://img.shields.io/badge/ROS%202-Jazzy-22303C?logo=ros)](https://docs.ros.org/en/jazzy/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue)](LICENSE)

A production-grade autonomous mobile robot application framework built on ROS 2 Jazzy — lifecycle management, fault tolerance, QoS tuning, multi-robot coordination, SROS2 security, and Gazebo simulation.

## Architecture

```
                    ┌──────────────────────────────────────────────┐
                    │          Fleet Manager                       │
                    │   /fleet/status   /fleet/health              │
                    └────┬──────────────────────────┬──────────────┘
                         │ /amr1/health/report      │ /amr2/health/report
    ┌────────────────────┼──────────────────────────┼──────────────────────┐
    │  AMR-1 (domain=1)  │                          │                      │
    │                    ▼                          ▼                      │
    │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │
    │  │  Lidar   │  │   IMU    │  │  Camera  │  │ Health   │            │
    │  │  10Hz    │  │  100Hz   │  │  5Hz     │  │ Monitor  │            │
    │  └────┬─────┘  └────┬─────┘  └────┬─────┘  │ watchdog │            │
    │       │   LaserScan │    Imu      │  Image  └──────────┘            │
    │       ▼             ▼             ▼                                 │
    │  ┌───────────────────────────────────────────┐                      │
    │  │      compute_container (single process)   │                      │
    │  │  ┌──────────┐  ┌──────────┐  ┌─────────┐ │  zero-copy via       │
    │  │  │  Fusion   │  │ Decision │  │MotorCtrl│ │  shared_ptr          │
    │  │  │  + EKF +  │──│preemption│──│ Action  │ │                      │
    │  │  │degradation│  │ + retry  │  │ Server  │ │                      │
    │  │  └──────────┘  └──────────┘  └─────────┘ │                      │
    │  │       MultiThreadedExecutor              │                      │
    │  └───────────────────────────────────────────┘                      │
    └────────────────────────────────────────────────────────────────────┘

    8 nodes → 5 processes (3 sensors + compute + health) — production layout.
    Fusion + decision + motor_ctrl share a process for zero-copy.
    Sensors use standard sensor_msgs — interoperable with sick_scan2, realsense-ros.
```

## Quick Start

```bash
cd ros2_ws
colcon build --packages-select ros2_robot_middleware
source install/setup.bash

# Single AMR
ros2 launch ros2_robot_middleware system.launch.py

# Multi-AMR fleet
ros2 launch ros2_robot_middleware fleet_multi.launch.py

# Gazebo simulation
ros2 launch ros2_robot_middleware simulation.launch.py

# Run tests
./src/ros2_robot_middleware/test.sh
```

## Tech Stack

| Component | Choice |
|-----------|--------|
| ROS 2 | Jazzy Jalisco (LTS, EOL 2029) |
| RMW | Fast-DDS with XML QoS profile |
| Messages | sensor_msgs (standard) + 5 custom (msg/srv/action) |
| Build | colcon + ament_cmake |
| Test | GoogleTest, 52 cases in 8 modules |
| CI | GitHub Actions |
| Simulation | Gazebo Harmonic + ros_gz_bridge |
| Security | SROS2 + DDS-Security |
| Observability | Traces (TracerContext) + Metrics (Prometheus + shm) + Logs (ring buffer) |
| Filter | EKF with pluggable measurement models |
| Language | C++17 |

## Docs

| Document | Description |
|----------|-------------|
| [Architecture & Project Structure](doc/ARCHITECTURE.md) | DDD layering, directory tree, design principles |
| [Design Document](doc/02-design-doc.md) | Full technical design |
| [ADR](doc/03-adr.md) | 10 architecture decisions with context and trade-offs |
| [Observability Design](doc/07-observability-design.md) | Traces / Metrics / Logs architecture |
| [Observability Usage](doc/08-observability-usage.md) | API, do's/don'ts, iteration plan |
| [HAL Design](doc/09-hal-design.md) | Hardware abstraction layer |
| [DDS Customization](doc/06-dds-customization.md) | Fast-DDS XML profiles |
| [Test Guide](test/README.md) | Running tests, coverage, test patterns |

## Status

| Metric | Value |
|--------|-------|
| Build | [![CI](https://github.com/guang-lee-cn/ros2_amr_framework/actions/workflows/ci.yml/badge.svg)](https://github.com/guang-lee-cn/ros2_amr_framework/actions) |
| Coverage | 77.6% — [test/coverage/](test/coverage/coverage.txt) |
| Tests | 52 cases, 8 modules |
| Lines | ~3,800 (include + src) |
| ROS 2 | Jazzy Jalisco (LTS, EOL 2029) |

## License

Apache 2.0 — see [LICENSE](LICENSE)
