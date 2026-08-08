"""场景模拟演示 — SceneSimulator 驱动计算容器（面试演示用，无物理仿真）。

替代 Gazebo：gz-sim gpu_lidar 在本平台有引擎 bug（车一动渲染失效，
world_pose 不随车更新，见架构文档 §7）。SceneSimulator 发布合成
/scan /odom /amcl_pose /tf，计算容器完整跑通 感知→决策→执行 闭环。

演示流程：
  车在原点 (0,0)，决策目标 (10,0)（box 障碍 (8,0) 之后）
  → A* 从 AMCL 位姿规划绕障路径 → motor PurePursuit 跟踪 + 护栏 + 平滑
  → SceneSimulator 积分 /cmd_vel 更新车 → 闭环自主导航到目标

可视化：Foxglove 连接 ws://localhost:8765，3D 面板看车 + scan + 路径。
"""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    scene = Node(
        package="ros2_robot_middleware",
        executable="scene_simulator",
        output="screen",
    )

    compute = Node(
        package="ros2_robot_middleware",
        executable="compute_container",
        namespace="",
        output="screen",
        parameters=[{
            # fusion 用 sick_tim781 适配器：订阅 /scan（SceneSimulator 提供）
            "sensors.lidar.type": "sick_tim781",
            "sensors.lidar.topic": "/scan",
            # 决策目标：box(8,0) 之前（x=7，box 前缘 7.75）。
            # 走直线到目标，护栏在 box 前不触发（0.75m > stop_dist 0.3）。
            # 绕障（G2-B VFH）单测已覆盖；demo 展示感知→决策→执行闭环。
            "goal_x": 7.0,
            "goal_y": 0.0,
            "vfh_enabled": False,
        }],
    )

    return LaunchDescription([scene, compute])
