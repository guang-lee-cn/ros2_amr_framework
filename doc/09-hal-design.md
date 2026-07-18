# M8：硬件抽象层设计

> 状态：设计中
> 目标：将传感器访问从 ROS2 消息类型解耦，支持模拟/真实硬件切换

---

## 一、当前问题

```cpp
// fusion_node.cpp — 传感器回调直接依赖 sensor_msgs
void FusionNode::lidar_callback(sensor_msgs::msg::LaserScan::SharedPtr msg) {
    perception_.on_lidar(msg->ranges.data(), msg->angle_min, msg->angle_increment);
}
```

换真实硬件时的问题：
- LiDAR 可能是 sick_scan2 的 `/scan` 输出，也可能是 Velodyne 的 `VelodyneScan`
- Camera 可能是 RealSense 的 `Image`，也可能是 USB 摄像头的裸 YUV buffer
- 每次换硬件 → 改 FusionNode callback → 改 PerceptionService 内部处理

**DDD 原则**：domain 层不应该知道 `sensor_msgs` 的存在。

---

## 二、目标架构

```
┌─────────────────────────────────────────────────────────────┐
│ domain/sensor_interface.hpp                                 │
│   ILidarSensor  /  IImuSensor  /  ICameraSensor            │
│   纯虚接口，纯 C++，零 ROS2 依赖                              │
└──────────────────────┬──────────────────────────────────────┘
                       │ 依赖反转
┌──────────────────────┼──────────────────────────────────────┐
│ infrastructure/sensors/                                     │
│                                                              │
│  SimulatedLidar   — 当前 timer + 数学模拟（已有，待适配）     │
│  SickTiM781Adapter — 真实 SICK 驱动 (sick_scan2)             │
│  Bmi088ImuAdapter  — 真实 BMI088 驱动                        │
│  RealSenseAdapter  — 真实 RealSense D435 驱动                │
│                                                              │
│ 每个 Adapter: ROS2 msg → LidarScan/ImuData/CameraFrame      │
└──────────────────────────────────────────────────────────────┘
```

---

## 三、接口设计（已完成）

`domain/perception/sensor_interface.hpp` 定义了三种原始数据类型和对应的纯虚接口：

```cpp
struct LidarScan   { const float *ranges; size_t count; float angle_min/inc; };
struct ImuData     { float accel_x/y; float gyro_z; };
struct CameraFrame { const uint8_t *data; size_t size; uint16_t w/h; };

class ILidarSensor  { virtual bool read(LidarScan &) = 0; };
class IImuSensor    { virtual bool read(ImuData &) = 0; };
class ICameraSensor { virtual bool read(CameraFrame &) = 0; };
```

**关键设计决策**：
- `read()` 返回 `bool`，不阻塞。传感器没准备好 → 返回 false
- 不传 `sensor_msgs` 等 ROS2 类型 → domain 层零 ROS2 依赖
- 接口不含时间戳 → 时间戳由 infrastructure 层（ROS2 pub/sub）管理，不是传感器职责

---

## 四、实现计划

### M8.1：现有模拟传感器适配接口

- [ ] `infrastructure/sensors/simulated_lidar.hpp` — 将现有 LidarNode 的 timer callback 封装为 `ILidarSensor` 实现
- [ ] `infrastructure/sensors/simulated_imu.hpp`
- [ ] `infrastructure/sensors/simulated_camera.hpp`
- [ ] FusionNode 通过接口访问传感器数据，不再直接持有 `sensor_msgs` callback

### M8.2：真实硬件适配器

- [ ] `infrastructure/sensors/sick_tim781.hpp` — 订阅 `/scan` (sensor_msgs/LaserScan)，适配到 `ILidarSensor`
- [ ] `infrastructure/sensors/bmi088_imu.hpp` — 订阅 `/imu/data`，适配到 `IImuSensor`
- [ ] `infrastructure/sensors/realsense_d435.hpp` — 订阅 `/camera/color/image_raw`，适配到 `ICameraSensor`

### M8.3：传感器工厂

```cpp
// 根据 launch 参数或配置文件创建传感器实例
auto lidar = SensorFactory::create_lidar(config);
// config.type = "simulated" → SimulatedLidar
// config.type = "sick_tim781" → SickTiM781Adapter
```

- [ ] `infrastructure/sensors/sensor_factory.hpp`
- [ ] YAML 配置文件 `sensor_config.yaml`

### M8.4：Mock 测试

- [ ] `test/mock/mock_lidar.hpp` — 注入固定 ranges 数组，不依赖 ROS2
- [ ] `test/mock/mock_imu.hpp`
- [ ] 基于 mock 的 PerceptionService 单元测试（当前是集成测试，需要启动 ROS2）

---

## 五、真实传感器移植指南：以 LiDAR 为例

### 5.1 驱动部署的两种模型

```
模型 A：驱动作为独立 ROS2 节点（推荐，当前架构匹配）
─────────────────────────────────────────────────────────
  sick_scan2 (独立进程)        我们的 SickTiM781Adapter (FusionNode 内)
  ┌──────────────────┐        ┌──────────────────────────┐
  │ 硬件驱动           │        │ 订阅 /scan                │
  │ UDP 读数据        │ publish │ 缓存最新 LaserScan        │
  │ 发布 /scan ───────┼───────▶│ read_impl() → LidarScan   │
  └──────────────────┘        └──────────────────────────┘

  部署：ros2 launch sick_scan2 sick_tim.launch.py  ← 独立启动驱动
       ros2 launch ros2_robot_middleware system.launch.py  ← 启动我们的管线

模型 B：SDK 直接嵌入适配器（驱动无 ROS2 封装时使用）
─────────────────────────────────────────────────────────
  SickTiM781Adapter (在 FusionNode 进程内)
  ┌──────────────────────────────┐
  │ #include <sick_scan_sdk.h>   │  ← 直接链接厂商 SDK .so
  │ init_impl(): open_udp()      │
  │ read_impl(): poll_sensor()   │
  └──────────────────────────────┘
```

**当前项目用模型 A**——`SickTiM781Adapter` 订阅 `sensor_msgs/LaserScan`，驱动进程分离。这也是 ROS2 生态的标准做法。

### 5.2 从零移植一个新 LiDAR 驱动的步骤

假设厂商提供了 C SDK（`libnewlidar.so` + `newlidar.h`），但没有 ROS2 驱动：

```
步骤 1：选移植模型
  模型 A（ROS2 node）→ 写一个独立节点，调 SDK，发布 sensor_msgs → 我们适配器不用改
  模型 B（嵌入适配器）→ 在 SickTiM781Adapter 里直接调 SDK → 省一个进程但耦合驱动

  推荐 A —— 驱动故障不会影响管线进程

步骤 2：写 ROS2 wrapper node（模型 A）
  class NewLidarDriver : public rclcpp::Node {
    NewLidarDriver() {
      sdk_handle_ = newlidar_init("/dev/ttyUSB0", 115200);  // SDK 调用
      timer_ = create_wall_timer(10ms, [this]() { publish_scan(); });
    }
    void publish_scan() {
      newlidar_scan_t raw;
      newlidar_read(sdk_handle_, &raw);  // SDK 调用
      auto msg = sensor_msgs::msg::LaserScan{};
      // ... 填充 msg.ranges, msg.angle_min, etc ...
      pub_->publish(msg);
    }
  };

步骤 3：部署
  编译为独立的 ROS2 package（如 newlidar_ros2）
  launch 文件中先启动 newlidar_ros2，再启动我们的管线
  我们的 SickTiM781Adapter 只需把 topic 名从 /scan 改为 /newlidar/scan
```

### 5.3 如何不写代码就适配——改 topic 参数

`SickTiM781Adapter` 的构造函数接受 topic 参数：

```cpp
// 当前：订阅 /scan
SickTiM781Adapter lidar{*this, "/scan"};

// 换传感器：改 topic 名即可
SickTiM781Adapter lidar{*this, "/newlidar/scan"};  // 另一个 LiDAR
```

只要新驱动发布的是 `sensor_msgs/LaserScan`，适配器一行不改。如果新驱动用自定义消息类型（如 `newlidar_msgs/Scan`），则需要写新的 Adapter 类——但接口（`ISensor<LidarScan>`）不变，PerceptionService 不受影响。

---

## 六、迭代计划

### 近期（提升面试技术深度）

| 优先级 | 任务 | 工作量 | 描述 |
|:---:|------|:---:|------|
| 1 | **ClusterDetector → DBSCAN 或 PCL Euclidean** | 1d | 当前 range-threshold 聚类是原型级。移植 DBSCAN 或 PCL 的 Euclidean Clustering，处理多物体、遮挡、噪点。domain 层纯 C++，不影响其他模块 |
| 2 | **跨帧目标跟踪（data association）** | 2d | 当前每帧独立检测，无轨迹。加入匈牙利算法或最近邻关联，KF 预测的 (x,y) 用于关联上一帧的物体 ID，输出带轨迹的 PerceptionObjects |
| 3 | **TF2 坐标变换集成** | 0.5d | 传感器外参（LiDAR→base_link 的静态 TF），PerceptionObjects 输出坐标统一到 base_link 下 |

### 远期

| 优先级 | 任务 | 工作量 | 描述 |
|:---:|------|:---:|------|
| 4 | M9 生产加固 | 2d | spdlog 替换自研 ring buffer、ARM64 交叉编译验证、OTA 模拟 |

### 技术债（低优先级）

| 任务 | 描述 |
|------|------|
| FleetManager 集成测试 | 需要多进程 launch，当前通过手动测试验证 |
| HealthMonitor 真实硬件整合 | 重启机制依赖 `lifecycle_msgs/srv/ChangeState`——当前只在 watchdog 触发时 log warning |
