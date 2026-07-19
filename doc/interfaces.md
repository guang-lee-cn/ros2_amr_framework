# 接口设计规范

## DDS Topic 接口

| Topic | 消息类型 | 方向 | 周期/触发 | QoS |
|-------|---------|:---:|:---:|------|
| `/sensor/lidar` | `sensor_msgs/LaserScan` | 入 | 10Hz | best_effort |
| `/sensor/imu` | `sensor_msgs/Imu` | 入 | 100Hz | reliable |
| `/sensor/camera` | `sensor_msgs/Image` | 入 | 5Hz | best_effort |
| `/perception/objects` | `PerceptionObjects` | 内 | 5Hz (200ms tick) | reliable |
| `/cmd/move_to_pose` | `MoveToPose` | 内 | 事件驱动 | Action |
| `/*/heartbeat` | `std_msgs/String` | 内 | 1Hz | reliable |
| `/cmd/status` | `std_msgs/String` | 内 | 1Hz | reliable |
| `/health/report` | `HealthReport` | 内 | 1Hz | reliable |

## Action 接口

| Action | 提供方 | 消费方 | 生命周期 |
|--------|--------|--------|---------|
| `/cmd/move_to_pose` | MotorCtrlNode | DecisionNode | Goal → Feedback (10Hz) → Result |

```cpp
// Goal
float64 target_x, target_y, target_theta
float64 max_speed

// Feedback
float64 current_x, current_y
float64 distance_remaining
float64 percent_complete

// Result
bool reached
float64 final_x, final_y
float64 elapsed_time
```

**取消语义**：消费方调用 `async_cancel_goal()`，提供方 `ACCEPT` 后在执行循环中检查 `goal_handle->is_canceling()`，立即停止并返回 `canceled(result)`。

## Service 接口

| Service | 提供方 | 用途 |
|---------|--------|------|
| `/cmd/set_param` | MotorCtrlNode | 运行时调参（`step_size` 等） |

```cpp
// Request
string param_name
float64 value

// Response
bool success
string message
```

## 进程内接口

| 接口 | 类型 | 调用方 | 实现方 |
|------|------|--------|--------|
| `ISensor<LidarScan>::read()` | 虚函数 | PerceptionService | SimulatedLidar / SickTiM781Adapter |
| `ISensor<ImuData>::read()` | 虚函数 | PerceptionService | SimulatedImu |
| `ISensor<CameraFrame>::read()` | 虚函数 | PerceptionService | SimulatedCamera |
| `ITransformProvider::transform_scan()` | 虚函数 | PerceptionService | Tf2TransformProvider |
| `SensorFactory::create_*()` | 静态工厂 | FusionNode | — |

## 函数 API 约束

| 规则 | 说明 |
|------|------|
| 智能指针所有权 | 构造函数参数用 `T&` (非拥有)，成员变量用 `unique_ptr<T>` (拥有) |
| 回调参数 | ROS2 callback 参数为 `SharedPtr` (DDS 中间件持有所有权) |
| 输出参数 | 大数据 (>1KB) 用引用参数 `T& out`，小数据 (<16B) 用返回值 |
| 线程安全 | `ISensor<T>::read()` 内部自己保证线程安全（值拷贝/mutex），调用方无需加锁 |
| 生命周期 | `ISensor<T>` 的 `init()/shutdown()` 在 LifecycleNode 的 `on_configure/on_cleanup` 中调用 |

## 接口版本兼容

- `.msg` / `.srv` / `.action` 文件新增字段必须在末尾，不删除已有字段，不改变已有字段类型和顺序
- DDS Topic 名变更 → 需要同步更新所有 subscriber/publisher，属于破坏性变更
- `ISensor<T>` 虚接口新增方法 → 旧的 Adapter 实现编译失败，需同步适配
