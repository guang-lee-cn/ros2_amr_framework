# M11：真实硬件驱动接入开发指南

> 适用：接入新传感器（LiDAR/IMU/Camera）或新执行器（底盘/电机）。
> 目标：驱动开发不修改框架代码，1-3 天完成接入。

---

## 一、总览

```
┌─────────┐   ┌──────────────────┐   ┌─────────────┐   ┌────────────┐
│ 硬件     │   │ ISensor<T>        │   │ ROS2 消息    │   │ 业务节点    │
│ LiDAR   │──▶│ IActuator<C,F>    │──▶│ /scan       │──▶│ FusionNode  │
│ IMU     │   │ amr_hal/ 适配器   │   │ /cmd_vel    │   │ DecisionNode│
│ 电机    │   │ （我们写）         │   │ /odom       │   │ MotorCtrlNode
└─────────┘   └──────────────────┘   └─────────────┘   └────────────┘
  硬件层          HAL 抽象层            ROS2 传输层        业务逻辑层
  (不可改)       (驱动开发在此)         (ROS2 提供)        (框架已就绪)
```

**框架零改动原则**：FusionNode/DecisionNode/MotorCtrlNode 只依赖接口（ISensor/IActuator），不关心背后是模拟还是真实硬件。

---

## 二、阶段 0：硬件准备

### 输入确认

| 项 | 检查 | 示例 |
|------|------|------|
| 物理接口 | USB / UART / CAN / 以太网 | LiDAR 用 USB-TTL，底盘用 CAN |
| 数据协议 | 厂商协议文档 / 已有 SDK | SICK SOPAS、Velodyne VLP16 二进制 |
| 电气验证 | 手动读原始字节 | `cat /dev/ttyUSB0` 有输出，`candump can0` 有帧 |

### 验收标准

```bash
# 串口设备
ls -l /dev/ttyUSB0     # 存在且可读
cat /dev/ttyUSB0 | xxd | head   # 有原始字节流

# CAN 设备
candump can0           # 能抓到帧
```

### 常见问题

| 问题 | 解决 |
|------|------|
| 设备权限不足 | `sudo usermod -aG dialout $USER` + 重新登录 |
| UART 波特率不对 | 查厂商文档，`stty -F /dev/ttyUSB0 115200` |
| CAN 未配置 | `sudo ip link set can0 up type can bitrate 500000` |

---

## 三、阶段 1：驱动适配器开发

### 传感器（单向流入）

```cpp
// amr_hal/src/sensor/velodyne_adapter.hpp
class VelodyneAdapter : public amr::domain::sensor::SensorBase<VelodyneAdapter,
                          amr::domain::sensor::LidarScan> {
public:
  bool init_impl() override {
    fd_ = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);
    return fd_ >= 0;
  }

  bool read_impl(amr::domain::sensor::LidarScan &out) override {
    ssize_t n = read(fd_, buf_, sizeof(buf_));
    if (n <= 0) return false;
    // 厂商协议解析 → out.ranges[], out.range_count, out.angle_*
    return parse_protocol(buf_, n, out);
  }

  void shutdown_impl() override { if (fd_ >= 0) close(fd_); }

private:
  int fd_ = -1;
  uint8_t buf_[1500]{};
};
```

### 执行器（双向流）

```cpp
// amr_hal/src/actuator/diff_drive_adapter.hpp
struct WheelCmd { float vx, vy, wz; };            // cmd_vel
struct WheelFeedback { float left_rps, right_rps; };  // 编码器

class DiffDriveAdapter : public amr::domain::execution::ActuatorBase<DiffDriveAdapter,
                           WheelCmd, WheelFeedback> {
public:
  bool init_impl() override { return can_open(); }

  bool write_impl(const WheelCmd &cmd) override {
    return can_send_frame(frame_from(cmd));       // 下发速度
  }

  bool read_impl(WheelFeedback &fb) override {
    return can_recv_frame(&fb);                    // 读编码器
  }
};
```

### 注册（一行宏）

```cpp
// 框架通过注册表获取适配器，不直接引用类名
REGISTER_SENSOR(lidar, velodyne, VelodyneAdapter);
REGISTER_ACTUATOR(drive, diff_drive, DiffDriveAdapter);
```

---

## 四、阶段 2：单元测试（不启动 ROS2）

### 策略：测协议解析，不测真实硬件

```cpp
// amr_hal/quality/src/test_velodyne.cpp
TEST(VelodyneTest, Parse_ValidFrame_ReturnsScan) {
  VelodyneAdapter adapter;
  // 构造一段厂商协议格式的 bytes
  std::vector<uint8_t> frame = { 0xA5, 0x5A, ... };
  LidarScan scan;
  EXPECT_TRUE(adapter.parse_for_test(frame, scan));   // 暴露 parse 为测试接口
  EXPECT_EQ(scan.range_count, 360);
}

TEST(VelodyneTest, Parse_TruncatedFrame_ReturnsFalse) {
  // 半帧数据 → 应返回 false，不崩溃
}
```

### 测试要点

| 测什么 | 怎么测 |
|------|------|
| 协议解析正确性 | 构造合法/非法字节流，喂给 parse |
| 超时/断连 | `read_impl` 返回 false → 降级逻辑触发 |
| 线程安全 | 多线程并发 read，无数据竞争 |
| 无真实硬件 | CI 中测试不依赖 /dev/ttyUSB0 |

---

## 五、阶段 3：模拟→真实切换（YAML 一行）

```yaml
# config/sensors.yaml
sensors:
  lidar:
    type: velodyne      # 原来是 simulated
    topic: /velodyne/points
```

框架代码零改动。FusionNode 通过注册表拿到 VelodyneAdapter，和之前拿 SimulatedLidar 完全一样。

### 执行器切换

```yaml
# config/actuators.yaml
actuators:
  drive:
    type: diff_drive    # 原来是 simulated
    can_iface: can0
```

---

## 六、阶段 4：集成验证（真实硬件在环）

### 传感器验证

```bash
# 1. 启动系统
ros2 launch ros2_robot_middleware system.launch.py

# 2. 确认数据流通
ros2 topic echo /perception/objects --once   # 出物体列表
ros2 topic echo /sensor/lidar/heartbeat --once

# 3. 确认速率达标
curl -s localhost:9090/metrics | grep amr_sensor_rate_hz

# 4. 降级测试
#    拔掉 LiDAR → 观察降级到 NO_LIDAR
#    插回 LiDAR → 观察自动恢复 FULL
ros2 topic echo /health/report --once
```

### 执行器验证

```bash
# 1. 发指令
ros2 action send_goal /cmd/move_to_pose ros2_robot_middleware/action/MoveToPose \
  "{target_x: 1.0, target_y: 0.0}"

# 2. 确认 odom 反馈
ros2 topic echo /odom

# 3. 闭环确认
#    cmd_vel 有输出 + odom 在变化 + 机器人实际移动
```

### 验收标准

| 场景 | 通过标准 |
|------|---------|
| 正常数据 | 感知结果与手动 topic echo 一致 |
| 速率 | 达到传感器标称频率（LiDAR 10Hz） |
| 断连恢复 | 拔线 → 降级 → 插回 → 自动恢复 |
| 连续运行 | 30 分钟无崩溃、无内存泄漏（ASan 跑一遍） |

---

## 七、驱动开发三种模式选择

| 模式 | 适用场景 | 工作量 | 说明 |
|:---:|------|:---:|------|
| **直接写适配器** | 厂商 SDK 简单、串口/UART 直连 | 1-2 天 | 自己实现 ISensor/IActuator |
| **包装厂商 ROS2 包** | 已有 ROS2 驱动（sick_scan2 等） | 0.5 天 | 封装 `ISensor` 调已有包，订阅其 topic |
| **适配 ros2_control** | 电机/关节（未来多底盘） | 1-3 天 | 写 SystemInterface plugin，IActuator 桥接 |

---

## 八、模拟/真实无缝切换矩阵

| 环境 | LiDAR | IMU | Camera | 底盘 |
|:---:|:---:|:---:|:---:|:---:|
| 单元测试 | SimulatedLidar | SimulatedImu | SimulatedCamera | SimulatedDrive |
| Gazebo 仿真 | ros_gz_bridge → ISensor | 同上 | 同上 | gz→cmd_vel |
| 真机 demo | SickTiM781Adapter | BMI088Adapter | RealSenseAdapter | DiffDriveAdapter |

切换 = 改 YAML 一行，代码零改动。

---

## 九、参考

- [HAL 设计](09-hal-design.md)
- [ISensor 接口](../../include/ros2_robot_middleware/domain/perception/sensor_interface.hpp)
- [IActuator 接口](../../include/ros2_robot_middleware/domain/execution/iactuator.hpp)
- [ROS2 驱动接入官方指南](https://docs.ros.org/en/jazzy/Tutorials/Developing-a-ROS-2-Driver.html)
