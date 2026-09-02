"""NAV2 快速验证 — Gazebo 仿真 + NAV2 导航栈（替代自研导航）。

保留：Gazebo 物理 + 激光 + 差速底盘（/odom /cmd_vel /scan_raw）
替换：fusion → decision → motor_ctrl 全部换成 NAV2 全家桶
     SLAM 在线建图 + Planner + Controller + Costmap 分层 + 恢复行为
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess, IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_dir = get_package_share_directory("ros2_robot_middleware")
    world_path = os.path.join(pkg_dir, "worlds", "factory_3c.sdf")
    nav2_params = os.path.join(pkg_dir, "config", "nav2_params.yaml")
    use_sim_time = LaunchConfiguration("use_sim_time", default="true")

    # ── 1. Gazebo ───────────────────────────────────────────────────────
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            os.path.join(get_package_share_directory("ros_gz_sim"),
                         "launch", "gz_sim.launch.py")]),
        launch_arguments={"gz_args": f"-s -r --headless-rendering -v4 {world_path}"}.items())

    spawn_amr = Node(
        package="ros_gz_sim", executable="create",
        arguments=["-name", "amr", "-file",
                   os.path.join(pkg_dir, "worlds", "amr.sdf"), "-x", "1", "-y", "0", "-z", "0"],
        output="screen")

    bridge = Node(
        package="ros_gz_bridge", executable="parameter_bridge",
        parameters=[{"config_file": os.path.join(pkg_dir, "config", "gz_bridge.yaml"),
                     "use_sim_time": use_sim_time}],
        output="screen")

    # ── 2. 机器人描述 + TF ─────────────────────────────────────────────
    with open(os.path.join(pkg_dir, "urdf", "amr_nav2.urdf"), "r") as f:
        robot_description = f.read()

    robot_state = Node(
        package="robot_state_publisher", executable="robot_state_publisher",
        parameters=[{"robot_description": robot_description, "use_sim_time": use_sim_time}],
        output="screen")

    # odom→chassis TF 由 Gazebo DiffDrive <tf_topic> 经桥接发布（gz_bridge.yaml /tf）
    # chassis→lidar TF 由 robot_state_publisher 从 URDF 发布，无需静态 TF

    # ── 3. SLAM 在线建图（替代预建地图 + AMCL）──────────────────────────
    slam = Node(
        package="slam_toolbox", executable="async_slam_toolbox_node",
        name="slam_toolbox",
        parameters=[{"use_sim_time": use_sim_time,
                     "base_frame": "amr/chassis",
                     "odom_frame": "amr/odom",
                     "scan_topic": "/scan_raw",
                     "mode": "mapping"}],
        output="screen")

    # ── 4. NAV2 导航栈 ─────────────────────────────────────────────────
    nav2 = {"parameters": [nav2_params, {"use_sim_time": use_sim_time}]}

    # slam_toolbox 非 nav2 系生命周期节点，不发 bond 心跳——
    # 必须用独立 manager 且关掉 bond 校验，否则 4s 超时连带放弃后续节点激活
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

    # ── 5. Foxglove ────────────────────────────────────────────────────
    foxglove = Node(
        package="foxglove_bridge", executable="foxglove_bridge",
        parameters=[{"port": 8765}], output="screen")

    return LaunchDescription([
        ExecuteProcess(cmd=["rm", "-f", "/dev/shm/amr_metrics_registry"]),
        gazebo,
        TimerAction(period=5.0, actions=[spawn_amr]),
        TimerAction(period=8.0, actions=[
            bridge, robot_state, slam,
        ]),
        TimerAction(period=12.0, actions=[
            lifecycle_manager_slam,
            lifecycle_manager,
            controller_server, planner_server, behavior_server, bt_navigator,
        ]),
        TimerAction(period=15.0, actions=[foxglove]),
    ])
