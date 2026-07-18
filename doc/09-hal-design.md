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

## 五、面试价值

1. **"硬件抽象层"** 直接命中 JD 第 3 条岗位职责
2. **接口设计** 展示 SOLID 原则中的依赖反转（DIP）——domain 定义接口，infrastructure 实现
3. **模拟/真实切换** 展示工程思维——"我在 WSL2 上开发用模拟传感器，部署时换一行 config 就切到真实硬件"
4. **mock 测试** 展示质量意识——"domain 层不依赖 ROS2，单测可以在 CI 上毫秒级跑，不需要启动 ROS2 进程"
