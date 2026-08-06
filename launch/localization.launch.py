"""Gazebo 定位导航 — map_server + AMCL + compute_container（G1b/G1c）

已知地图导航：map_server 加载建图产物 → AMCL 定位（map→odom TF）→
compute_container 的 decision 用 map 坐标系位姿规划（G1c）。

TF 树：map → amr/odom → amr/chassis → amr/chassis/lidar
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node as RosNode
from launch_ros.actions import LifecycleNode


def generate_launch_description():
    pkg_dir = get_package_share_directory("ros2_robot_middleware")
    world_path = os.path.join(pkg_dir, "worlds", "warehouse.sdf")
    amr_path = os.path.join(pkg_dir, "worlds", "amr.sdf")
    map_yaml = os.path.join(pkg_dir, "config", "maps", "amr_map.yaml")
    amcl_params = os.path.join(pkg_dir, "config", "amcl.yaml")

    declare_use_sim_time = DeclareLaunchArgument(
        "use_sim_time", default_value="true", description="Use sim time")
    use_sim_time = LaunchConfiguration("use_sim_time")

    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            os.path.join(get_package_share_directory("ros_gz_sim"),
                         "launch", "gz_sim.launch.py"),
        ]),
        launch_arguments={"gz_args": f"-r {world_path}"}.items(),
    )

    spawn_amr = RosNode(
        package="ros_gz_sim", executable="create",
        arguments=["-name", "amr", "-file", amr_path,
                   "-x", "0", "-y", "0", "-z", "0.15"],
        output="screen",
    )

    bridges = [
        ("bridge_clock", "/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock", None),
        ("bridge_lidar", "/lidar@sensor_msgs/msg/LaserScan[gz.msgs.LaserScan",
         [("/lidar", "/scan")]),
        ("bridge_tf", "/model/amr/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V",
         [("/model/amr/tf", "/tf")]),
        ("bridge_odom", "/model/amr/odometry@nav_msgs/msg/Odometry[gz.msgs.Odometry",
         [("/model/amr/odometry", "/odom")]),
        ("bridge_cmd_vel", "/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist", None),
    ]
    bridge_nodes = [
        RosNode(package="ros_gz_bridge", executable="parameter_bridge",
                name=name, arguments=[arg],
                remappings=remap, output="screen")
        for name, arg, remap in bridges
    ]

    static_tf_lidar = RosNode(
        package="tf2_ros", executable="static_transform_publisher",
        parameters=[{"use_sim_time": use_sim_time}],
        arguments=["0.25", "0", "0.05", "0", "0", "0",
                   "amr/chassis", "amr/chassis/lidar"],
        output="screen",
    )

    # ── 已知地图导航 ────────────────────────────────────────────────
    map_server = LifecycleNode(
        package="nav2_map_server", executable="map_server",
        name="map_server", namespace="",
        parameters=[{"yaml_filename": map_yaml, "use_sim_time": use_sim_time}],
        output="screen",
    )

    amcl = LifecycleNode(
        package="nav2_amcl", executable="amcl",
        name="amcl", namespace="",
        parameters=[amcl_params],
        output="screen",
    )

    # lifecycle_manager 等待节点 ready 后统一 configure/activate（ChangeState
    # 事件时序不稳，节点未启动时事件丢失，amcl 停在 unconfigured）
    lifecycle_manager = RosNode(
        package="nav2_lifecycle_manager", executable="lifecycle_manager",
        name="lifecycle_manager",
        parameters=[{
            "use_sim_time": use_sim_time,
            "autostart": True,
            "node_names": ["map_server", "amcl"],
            "bond_timeout": 0.0,
        }],
        output="screen",
    )

    compute = RosNode(
        package="ros2_robot_middleware", executable="compute_container",
        output="screen",
        parameters=[{
            "use_sim_time": use_sim_time,
            "sensors.lidar.type": "sick_tim781",
            "sensors.lidar.topic": "/scan",
            # G1c: 任务点用 map 坐标系（world (2.0,0) → map (10.16,9.85)）
            "goal_x": 10.16,
            "goal_y": 9.85,
        }],
    )

    return LaunchDescription([
        declare_use_sim_time,
        gazebo,
        spawn_amr,
        *bridge_nodes,
        static_tf_lidar,
        map_server, amcl,
        lifecycle_manager,
        compute,
    ])
