# ROS2 AMR Framework

A production-grade autonomous mobile robot application framework built on ROS 2 Jazzy. Covers lifecycle management, fault tolerance, QoS tuning, multi-robot coordination, security encryption, and Gazebo simulation.

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
    │  │ PID 1001 │  │ PID 1002 │  │ PID 1003 │  │ watchdog │            │
    │  └────┬─────┘  └────┬─────┘  └────┬─────┘  │ PID 1005 │            │
    │       │   LaserScan │    Imu      │  Image  └──────────┘            │
    │       │             │             │                                 │
    │       ▼             ▼             ▼                                 │
    │  ┌───────────────────────────────────────────┐                      │
    │  │      compute_container (PID 1004)         │ ◄── single process   │
    │  │  ┌──────────┐  ┌──────────┐  ┌─────────┐ │                      │
    │  │  │  Fusion   │  │ Decision │  │MotorCtrl│ │  zero-copy via       │
    │  │  │  + KF +   │──│preemption│──│ Action  │ │  shared_ptr          │
    │  │  │degradation│  │ + retry  │  │ Server  │ │                      │
    │  │  └──────────┘  └──────────┘  └─────────┘ │                      │
    │  │       MultiThreadedExecutor              │                      │
    │  └───────────────────────────────────────────┘                      │
    └────────────────────────────────────────────────────────────────────┘

    All nodes:     rclcpp_lifecycle::LifecycleNode
    Process model: 5 processes (3 sensors + compute + health) — production layout
    Compute layer: fusion + decision + motor_ctrl in single process (zero-copy)
    Sensors:       sensor_msgs (standard types — interoperable with sick_scan2/realsense-ros)
    DDS QoS:       per-topic tuning + Fast-DDS XML profile
    Security:      SROS2 per-node enclave + governance.p7s
    Simulation:    Gazebo Harmonic + ros_gz_bridge
```

## Quick Start

```bash
# ROS 2 Jazzy required
cd ros2_ws
colcon build --packages-select ros2_robot_middleware
source install/setup.bash

# Launch all 8 nodes
ros2 launch ros2_robot_middleware system.launch.py

# Multi-AMR with Fleet Manager
ros2 launch ros2_robot_middleware fleet_multi.launch.py

# Run 13 integration tests
colcon test --packages-select ros2_robot_middleware --return-code-on-test-failure

# Gazebo simulation (headless — remove -s for GUI)
ros2 launch ros2_robot_middleware simulation.launch.py
```

## Milestones

| Milestone | Description |
|-----------|-------------|
| **Phase 1** | 7-node perception-to-actuation pipeline + Docker |
| **M1** | LifecycleNode migration for all nodes |
| **M2** | Sensor degradation (5 levels) + watchdog recovery + action preemption |
| **M3.1** | Custom messages → standard sensor_msgs |
| **M3.2** | 2D Kalman Filter (constant-acceleration model) |
| **M3.3** | Fast-DDS XML profile + adaptive KF timestep |
| **M3.4** | Gazebo Harmonic simulation environment |
| **M4** | Fleet Manager + multi-AMR cluster architecture |
| **M5** | SROS2 security (keystore + per-node ACL) |
| **M6** | GitHub Actions CI + clang-tidy |
| **M7** | **Observability: Traces / Metrics / Logs** ✅ |
| **M8** | **Hardware Abstraction Layer** (planned) — `sensor_interface.hpp` + adapters for real sensors |
| **M9** | **Production hardening** — spdlog migration, OTA simulation, ARM cross-compile |

## Architecture Decisions (ADR)

| # | Decision | Rationale |
|---|----------|-----------|
| ADR-1 | 8 nodes per-device | Fault isolation — single sensor crash does not take down fusion |
| ADR-2 | LifecycleNode base class | Managed startup dependency chain, recovery foundation |
| ADR-3 | Action for motor control | Long-running tasks need Goal→Feedback→Result + cancellation |
| ADR-4 | Raw TCP HTTP for Prometheus | Eliminates bridge middleware — one less failure point per hop |
| ADR-5 | std_msgs/String for heartbeat | Three dimensions (source/time/status) carried orthogonally |
| ADR-6 | Callback-cache fusion | Current baseline; EKF + timestamp alignment planned |
| ADR-7 | Multi-threaded (compute) + single-threaded (sensors) | Hybrid strategy — parallel callbacks for compute, simplicity for sensors |
| ADR-8 | Per-topic QoS differentiation | IMU reliable (incremental), Camera best-effort (absolute) |

See [doc/03-adr.md](doc/03-adr.md) for full context, alternatives considered, and trade-offs.

## Interfaces

### Standard Message Types (M3.1)
| Topic | Type | QoS |
|-------|------|-----|
| `/sensor/lidar` | `sensor_msgs/msg/LaserScan` | best_effort |
| `/sensor/imu` | `sensor_msgs/msg/Imu` | reliable |
| `/sensor/camera` | `sensor_msgs/msg/Image` | best_effort |

### Custom Messages
| Interface | Type | Description |
|-----------|------|-------------|
| `PerceptionObjects` | msg | Object[] objects — fusion output |
| `HealthStatus` | msg | node_name, status (OK/WARN/ERROR/FATAL/STALE), timeout_s |
| `HealthReport` | msg | HealthStatus[] nodes — aggregated health report |
| `SetParam` | srv | Runtime parameter tuning |
| `MoveToPose` | action | Goal→Feedback→Result for motor control |

## Tests

**13 GWT-pattern integration tests** (GoogleTest, LifecycleNode-aware)

| Layer | Tests |
|-------|-------|
| Sensor | LidarNode (360 ranges in physical bounds), ImuNode (noise within BMI088 spec), CameraNode (640×480 rgb8) |
| MotorCtrl | Close-target immediate reach, far-target stepped reach, known/unknown parameter setting |
| Decision | Perception-to-goal dispatch with preemption |
| Fusion | All sensors → object detection; degradation: IMU-missing, LiDAR-missing, all-present, dual-missing→CRITICAL |

## Project Structure

```
ros2_amr_framework/
├── include/ros2_robot_middleware/
│   ├── domain/                     # Pure business logic — zero ROS2 dependency
│   │   ├── perception/             #   KF, clustering, degradation policy
│   │   ├── planning/               #   Target selection, preemption
│   │   ├── execution/              #   Trajectory interpolation
│   │   └── monitoring/             #   Heartbeat analysis, recovery policy
│   ├── application/                # Use case orchestration — depends on domain
│   │   ├── perception_service.hpp
│   │   ├── planning_service.hpp
│   │   ├── execution_service.hpp
│   │   └── monitoring_service.hpp
│   ├── infrastructure/             # ROS2 adapters — only layer allowed to depend on rclcpp
│   │   ├── lidar_node.hpp
│   │   ├── imu_node.hpp
│   │   ├── camera_node.hpp
│   │   ├── fusion_node.hpp
│   │   ├── decision_node.hpp
│   │   ├── motor_ctrl_node.hpp
│   │   ├── health_monitor_node.hpp
│   │   ├── fleet_manager_node.hpp
│   │   └── aliases.hpp
│   └── observability/              # Cross-cutting — traces, metrics, logs
├── src/
│   └── infrastructure/             # Mirrors include structure (domain/app are header-only)
├── test/test_robot_middleware.cpp  # 13 GWT integration tests
├── msg/                            # Custom messages
├── srv/SetParam.srv
├── action/MoveToPose.action
├── config/
│   ├── params.yaml
│   ├── fastdds_profiles.xml        # Per-topic QoS tuning
│   └── sros2/                      # Security keystore + permissions
├── worlds/                         # Gazebo simulation assets
│   ├── warehouse.sdf
│   └── amr.sdf
├── launch/
│   ├── system.launch.py            # Single-AMR full pipeline
│   ├── fleet_multi.launch.py       # Multi-AMR + Fleet Manager
│   ├── simulation.launch.py        # Gazebo + ros_gz_bridge
│   └── system_secure.launch.py     # SROS2 encrypted launch
├── .github/workflows/ci.yml        # CI/CD pipeline
├── .clang-tidy                     # Static analysis configuration
├── doc/                            # Design docs, ADR, observability design
└── toolkit/                        # Docker/Podman multi-stage build
```

## Tech Stack

| Component | Choice |
|-----------|--------|
| ROS 2 | Jazzy Jalisco (LTS, EOL 2029) |
| RMW | Fast-DDS (eProsima) with XML profile |
| Message standard | sensor_msgs — interoperable with sick_scan2, realsense-ros |
| Build | colcon + ament_cmake + static library |
| Test | GoogleTest, 48 cases in 7 modules, LifecycleNode fixtures |
| CI/CD | GitHub Actions + clang-tidy |
| Simulation | Gazebo Harmonic + ros_gz_bridge |
| Security | SROS2 + DDS-Security |
| Observability | Traces (TracerContext) + Metrics (Prometheus + shm) + Logs (ring buffer → JSON) |
| Calman Filter | EKF with pluggable measurement models (linear + range-bearing) |
| Language | C++17 |

## Docs

- [PRD](doc/01-prd.md)
- [Design Document](doc/02-design-doc.md)
- [Architecture Decision Records](doc/03-adr.md) — 8 decisions with context and trade-offs
- [DDS Customization Guide](doc/06-dds-customization.md)

## License

Apache 2.0
