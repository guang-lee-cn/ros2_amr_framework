# 手动建图操作指南（teleop 遥控）

> 记录日期：2026-08-06
> 用途：学习用。真实商用 AMR 建图通常由人工遥控车巡图（闭环/手控），
> 而非开环速度-时间盲走（会漂移跑飞）。

## 一、为什么手动建图

开环速度-时间巡图（发固定 cmd_vel 走固定时长）在真实物理（摩擦/惯性）下不可靠——
车会漂移、转向累计误差、甚至跑飞。商用做法是**人工遥控**或**闭环反馈控制**。

两种可靠方式：
1. **手动 teleop**（本指南）：人用键盘遥控车走遍场地，LiDAR 建图
2. **闭环巡图脚本**（本项目已实现）：`scripts/patrol_closedloop.py`，订阅 /odom 反馈控制

## 二、teleop 手动建图步骤

```bash
# 终端 1：启动建图环境（Gazebo + 桥 + slam_toolbox）
source /opt/ros/jazzy/setup.bash
source /home/guang/code/ros2_ws/install/setup.bash
ros2 launch ros2_robot_middleware mapping.launch.py

# 终端 2：启动键盘遥控
source /opt/ros/jazzy/setup.bash
source /home/guang/code/ros2_ws/install/setup.bash
ros2 run teleop_twist_keyboard teleop_twist_keyboard
```

## 三、键盘控制键位

| 键 | 动作 |
|---|---|
| `i` | 前进 |
| `k` | 后退 |
| `j` | 左转 |
| `l` | 右转 |
| `u` / `o` | 斜向前进 |
| `m` / `.` | 斜向后退 |
| `空格` | 急停 |
| `q` / `z` | 加速 / 减速 |

## 四、巡图路径建议（warehouse 场景）

warehouse 布局：
- 第一堵墙 **x=3**（横跨 y∈[-2,2]）
- 第二堵墙 **x=6**
- 货架 (4.5, ±1.5)

车起点 (0,0)，只能在 **x<3 区域**活动（墙前）。建议蛇形覆盖：
- `i` 前进几秒 → `l`/`j` 转向 → 再走，在 x∈[0.5,2.5], y∈[-1.5,1.5] 之间往返
- 让 LiDAR（10m 量程，360°）扫到所有墙/货架

## 五、保存地图

```bash
ros2 run nav2_map_server map_saver_cli -f ~/amr_map
```

产物：`amr_map.pgm`（栅格图）+ `amr_map.yaml`（参数），供 AMCL 定位用。

## 六、踩坑记录

1. **开环控制会漂移**：车会跑飞（曾漂移到 (72, 50)），必须闭环/遥控
2. **进程残留**：多套 gz/bridge 残留会导致数据混乱、车位置跳变。
   清理注意 `comm` 字段截断（parameter_bridge→parameter_bridg）和 gz 进程名是 `sim`/`ruby`。
3. **静态 TF 要 use_sim_time**：否则 TF 时间跳变，slam 丢 scan
4. **LiDAR 外参 TF 必需**：scan frame（amr/chassis/lidar）需连到 base
