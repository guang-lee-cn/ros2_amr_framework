# TF2 坐标变换集成 — 技术方案

> 状态：设计中

## 一、问题

当前所有传感器输出都在各自的坐标系下（`lidar_frame`、`imu_link`、`camera_link`），没有统一到机器人本体坐标系（`base_link`）。DBSCAN 聚类和 KF 跟踪在错误的坐标系下进行——AMR 移动时，历史状态和当前观测之间存在坐标系偏差。

```
当前:
  LiDAR → lidar_frame (传感器自身)
  IMU   → imu_link
  Fusion 在 lidar_frame 下运算

目标:
  LiDAR → base_link (机器人中心)
  IMU   → base_link
  Fusion 在 base_link 下运算
```

## 二、技术选型

### 唯一候选：tf2（ROS2 标准库）

| 维度 | tf2 (ROS2 官方) |
|------|------|
| 许可证 | BSD-3（无限制） |
| 维护方 | Open Robotics / ROS2 核心团队 |
| ROS2 集成 | `apt install ros-jazzy-tf2-*` 即可 |
| 性能 | `BufferCore` 内部用哈希表 O(1) 查找 |
| 生态 | 所有 ROS2 传感器驱动都发布 TF，100% 兼容 |

**不存在闭源方案。** tf2 是 ROS2 生态中坐标变换的唯一标准。SLAM、导航、感知全部依赖它。

## 三、DDD 分层设计

tf2 是 ROS2 组件——不允许进入 domain 层。按 DDD 规则，通过接口注入：

```
domain/
  └── transform_provider.hpp      ← ITransformProvider 纯虚接口
         virtual bool transform(const LidarScan& in, LidarScan& out,
                                const std::string& target_frame) = 0;

infrastructure/
  └── tf2_transform_provider.hpp  ← Tf2TransformProvider : ITransformProvider
         内部持有 tf2_ros::Buffer，读 /tf_static

application/
  └── PerceptionService           ← 通过 ITransformProvider& 注入，tick() 中调 transform()
```

### 为什么不用 tf2 的 `doTransform` 直接操作 sensor_msgs

`LidarScan` 是我们自己定义的数据类型。tf2 不知道如何变换它。有两个选择：

**A) 在 `tick()` 中先拿到 `sensor_msgs/LaserScan`，调 tf2 变换，再转成 `LidarScan`**
- 优点：复用 tf2 的 LaserScan 变换逻辑
- 缺点：FusionNode 需要同时持有 `sensor_msgs` 和 `LidarScan` 两份数据

**B) 自己写 `transform(LidarScan, "lidar_frame"→"base_link")`**
- 优点：domain 数据类型统一，不依赖 sensor_msgs
- 缺点：手动实现坐标变换（旋转+平移），~20 行代码

**选择 B。** 静态 TF 的变换是刚体变换（旋转矩阵+平移向量），20 行代码不值得引入 sensor_msgs 依赖。

## 四、静态 TF 配置文件

```yaml
# config/sensors.yaml 中追加
tf:
  lidar:
    parent: base_link
    x: 0.15, y: 0.0, z: 0.3     # LiDAR 在机器人前方 15cm，高 30cm
    roll: 0.0, pitch: 0.0, yaw: 0.0
  imu:
    parent: base_link
    x: 0.0, y: 0.0, z: 0.1      # IMU 在机器人中心，高 10cm
  camera:
    parent: base_link
    x: 0.15, y: 0.0, z: 0.5
```

这些是静态 TF——机器人装配时标定一次，运行时不变化，发布到 `/tf_static`。

## 五、数据流变化

```
当前:
  LidarScan(lidar_frame) → DBSCAN → Clusters(lidar_frame) → KF → Tracker

目标:
  LidarScan(lidar_frame) → ITransformProvider::transform → LidarScan(base_link)
    → DBSCAN → Clusters(base_link) → KF → Tracker → TrackedObjects(base_link)
```

只有 LiDAR 需要变换（IMU 返回加速度，不涉及位置变换；Camera 当前未参与融合）。

## 六、接口设计

```cpp
// domain/transform_provider.hpp
class ITransformProvider {
public:
    virtual ~ITransformProvider() = default;

    /// Transform a LiDAR scan from its native frame to target_frame.
    /// Returns true if transform succeeded.
    virtual bool transform_scan(
        const LidarScan &in, LidarScan &out,
        const std::string &target_frame) = 0;
};
```

## 七、性能影响

- 360 个点 × 旋转+平移 = ~360 × 6 次浮点运算 ≈ 2160 FLOP，每 200ms 一次
- tf2 `Buffer::lookupTransform` 是 O(1) 哈希查找
- 总开销 < 10μs，在 200ms tick 中可忽略

## 八、是否遵循 DDD

| 层 | 依赖 |
|---|------|
| `domain/transform_provider.hpp` | 零依赖（纯虚接口） |
| `infrastructure/tf2_transform_provider.hpp` | `tf2_ros` + `tf2_geometry_msgs` |
| `application/PerceptionService` | `ITransformProvider&`（注入） |

domain 层不依赖 tf2。编译期可验证。

## 九、实施计划

| 步骤 | 内容 | 预估 |
|:---:|------|:---:|
| 1 | `domain/transform_provider.hpp` — ITransformProvider 接口 | 15min |
| 2 | `infrastructure/tf2_transform_provider.hpp` — tf2 实现 | 30min |
| 3 | `config/sensors.yaml` — 追加 TF 配置 | 5min |
| 4 | `PerceptionService` — 注入 ITransformProvider，tick() 中变换 | 15min |
| 5 | `FusionNode` — 创建 Tf2TransformProvider，注入 | 10min |
| 6 | 测试 + 验证 | 15min |
