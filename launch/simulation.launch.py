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
            # 必须用 GUI 模式（不带 -s/headless）：gpu_lidar 需要渲染场景，headless
            # 下 WSL2 无 GPU 上下文 → /scan 全 inf，护栏/VFH/感知全失效。
            # GUI 窗口在 WSL2 d3d12 下会黑屏/偶发崩溃，但这是纯显示问题——
            # 仿真与传感器数据完全正常，可视化交给 Foxglove。
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
            "-x", "0", "-y", "0", "-z", "0.0",  # 轮底 model z=0.025，0.0 几乎贴地，最低冲击
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

    # 里程计桥（新增方向）：gz DiffDrive odometry → ROS /odom
    # motor_ctrl 以 /odom 为闭环位姿源（订阅 robot_localization EKF 或此直连），
    # 缺此桥则 motor fallback 运动学积分，位姿与物理车漂移。
    bridge_odom = RosNode(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="bridge_odom",
        arguments=[
            "/model/amr/odometry"
            "@nav_msgs/msg/Odometry"
            "[gz.msgs.Odometry",
        ],
        remappings=[
            ("/model/amr/odometry", "/odom"),
        ],
        output="screen",
    )

    # 静态 TF：chassis → lidar（0.25 0 0.30），Foxglove URDF 渲染 lidar 圆柱用
    static_tf_lidar = RosNode(
        package="tf2_ros", executable="static_transform_publisher",
        parameters=[{"use_sim_time": use_sim_time}],
        arguments=["0.25", "0", "0.30", "0", "0", "0",
                   "amr/chassis", "amr/chassis/lidar"],
        output="screen",
    )

    # TF 桥（新增）：gz 模型位姿 → ROS /tf
    # 没有它 /tf 发布者为 0，Foxglove 3D 面板无坐标系可渲染 → 看不到小车。
    bridge_tf = RosNode(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="bridge_tf",
        arguments=[
            "/model/amr/tf"
            "@tf2_msgs/msg/TFMessage"
            "[gz.msgs.Pose_V",
        ],
        remappings=[
            ("/model/amr/tf", "/tf"),
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
            # 不能设 name=：compute_container 内部创建 fusion/decision/motor_ctrl 三个节点，
            # name= 会把三者全部重命名成 compute，导致节点冲突、订阅注册异常（/scan 无订阅者）。
            namespace="",
            output="screen",
            parameters=[{
                "use_sim_time": use_sim_time,
                # 仿真模式：LiDAR 从 Gazebo 桥接的 /scan 读真实点云，
                # 而非内部 Simulated 传感器（Simulated 是纯数学噪声）。
                # IMU 保持 simulated（robot_localization 用，模拟数据够）。
                "sensors.lidar.type": "sick_tim781",
                "sensors.lidar.topic": "/scan",
                # decision 任务点（任务层下发）。warehouse 场景第一堵墙在 x=3，
                # 设 2.5 为墙前可达任务点 —— 感知物体全作障碍，不追目标。
                "goal_x": 2.5,
                "goal_y": 0.0,
            }],
        ),
        # health_monitor 不参与仿真：其 1s 检查会误判 compute 启动期节点为 ERROR，
        # 触发 change_state 把 fusion/decision/motor 卡成 inactive，导致发布中断。
        # （生产模式在 system.launch.py 使用 health_monitor）
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
        bridge_odom,
        bridge_tf,
        static_tf_lidar,
        *nodes,
    ])
