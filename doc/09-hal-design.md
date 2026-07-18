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

### 5.4 模型性能对比

| 环节 | 模型 A（独立进程 + DDS） | 模型 B（SDK 嵌入） |
|------|:---:|:---:|
| 进程间通信 | DDS SHM ~1-5μs / UDP ~50-200μs | 无（同进程） |
| 序列化 | CDR encode+decode ~10-50μs | 原始内存拷贝 ~1-5μs |
| 额外延迟合计 | **~20-200μs** | **~5-10μs** |

在 LiDAR 10Hz（100ms 周期）下，200μs = 0.2%。在 IMU 100Hz（10ms 周期）下，200μs = 2%。**两种场景都远低于实时预算——延迟不是选型因素。**

真正决定选型的维度：

| 维度 | 模型 A | 模型 B |
|------|:---:|:---:|
| 故障隔离 | ✅ 驱动 crash → 只杀自己的进程 | ❌ 驱动 crash → 整个管线挂 |
| 驱动更新 | ✅ `apt upgrade` 不重新编译 | ❌ 重新编译适配器 + 重新链接 |
| 许可证兼容 | ✅ 驱动和我们的代码无链接关系 | ⚠️ GPL 驱动的 copyleft 传染 |
| 社区生态 | ✅ sensor_msgs，任何 ROS2 工具可订阅 | ❌ 自定义格式，脱离生态 |
| RAM | ⚠️ 多一个进程（~50MB） | ✅ 零额外进程 |
| CPU 核利用 | ✅ 驱动可绑独立核 | ⚠️ 共享管线进程核 |

**判断标准**：传感器已有 ROS2 社区驱动 → 模型 A（零成本）。厂商只给 bare C SDK + 无社区驱动 → 模型 B（但要先查许可证）。

---

## 六、同行生产方案参考

### 6.1 Clearpath Robotics（Husky / Warthog）

[Clearpath 传感器集成文档](https://docs.clearpathrobotics.com/docs/ros/config/yaml/sensors/overview/) 揭示的模式：

- **YAML 声明式传感器配置**：每个传感器驱动通过 `ros_parameters` YAML 节点参数化，不在代码中 hardcode。
- **标准消息类型**：全部使用 `sensor_msgs/Image`、`sensor_msgs/LaserScan`、`sensor_msgs/PointCloud2` 等标准类型——不用自定义消息。
- **驱动作为独立节点**：如 LiDAR 拆为 `velodyne_driver_node` + `velodyne_transform_node`，各独立进程。
- **这是模型 A 的生产验证版本**——与我们当前架构思路一致。

### 6.2 NAV2（ROS2 导航栈）

- 全部传感器驱动使用 `rclcpp_lifecycle::LifecycleNode`——和我们一样
- 传感器通过 Lifecycle 状态机管理（configure → activate → deactivate → cleanup）
- 这是 ROS2 生产级系统的默认选择

### 6.3 工业 AMR（MiR / Fetch / Amazon Robotics）

- **安全传感器（SICK safety LiDAR）**：通常通过安全 PLC 硬件级集成，不走 ROS2。安全回路独立于计算回路——这是功能安全（ISO 13849）的要求。
- **非安全传感器（导航用 LiDAR / 深度相机）**：使用 ROS2 驱动，独立进程，发布标准 sensor_msgs。
- **Amazon Robotics**（推测）：驱动层使用自定义 RTOS 保证实时性，上层感知/规划用 ROS2/DDS。
- **公有信息有限**——AMR 公司的传感器集成方案通常属商业机密。

### 6.4 规律总结

| 层次 | 行业做法 | 我们的对齐情况 |
|------|---------|:---:|
| 传感器驱动 | 独立 ROS2 进程 + LifecycleNode | ✅ 一致（模型 A） |
| 消息类型 | 标准 sensor_msgs，不自定义 | ✅ 一致 |
| 配置方式 | YAML 参数化，不 hardcode topic/参数 | ⚠️ 我们当前 hardcode，待改进 |
| 安全传感器 | 安全 PLC 硬线集成，不走 ROS2 | N/A（非本项目范围） |
| 驱动故障隔离 | 独立进程，崩溃不扩散 | ✅ 一致 |
| 模拟 → 真实切换 | 改 launch/YAML 参数 | ✅ 我们的 HAL 支持，改一行 topic |

---

## 七、迭代计划

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
