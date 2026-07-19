# Roadmap — Future Work

## Completed (Phase 1–2)

- [x] 8-node perception-to-actuation pipeline
- [x] LifecycleNode migration (M1)
- [x] Sensor degradation + watchdog recovery + action preemption (M2)
- [x] sensor_msgs standardization (M3.1)
- [x] 2D Kalman Filter with Joseph form + Mahalanobis outlier rejection (M3.2–3.3)
- [x] Gazebo Harmonic simulation (M3.4)
- [x] Fleet Manager + multi-AMR (M4)
- [x] SROS2 security (M5)
- [x] CI/CD + clang-tidy (M6)
- [x] compute_container: fusion + decision + motor_ctrl in single process

## Short Term

### DDD Layered Architecture Refactoring

Current flat-node architecture mixes domain logic with ROS2 infrastructure. DDD refactoring extracts 4 bounded contexts:

| Context | Domain Entities | Core Services |
|---------|----------------|---------------|
| Perception | Object, Cluster, LaserScan | KalmanFilter, ClusterDetector, DegradationPolicy |
| Planning | Goal, MotionProfile | TargetSelector, PreemptPolicy |
| Execution | MotorCommand, Feedback | Interpolator, CancelHandler |
| Monitoring | NodeHealth, FleetState | HealthAggregator, RecoveryPolicy |

Target: domain layer with zero ROS2 dependency, fully unit-testable.

See `mdDoc/ddd-architecture.md` for detailed design.

### End-to-End Latency Benchmark

- Measure: LiDAR data generation → motor_ctrl starts execution
- Compare: independent processes vs compute_container
- Tools: `ros2 topic delay`, custom timestamp injection

## Medium Term

### SLAM / nav2 Integration

- Replace callback-cache fusion with `slam_toolbox` (2D LiDAR SLAM)
- Replace DecisionNode rule engine with `nav2` behavior tree
- Dependency: `ros-jazzy-slam-toolbox`, `ros-jazzy-navigation2`

### Executor Model Optimization

- Sensor nodes: `StaticSingleThreadedExecutor` for deterministic scheduling
- compute_container: benchmark MultiThreadedExecutor thread contention
- Health monitor: separate `SingleThreadedExecutor` for isolation

## Long Term

### LLM-Based Decision Engine

- Replace `DecisionNode::on_perception()` with LLM inference
- Pipeline: `PerceptionObjects → JSON prompt → llama.cpp → goal (x, y)`
- Reserved interface: `send_goal(target_x, target_y)` unchanged
- Fallback: rule engine if LLM unavailable

### Real Sensor Driver Integration

- LiDAR: `sick_scan2` → `sensor_msgs/msg/LaserScan` (already compatible)
- IMU: `bno055` → `sensor_msgs/msg/Imu` (already compatible)
- Camera: `realsense-ros` → `sensor_msgs/msg/Image` (already compatible)

### Multi-AMR Production Deployment

- ROS_DOMAIN_ID isolation per AMR
- DDS Partitions for Fleet Manager cross-domain communication
- MQTT Bridge to cloud (Grafana dashboard)
