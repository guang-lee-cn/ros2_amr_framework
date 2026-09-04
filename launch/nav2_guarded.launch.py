"""NAV2 × 场景仿真 × 自研安全闸 — L3 分层共存生产形态。

在 nav2_scene 基础上把 NAV2 控制器输出经独立安全闸下发：
  controller_server: /cmd_vel → 重映射 /cmd_vel_raw
  cmd_vel_guard_node: /cmd_vel_raw + /scan_raw → 域 CollisionGuard → /cmd_vel
                     （近障减速/硬停、全盲 fail-safe、stale 硬停）
  scene_simulator:   /cmd_vel 执行

职责边界：闸只钳速度不否决目标（目标生命周期归 NAV2 行为树），
被拦超时由 NAV2 progress checker 接管——Side B 双头死循环的教训。
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
    scene_name = LaunchConfiguration("scene", default="rack_3c")
    random_boxes = LaunchConfiguration("random_boxes", default="0")
    movers = LaunchConfiguration("movers", default="0")
    mover_speed = LaunchConfiguration("mover_speed", default="0.6")

    scene = Node(
        package="ros2_robot_middleware", executable="scene_simulator",
        name="scene_simulator",
        parameters=[{"scene_name": scene_name,
                     "random_boxes": random_boxes,
                     "movers": movers,
                     "mover_speed": mover_speed,
                     "broadcast_map_tf": False,
                     "use_sim_time": use_sim_time}],
        remappings=[("/scan", "/scan_raw")],
        output="screen")

    slam = Node(
        package="slam_toolbox", executable="async_slam_toolbox_node",
        name="slam_toolbox",
        parameters=[{"use_sim_time": use_sim_time,
                     "base_frame": "amr/chassis",
                     "odom_frame": "amr/odom",
                     "scan_topic": "/scan_raw",
                     "mode": "mapping"}],
        output="screen")

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
        name="controller_server", **nav2,
        remappings=[("/cmd_vel", "/cmd_vel_raw")],  # 输出改道安全闸
        output="screen")

    planner_server = Node(
        package="nav2_planner", executable="planner_server",
        name="planner_server", **nav2, output="screen")

    behavior_server = Node(
        package="nav2_behaviors", executable="behavior_server",
        name="behavior_server", **nav2,
        remappings=[("/cmd_vel", "/cmd_vel_raw")],  # 恢复行为同闸
        output="screen")

    bt_navigator = Node(
        package="nav2_bt_navigator", executable="bt_navigator",
        name="bt_navigator", **nav2, output="screen")

    guard = Node(
        package="ros2_robot_middleware", executable="cmd_vel_guard_node",
        name="cmd_vel_guard",
        respawn=True, respawn_delay=2.0,   # ADR §5.1: 安全闸崩溃自动拉起
        parameters=[{"guard_stop_dist": 0.30,
                     "guard_safe_dist": 0.80,
                     "guard_min_valid_echoes": 50}],
        output="screen")

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
        guard,
        foxglove,
    ])
