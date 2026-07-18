# Architecture & Project Structure

## DDD Layering

```
include/ros2_robot_middleware/
├── domain/          ← Pure business logic, zero ROS2 dependency
│   ├── perception/  ← KF, cluster detection, degradation policy
│   ├── planning/    ← Target selection, preemption
│   ├── execution/   ← Trajectory interpolation
│   └── monitoring/  ← Heartbeat analysis, recovery policy
├── application/     ← Use case orchestration, depends on domain
│   ├── perception_service.hpp
│   ├── planning_service.hpp
│   ├── execution_service.hpp
│   └── monitoring_service.hpp
├── infrastructure/  ← ROS2 adapters (only layer allowed to #include <rclcpp>)
│   ├── lidar_node.hpp, imu_node.hpp, camera_node.hpp
│   ├── fusion_node.hpp, decision_node.hpp, motor_ctrl_node.hpp
│   ├── health_monitor_node.hpp, fleet_manager_node.hpp
│   ├── aliases.hpp
│   └── sensors/     ← HAL sensor implementations
└── observability/   ← Cross-cutting: traces, metrics, logs
    ├── ring_buffer.hpp, metrics_registry.hpp, tracer.hpp
    └── log_event.hpp, log_worker.hpp, logging.hpp, trace_points.hpp
```

## Source Tree

```
ros2_amr_framework/
├── include/ros2_robot_middleware/   ← Public headers (DDD 4-layer)
├── src/infrastructure/             ← Node implementations
│   ├── *_node.cpp                  ← 8 node classes
│   ├── *_main.cpp                  ← 8 entry points
│   └── compute_container.cpp       ← Composable container entry
├── test/                           ← Test suite (8 modules, 52 cases)
│   ├── test_sensor_nodes.cpp       ← 3 tests
│   ├── test_motor_ctrl.cpp         ← 4 tests
│   ├── test_decision.cpp           ← 1 test
│   ├── test_fusion.cpp             ← 3 tests
│   ├── test_observability.cpp      ← 11 tests
│   ├── test_kalman_filter.cpp      ← 8 tests
│   ├── test_monitoring.cpp         ← 14 tests
│   ├── test_sensor_hal.cpp         ← 8 tests
│   ├── test.sh                     ← Build + test + coverage script
│   └── README.md
├── msg/                            ← Custom ROS2 messages
│   ├── Object.msg
│   ├── PerceptionObjects.msg
│   ├── HealthStatus.msg
│   └── HealthReport.msg
├── srv/SetParam.srv                ← Parameter tuning service
├── action/MoveToPose.action        ← Motor control action
├── launch/                         ← ROS2 launch files
│   ├── system.launch.py            ← Single AMR (production)
│   ├── fleet_multi.launch.py       ← Multi-AMR fleet
│   ├── simulation.launch.py        ← Gazebo simulation
│   └── system_secure.launch.py     ← SROS2 encrypted
├── config/                         ← Runtime configuration
│   ├── params.yaml
│   ├── fastdds_profiles.xml        ← Per-topic QoS
│   ├── grafana_dashboard.json      ← Prometheus dashboard
│   └── sros2/                      ← Security keystore
├── worlds/                         ← Gazebo simulation assets
├── doc/                            ← Design documents, ADR
├── toolkit/                        ← Docker/Podman builds
├── .github/workflows/ci.yml        ← CI pipeline
└── .clang-tidy                     ← Static analysis config
```

## Process Model

```
PID 1: lidar_node         — independent (driver fault isolation)
PID 2: imu_node           — independent
PID 3: camera_node        — independent
PID 4: compute_container  — fusion + decision + motor_ctrl (zero-copy, shared memory)
PID 5: health_monitor     — independent (must not share fate with monitored nodes)
PID 6: fleet_manager      — independent (cross-AMR orchestration)

8 nodes → 6 processes. Compute nodes share a process via MultiThreadedExecutor.
Sensor drivers and health monitor remain isolated for fault containment.
```

## Dependency Rules (enforced at compile time)

```
domain/        → zero ROS2 headers
application/   → depends on domain/ only
infrastructure/→ depends on domain/ + application/ + rclcpp
observability/ → zero project-internal dependencies (pure utility)
```

## Key Interfaces

| Interface | Type | Topic / Service |
|-----------|------|-----------------|
| LiDAR scan | `sensor_msgs/LaserScan` | `/sensor/lidar` (best_effort) |
| IMU data | `sensor_msgs/Imu` | `/sensor/imu` (reliable) |
| Camera frame | `sensor_msgs/Image` | `/sensor/camera` (best_effort) |
| Perception output | `PerceptionObjects` | `/perception/objects` (reliable) |
| Health report | `HealthReport` | `/health/report` (reliable) |
| Motor control | `MoveToPose` (action) | `/cmd/move_to_pose` |
| Parameter tuning | `SetParam` (service) | `/cmd/set_param` |
