"""Gazebo Harmonic 仿真启动 — AMR 机器人 + 传感器桥梁 + 业务节点

架构：
  Gazebo Sensors → ros_gz_bridge → ROS2 Topics → 我们的业务节点
                    (gz → ros2)     (/sensor/*)

启动后检查：
  ros2 topic list | grep -E "scan|points"   # 确认数据 topic 存在
  ros2 topic hz /scan                       # 确认 LiDAR 数据流通（~10Hz）
  ros2 topic hz /points                     # 确认点云流通
  ros2 topic pub -1 /cmd_vel geometry_msgs/Twist "{linear: {x: 0.2}}"  # 确认车动
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
            # -s --headless-rendering：服务器 + 离屏渲染（传感器仍产数据）。
            # 不用 GUI 模式——WSL2 d3d12 下 Qt GLX 集成偶发崩溃（RenderThread 段错误）。
            # 可视化交给 Foxglove（点云/图像/里程均可看）。
            "gz_args": f"-r -s --headless-rendering {world_path}",
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
            "-x", "0", "-y", "0", "-z", "0.15",  # 略高于着地高度(0.125)，落下后轮子干净着地
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
    # 实测：gpu_lidar 数据话题为 /lidar（<topic>lidar</topic> 解析到根），
    # 帧路径 .../sensor/lidar/scan 为空话题。桥接源必须用 /lidar。
    bridge_lidar = RosNode(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="bridge_lidar",
        arguments=[
            "/lidar"
            "@sensor_msgs/msg/LaserScan"
            "[gz.msgs.LaserScan",
        ],
        remappings=[
            ("/lidar", "/scan"),
        ],
        output="screen",
    )

    # 点云桥（新增）：gz /lidar/points → ROS /points (PointCloud2)
    bridge_points = RosNode(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="bridge_points",
        arguments=[
            "/lidar/points"
            "@sensor_msgs/msg/PointCloud2"
            "[gz.msgs.PointCloudPacked",
        ],
        remappings=[
            ("/lidar/points", "/points"),
        ],
        output="screen",
    )

    bridge_imu = RosNode(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="bridge_imu",
        arguments=[
            "/imu"
            "@sensor_msgs/msg/Imu"
            "[gz.msgs.IMU",
        ],
        remappings=[
            ("/imu", "/sensor/imu"),
        ],
        output="screen",
    )

    bridge_camera = RosNode(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="bridge_camera",
        arguments=[
            "/camera"
            "@sensor_msgs/msg/Image"
            "[gz.msgs.Image",
        ],
        remappings=[
            ("/camera", "/sensor/camera"),
        ],
        output="screen",
    )

    # 速度指令桥（新增方向）：ROS /cmd_vel → gz /cmd_vel (Twist)
    # 闭环最后一跳：motor 的指令经此桥回到 Gazebo DiffDrive
    bridge_cmd_vel = RosNode(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="bridge_cmd_vel",
        arguments=[
            "/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist",
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
                # 仿真模式：LiDAR 从 Gazebo 桥接的 /scan 读真实点云，
                # 而非内部 Simulated 传感器（Simulated 是纯数学噪声）。
                # IMU 保持 simulated（robot_localization 用，模拟数据够）。
                "sensors.lidar.type": "sick_tim781",
                "sensors.lidar.topic": "/scan",
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
        bridge_points,
        bridge_imu,
        bridge_camera,
        bridge_cmd_vel,
        *nodes,
    ])
