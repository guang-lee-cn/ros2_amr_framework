# 接口契约（Contracts）

> 子系统间通信的"法律文件"，strategy.md §5 产出。
> 下游 `operations/cpp.md` 子系统实现以此为唯一事实来源。

## 话题契约

| 话题 | 类型 | 方向 | 生产者 | 消费者 | 兼容性 |
|------|------|------|--------|--------|--------|
| `/scan` | sensor_msgs/LaserScan | gz→ros | 桥接 | fusion_node | **保持（必同型）** |
| `/points` | sensor_msgs/PointCloud2 | gz→ros | 桥接 | Foxglove / 感知(可选) | 新增 |
| `/imu` | sensor_msgs/Imu | gz→ros | 桥接 | fusion/定位 | **保持** |
| `/image` | sensor_msgs/Image | gz→ros | 桥接 | Foxglove(可选) | **保持** |
| `/cmd_vel` | geometry_msgs/Twist | ros→gz | motor_ctrl | 桥接→gz DiffDrive | **新增方向** |
| `/clock` | rosgraph_msgs/Clock | gz→ros | 桥接 | use_sim_time 节点 | 保持 |
| `/health/report` | 自定义 HealthReport | ros | health_monitor | 观测 | 保持 |
| `/health/check` | 自定义 SetParam srv | ros | 调用方 | health_monitor | 保持 |
| `/joint_states` | sensor_msgs/JointState | gz→ros | 桥接 | 观测(可选) | 新增(可选) |

## 版本管理策略

1. **向后兼容为第一原则**：`/scan` `/imu` `/image` 改名或改型 = 破坏性变更，须走版本评审。
2. **Additive 无压力**：新话题（`/points` `/joint_states`）直接新增，不影响既有消费方。
3. **msg/srv 版本化**：接口定义变更记录进 CHANGELOG。
4. **契约变更流程**：cpp.md 实现中发现契约不可实现/歧义 → 暂停，输出问题描述 + 修订建议，由主 agent 修订契约后重新下发；子 agent 不得自行修改契约。

## 实现层物理约束（正向传递）

- 单个接口类公有方法 ≤ 9，成员变量 ≤ 5；超限在契约阶段拆正交子接口。
- sensor_adapter 转换函数：**无状态**（SICK 原始 → LaserScan 纯函数），不引入接口类。

## 文件索引

| 文件 | 内容 |
|------|------|
| [bridge-contract.md](bridge-contract.md) | gz↔ros2 桥接映射（与仿真层的法律文件） |
