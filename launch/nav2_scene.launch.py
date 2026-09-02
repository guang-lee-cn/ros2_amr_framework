"""NAV2 × 场景仿真器 — 纯 CPU 导航演示（无 Gazebo / 无 GPU 渲染）。

背景：gz-sim gpu_lidar 的渲染线程在本平台静默死亡（头文件 simulated_scene.hpp
有案底），Gazebo 路线只适合短演示。本 launch 用项目自带的纯 CPU 射线投射
场景仿真器（rack_3c：4 排料架 + 2 机台）喂 NAV2 全栈：
  scene_simulator: /cmd_vel 进 → /odom /scan_raw TF markers 出（20Hz）
  slam_toolbox:    在线建图（map→odom）
  NAV2 四件套:     NavFn 全局规划 + DWB 控制 + 行为树 + 恢复行为
  foxglove_bridge: ws://localhost:8765

注意：场景仿真器走墙钟（无 /clock）→ 全栈 use_sim_time=false。
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_dir = get_package_share_directory("ros2_robot_middleware")
    nav2_params = os.path.join(pkg_dir, "config", "nav2_params.yaml")
    use_sim_time = LaunchConfiguration("use_sim_time", default="false")

    # ── 1. 场景仿真器（纯 CPU 射线投射，替代 Gazebo+bridge+robot_state_pub）──
    scene = Node(
        package="ros2_robot_middleware", executable="scene_simulator",
        name="scene_simulator",
        parameters=[{"scene_name": "rack_3c",
                     "broadcast_map_tf": False,   # map→odom 让位给 slam_toolbox
                     "use_sim_time": use_sim_time}],
        remappings=[("/scan", "/scan_raw")],      # NAV2 参数表期望 /scan_raw
        output="screen")

    # ── 2. SLAM 在线建图 ─────────────────────────────────────────────
    slam = Node(
        package="slam_toolbox", executable="async_slam_toolbox_node",
        name="slam_toolbox",
        parameters=[{"use_sim_time": use_sim_time,
                     "base_frame": "amr/chassis",
                     "odom_frame": "amr/odom",
                     "scan_topic": "/scan_raw",
                     "mode": "mapping"}],
        output="screen")

    # ── 3. NAV2（双 lifecycle manager，slam 无 bond 需独立管理）────────
    nav2 = {"parameters": [nav2_params, {"use_sim_time": use_sim_time}]}

    lifecycle_manager_slam = Node(
        package="nav2_lifecycle_manager", executable="lifecycle_manager",
        name="lifecycle_manager_slam",
        parameters=[{"use_sim_time": use_sim_time,
                     "autostart": True,
                     "bond_timeout": 0.0,
                     "node_names": ["slam_toolbox"]}],
        output="screen")

    lifecycle_manager = Node(
        package="nav2_lifecycle_manager", executable="lifecycle_manager",
        name="lifecycle_manager_navigation",
        parameters=[{"use_sim_time": use_sim_time,
                     "autostart": True,
                     "node_names": ["controller_server", "planner_server",
                                    "behavior_server", "bt_navigator"]}],
        output="screen")

    controller_server = Node(
        package="nav2_controller", executable="controller_server",
        name="controller_server", **nav2, output="screen")

    planner_server = Node(
        package="nav2_planner", executable="planner_server",
        name="planner_server", **nav2, output="screen")

    behavior_server = Node(
        package="nav2_behaviors", executable="behavior_server",
        name="behavior_server", **nav2, output="screen")

    bt_navigator = Node(
        package="nav2_bt_navigator", executable="bt_navigator",
        name="bt_navigator", **nav2, output="screen")

    foxglove = Node(
        package="foxglove_bridge", executable="foxglove_bridge",
        parameters=[{"port": 8765}], output="screen")

    return LaunchDescription([
        ExecuteProcess(cmd=["rm", "-f", "/dev/shm/amr_metrics_registry"]),
        scene,
        slam,
        lifecycle_manager_slam,
        lifecycle_manager,
        controller_server, planner_server, behavior_server, bt_navigator,
        foxglove,
    ])
