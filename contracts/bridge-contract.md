# 桥接契约（ros_gz_bridge 映射）

> 与仿真层（sim_bridge 子系统）的"法律文件"。D2 决策（仿真重建）后此映射即生效。
> 话题名以 `{world}` 为参数，实际按 world 名替换。

## gz → ros 方向（传感器数据）

```
[gz] /world/{world}/model/amr/link/chassis/sensor/lidar/scan
     → [ros] /scan      (sensor_msgs/LaserScan)
[gz] /world/{world}/model/amr/link/chassis/sensor/lidar/points
     → [ros] /points    (sensor_msgs/PointCloud2)   ← 新增（D4：gz ray 直出点云）
[gz] /world/{world}/model/amr/link/chassis/sensor/imu/imu
     → [ros] /imu       (sensor_msgs/Imu)
[gz] /world/{world}/model/amr/link/chassis/sensor/camera/image
     → [ros] /image     (sensor_msgs/Image)
[gz] /clock
     → [ros] /clock     (rosgraph_msgs/Clock)
```

## ros → gz 方向（控制指令）

```
[ros] /cmd_vel          (geometry_msgs/Twist)
     → [gz] /cmd_vel    ← 新增方向，补上闭环最后一跳（D1 根因 1 修复）
```

## 约束

1. `/scan` `/imu` `/image` 类型不得变更（向后兼容铁律）。
2. `/cmd_vel` 桥接为**双向配置**中新增的 ros→gz 半边；若 gz DiffDrive 插件已订阅同名 gz 话题，则此映射为纯转发。
3. `/points` 与 `/scan` 来自同一 ray 传感器：samples/range 配置保持一致，保证感知结果可比。
4. 桥接节点实例数：clock、scan、points、imu、image、cmd_vel 各一个 parameter_bridge（或合并）。

## 验证方法

```
ros2 topic hz /scan          # 10Hz（sdf update_rate）
ros2 topic hz /points
ros2 topic pub -1 /cmd_vel geometry_msgs/Twist "{linear: {x: 0.2}}"
# 观测 Gazebo 中车开始运动 → 闭环最后一跳打通
```
