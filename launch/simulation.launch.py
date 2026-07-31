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
from launch.actions import ExecuteProcess
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
            # GUI 模式（无 -s）：headless 下渲染系统不初始化，lidar/camera 传感器不产数据。
            # GUI 强制激活渲染 → 传感器工作。窗口可最小化，显示交给 Foxglove。
            # 为降低 CPU 负载：窗口最小化 + 降低传感器频率（见 amr.sdf）。
            "gz_args": f"-r {world_path}",
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

    # ── ros_gz_bridge: 时钟桥接 ────────────────────────────────────────
    # 关键：use_sim_time=true 的节点需要 /clock，否则回调阻塞。
    # Gazebo /clock → ROS2 /clock (rosgraph_msgs/Clock)
    bridge_clock = RosNode(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="bridge_clock",
        arguments=[
            "/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock",
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

    # ── 我们的业务节点 ──────────────────────────────────────────────
    # 仿真模式下 sensor 节点不启动——Gazebo 传感器通过 ros_gz_bridge 直接
    # 提供 /sensor/lidar, /sensor/imu, /sensor/camera 数据。
    # compute_container 承载 fusion + decision + motor（零拷贝），
    # health_monitor 独立（与 system.launch.py 一致）。
    nodes = [
        RosNode(
            package="ros2_robot_middleware",
            executable="compute_container",
            name="compute",
            namespace="",
            output="screen",
            parameters=[{
                "use_sim_time": use_sim_time,
                # 仿真模式：LiDAR 从 Gazebo 桥接的 /sensor/lidar 读真实点云，
                # 而非内部 Simulated 传感器（Simulated 是纯数学噪声）。
                # IMU 保持 simulated（robot_localization 用，模拟数据够）。
                "sensors.lidar.type": "sick_tim781",
                "sensors.lidar.topic": "/sensor/lidar",
            }],
        ),
        LifecycleNode(
            package="ros2_robot_middleware",
            executable="health_monitor_node",
            name="health_monitor",
            namespace="",
            output="screen",
            parameters=[{"use_sim_time": use_sim_time}],
            respawn=True,
            respawn_delay=2.0,
        ),
    ]

    return LaunchDescription([
        ExecuteProcess(
            cmd=['rm', '-f', '/dev/shm/amr_metrics_registry'],
            name='clean_shm',
            shell=False,
        ),
        declare_use_sim_time,
        gazebo,
        spawn_amr,
        bridge_clock,
        bridge_lidar,
        bridge_imu,
        bridge_camera,
        *nodes,
    ])
