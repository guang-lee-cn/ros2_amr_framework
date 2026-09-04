"""NAV2 生产定位形态 — 预建地图 + map_server + AMCL（替代在线 SLAM）。

拓扑（对齐商用 AMR 三段式：部署期建图一次 → 运行期定位导航）：
  部署期: nav2_scene.launch（SLAM 模式）巡航建图 → map_saver_cli 存 maps/
  运行期: 本 launch —— map_server 加载预建地图，AMCL 扫描匹配定位
          （map→odom），NAV2 全局代价地图 = static_layer(预建) +
          obstacle_layer(动态) + inflation，安全闸照常收尾。

与 SLAM 开发态的差异：
  - 无 slam_toolbox：map→odom 由 AMCL 提供，走廊漂移消失（SLAM 弱区）
  - 全局代价地图含 static_layer：未扫描区域按预建图判定，不再靠
    allow_unknown 蛮干
  - 启动即有完整地图（冷启动可立即规划远程目标）
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_dir = get_package_share_directory("ros2_robot_middleware")
    nav2_params = os.path.join(pkg_dir, "config", "nav2_params_localized.yaml")
    map_yaml = os.path.join(pkg_dir, "maps", "rack_3c.yaml")
    use_sim_time = LaunchConfiguration("use_sim_time", default="false")

    scene = Node(
        package="ros2_robot_middleware", executable="scene_simulator",
        name="scene_simulator",
        parameters=[{"scene_name": "rack_3c",   # 场景需与建图时一致
                     "broadcast_map_tf": False,  # map→odom 归 AMCL
                     "use_sim_time": use_sim_time}],
        remappings=[("/scan", "/scan_raw")],
        output="screen")

    map_server = Node(
        package="nav2_map_server", executable="map_server",
        name="map_server",
        parameters=[nav2_params, {"yaml_filename": map_yaml}],
        output="screen")

    amcl = Node(
        package="nav2_amcl", executable="amcl",
        name="amcl",
        parameters=[nav2_params],
        output="screen")

    nav2 = {"parameters": [nav2_params, {"use_sim_time": use_sim_time}]}

    # 激活顺序: map_server → amcl → 导航四件套（static_layer 依赖活跃的
    # map_server，AMCL 依赖地图；单一 manager 按序拉起）
    lifecycle_manager = Node(
        package="nav2_lifecycle_manager", executable="lifecycle_manager",
        name="lifecycle_manager_navigation",
        parameters=[{"use_sim_time": use_sim_time,
                     "autostart": True,
                     "node_names": ["map_server", "amcl",
                                    "controller_server", "planner_server",
                                    "behavior_server", "bt_navigator"]}],
        output="screen")

    controller_server = Node(
        package="nav2_controller", executable="controller_server",
        name="controller_server", **nav2,
        remappings=[("/cmd_vel", "/cmd_vel_raw")],
        output="screen")

    planner_server = Node(
        package="nav2_planner", executable="planner_server",
        name="planner_server", **nav2, output="screen")

    behavior_server = Node(
        package="nav2_behaviors", executable="behavior_server",
        name="behavior_server", **nav2,
        remappings=[("/cmd_vel", "/cmd_vel_raw")],
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
        map_server, amcl,
        lifecycle_manager,
        controller_server, planner_server, behavior_server, bt_navigator,
        guard,
        foxglove,
    ])
