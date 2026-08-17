"""场景模拟演示 — SceneSimulator 驱动计算容器（面试演示用，无物理仿真）。

替代 Gazebo：gz-sim gpu_lidar 在本平台有引擎 bug（车一动渲染失效，
world_pose 不随车更新，见架构文档 §7）。SceneSimulator 发布合成
/scan /odom /amcl_pose /tf，计算容器完整跑通 感知→决策→执行 闭环。

演示流程：
  车在原点 (0,0)，决策目标 (10,0)（box 障碍 (8,0) 之后）
  → A* 从 AMCL 位姿规划绕障路径 → motor PurePursuit 跟踪 + 护栏 + 平滑
  → SceneSimulator 积分 /cmd_vel 更新车 → 闭环自主导航到目标

可视化：Foxglove 连接 ws://localhost:8765，3D 面板看车 + scan + 路径。
robot_state_publisher 读 amr_visual.urdf 发布 /robot_description + joint TF
（fixed joint 无需 /joint_states）→ Foxglove 从 /robot_description 渲染小车。
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_dir = get_package_share_directory("ros2_robot_middleware")
    scene_name = LaunchConfiguration("scene")
    declare_scene = DeclareLaunchArgument(
        "scene", default_value="rack_3c",
        description="SimulatedScene preset: rack_4box | rack_3c | warehouse_open")
    urdf_path = os.path.join(pkg_dir, "worlds", "amr_visual.urdf")

    # scene 是 map→amr/odom 的唯一 TF 发布者，误杀即整棵子树坍塌 →
    # Foxglove 报"X 到 map 变换缺失"。respawn 让它崩/被杀后自动重启（B1）。
    scene = Node(
        package="ros2_robot_middleware",
        executable="scene_simulator",
        output="screen",
        respawn=True,
        respawn_delay=2.0,
    )

    # 发布 /robot_description + fixed-joint TF → Foxglove 渲染小车模型
    robot_state_publisher = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        output="screen",
        parameters=[{"robot_description": open(urdf_path).read()}],
    )

    compute = Node(
        package="ros2_robot_middleware",
        executable="compute_container",
        namespace="",
        output="screen",
        parameters=[{
            "sensors.lidar.type": "sick_tim781",
            "sensors.lidar.topic": "/scan",
            "goal_x": 17.0,
            "goal_y": 4.0,
            "vfh_enabled": False,
            "scene_name": scene_name,
        }],
    )

    # 循环 goal（machine1→machine2→home 往返），订阅 /amcl_pose 到达切 goal
    patrol = Node(
        package="ros2_robot_middleware",
        executable="patrol_3c",
        output="screen",
    )

    return LaunchDescription([scene, robot_state_publisher, compute, patrol])
