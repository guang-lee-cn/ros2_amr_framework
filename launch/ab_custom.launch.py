"""A/B 对照 — 自研栈形态（无 patrol 层，目标由 ab_runner 注入 /goal_pose）。

与 nav2_scene.launch.py 同场景（rack_3c）同传感器（/scan+/odom+/cmd_vel），
跑相同目标序列，指标对齐：每站耗时/成败/激光最小距离/里程。
自研栈保持 scene_demo 的交付配置（vfh_enabled=False、static_obstacles 默认）。
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    pkg_dir = get_package_share_directory("ros2_robot_middleware")
    urdf_path = os.path.join(pkg_dir, "urdf", "amr_nav2.urdf")
    scene_name = "rack_3c"

    scene = Node(
        package="ros2_robot_middleware", executable="scene_simulator",
        output="screen", respawn=True, respawn_delay=2.0,
        parameters=[{"scene_name": scene_name}])  # broadcast_map_tf 默认 true（无 SLAM）

    robot_state_publisher = Node(
        package="robot_state_publisher", executable="robot_state_publisher",
        output="screen",
        parameters=[{"robot_description": open(urdf_path).read()}])

    compute = Node(
        package="ros2_robot_middleware", executable="compute_container",
        namespace="", output="screen",
        parameters=[{
            "sensors.lidar.type": "sick_tim781",
            "sensors.lidar.topic": "/scan",
            "goal_x": 17.0, "goal_y": 4.0,   # 初始 goal，ab_runner 随后经 /goal_pose 覆盖
            "vfh_enabled": False,
            "scene_name": scene_name,
        }])

    foxglove = Node(
        package="foxglove_bridge", executable="foxglove_bridge",
        parameters=[{"port": 8765}], output="screen")

    return LaunchDescription([scene, robot_state_publisher, compute, foxglove])
