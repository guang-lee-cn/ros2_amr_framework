"""Gazebo Harmonic 仿真启动 — AMR 机器人 + 传感器桥梁 + 业务节点

架构：
  Gazebo Sensors → ros_gz_bridge → ROS2 Topics → 我们的业务节点
                    (gz → ros2)     (/sensor/*)

启动后检查：
  ros2 topic list | grep sensor     # 确认传感器 topic 存在
  ros2 topic echo /sensor/lidar     # 确认 LiDAR 数据流通
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import LifecycleNode


def generate_launch_description():
    pkg_dir = get_package_share_directory("ros2_robot_middleware")
    world_path = os.path.join(pkg_dir, "worlds", "warehouse.sdf")
    amr_path = os.path.join(pkg_dir, "worlds", "amr.sdf")

    use_sim_time = LaunchConfiguration("use_sim_time", default="true")

    # ── 仿真时钟参数声明 ──────────────────────────────────────────────
    declare_use_sim_time = DeclareLaunchArgument(
        "use_sim_time", default_value="true",
        description="Use Gazebo /clock topic for ROS2 time")

    # ── Gazebo Harmonic 启动 ──────────────────────────────────────────
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            os.path.join(
                get_package_share_directory("ros_gz_sim"),
                "launch", "gz_sim.launch.py"),
        ]),
        launch_arguments={
            # -s = headless server mode (WSL2/无 GPU 可用)
            # 移除 -s 可启用 GUI（需要 GPU 加速）
            "gz_args": f"-r -s {world_path}",
        }.items(),
    )

    # ── Spawn AMR 机器人模型 ──────────────────────────────────────────
    # 使用 ros_gz_sim 的 create 节点将机器人模型生成到仿真中
    from launch_ros.actions import Node as RosNode
    spawn_amr = RosNode(
        package="ros_gz_sim",
        executable="create",
        arguments=[
            "-name", "amr",
            "-file", amr_path,
            "-x", "0", "-y", "0", "-z", "0.1",
        ],
        output="screen",
    )

    # ── ros_gz_bridge: 传感器数据桥接 ──────────────────────────────────
    # Gazebo 传感器 topic → ROS2 topic，使用 parameter_bridge 语法
    #
    # 语法: <gz_topic>@<ros_type>[<gz_type>]
    # 方向: [ 表示 gz→ros, ] 表示 ros→gz, @ 双向
    bridge_lidar = RosNode(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="bridge_lidar",
        arguments=[
            "/world/warehouse/model/amr/link/chassis/sensor/lidar/scan"
            "@sensor_msgs/msg/LaserScan"
            "[gz.msgs.LaserScan",
        ],
        remappings=[
            ("/world/warehouse/model/amr/link/chassis/sensor/lidar/scan",
             "/sensor/lidar"),
        ],
        output="screen",
    )

    bridge_imu = RosNode(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="bridge_imu",
        arguments=[
            "/world/warehouse/model/amr/link/chassis/sensor/imu/imu"
            "@sensor_msgs/msg/Imu"
            "[gz.msgs.IMU",
        ],
        remappings=[
            ("/world/warehouse/model/amr/link/chassis/sensor/imu/imu",
             "/sensor/imu"),
        ],
        output="screen",
    )

    bridge_camera = RosNode(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="bridge_camera",
        arguments=[
            "/world/warehouse/model/amr/link/chassis/sensor/camera/image"
            "@sensor_msgs/msg/Image"
            "[gz.msgs.Image",
        ],
        remappings=[
            ("/world/warehouse/model/amr/link/chassis/sensor/camera/image",
             "/sensor/camera"),
        ],
        output="screen",
    )

    # ── 我们的业务节点（LifecycleNode, 自激活） ────────────────────────
    # 注意：仿真模式下 sensor 节点不启动——Gazebo 传感器通过 ros_gz_bridge 直接
    # 提供 /sensor/lidar, /sensor/imu, /sensor/camera 数据。
    # 仅在非仿真模式（system.launch.py）启动 sensor 节点做模拟数据生成。
    compute_nodes = [
        ("fusion_node", "fusion"),
        ("decision_node", "decision"),
        ("motor_ctrl_node", "motor_ctrl"),
        ("health_monitor_node", "health_monitor"),
    ]

    nodes = []
    for exec_name, node_name in compute_nodes:
        nodes.append(
            LifecycleNode(
                package="ros2_robot_middleware",
                executable=exec_name,
                name=node_name,
                namespace="",
                output="screen",
                parameters=[{"use_sim_time": use_sim_time}],
            )
        )

    return LaunchDescription([
        declare_use_sim_time,
        gazebo,
        spawn_amr,
        bridge_lidar,
        bridge_imu,
        bridge_camera,
        *nodes,
    ])
