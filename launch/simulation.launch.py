"""Gazebo Harmonic 仿真 — AMR + gz bridge + 业务节点（GUI 模式）。

架构：gz sim(GUI) → ros_gz bridge → ROS2 → compute_container(decision/fusion/motor)。
gz 发 /scan /odom /tf /clock，吃 /cmd_vel；mock_amcl 补 map 帧（decision 要
/amcl_pose + TF map→amr/odom 才能规划/dispatch）。

Server-only headless 必须：gpu_lidar 走 ogre2 EGL 渲染（--headless-rendering），
绕开 WSLg 的 GLX——GUI 模式下 ogre2 RTT readback 在 Mesa D3D12 静默失败
（lidar 全 inf）/相机 SIGSEGV（见 amr.sdf 相机注释）。-s 不起 GUI 窗口，
可视化完全靠 Foxglove。GUI 模式（旧注释）的"GUI 必须/无 GPU"是误判。

检查：ros2 topic hz /scan（~10Hz，ranges 有值）+ /amcl_pose（mock 发）+ cmd_vel → 车动。
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess, DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_xml.launch_description_sources import XMLLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node as RosNode


def generate_launch_description():
    pkg_dir = get_package_share_directory("ros2_robot_middleware")
    world_path = os.path.join(pkg_dir, "worlds", "factory_3c.sdf")
    amr_path = os.path.join(pkg_dir, "worlds", "amr.sdf")
    bridge_yaml = os.path.join(pkg_dir, "config", "gz_bridge.yaml")
    use_sim_time = LaunchConfiguration("use_sim_time", default="true")

    # gz sim（server-only headless：gpu_lidar 走 ogre2 EGL，绕开 WSLg GLX readback bug）
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            os.path.join(get_package_share_directory("ros_gz_sim"),
                         "launch", "gz_sim.launch.py")]),
        launch_arguments={"gz_args": f"-s -r --headless-rendering -v4 {world_path}"}.items())

    spawn_amr = RosNode(
        package="ros_gz_sim", executable="create",
        arguments=["-name", "amr", "-file", amr_path, "-x", "1", "-y", "0", "-z", "0"],
        output="screen")

    # ros_gz bridge（YAML：/scan /odom reliable，/tf，/cmd_vel，/clock）
    bridge = RosNode(
        package="ros_gz_bridge", executable="parameter_bridge",
        parameters=[{"config_file": bridge_yaml, "use_sim_time": use_sim_time}],
        output="screen")

    # scan_filter：仿真专用 /scan_raw → /scan 近距伪影滤除(<0.35m, WSL2 gpu_lidar 幻影)。
    # 传感器参数不可改(min_range/高度改动→渲染静默失效, 2026-08-16 实验), 只能桥后过滤。
    # 真机 launch 不含此节点; 阈值经 launch 参数可调。
    scan_filter = RosNode(
        package="ros2_robot_middleware", executable="scan_filter",
        parameters=[{"use_sim_time": use_sim_time, "min_valid_range": 0.35}],
        output="screen")

    # mock_amcl：gz /odom → /amcl_pose(map) + TF map→amr/odom（decision A* 起点 + dispatch 变换）
    mock_amcl = RosNode(
        package="ros2_robot_middleware", executable="mock_amcl",
        parameters=[{"use_sim_time": use_sim_time}], output="screen")

    # factory_markers：发 gz world 障碍 MarkerArray（墙+料架+机台）— Foxglove 可视化
    factory_markers = RosNode(
        package="ros2_robot_middleware", executable="factory_markers",
        parameters=[{"use_sim_time": use_sim_time}], output="screen")

    # robot_markers：车体 MarkerArray（底盘+lidar+轮）— Foxglove 看车在哪
    robot_markers = RosNode(
        package="ros2_robot_middleware", executable="robot_markers",
        parameters=[{"use_sim_time": use_sim_time}], output="screen")

    # patrol_3c：送料往返（起点→机台1→机台2→起点 循环），到 goal 后 set next goal
    patrol_3c = RosNode(
        package="ros2_robot_middleware", executable="patrol_3c",
        parameters=[{"use_sim_time": use_sim_time}], output="screen")

    # static_tf amr/chassis → amr/chassis/lidar (0.25, 0, 1.00)（对齐 amr.sdf lidar pose）
    static_tf_lidar = RosNode(
        package="tf2_ros", executable="static_transform_publisher",
        parameters=[{"use_sim_time": use_sim_time}],
        arguments=["0.25", "0", "1.00", "0", "0", "0",
                   "amr/chassis", "amr/chassis/lidar"],
        output="screen")

    # foxglove_bridge：随仿真起（t=0 就在）—— 否则启动间隙 Studio 连不上，
    # 表现为"刚开始看不到环境和小车"（2026-08-17 用户反馈）
    foxglove = IncludeLaunchDescription(
        XMLLaunchDescriptionSource([
            os.path.join(get_package_share_directory("foxglove_bridge"),
                         "launch", "foxglove_bridge_launch.xml")]),
        launch_arguments={"port": "8765"}.items())

    # compute_container（goal=15，绕障；不设 name=：会重命名三节点冲突）
    compute = RosNode(
        package="ros2_robot_middleware", executable="compute_container",
        namespace="", output="screen",
        parameters=[{
            "use_sim_time": use_sim_time,
            "sensors.lidar.type": "sick_tim781", "sensors.lidar.topic": "/scan",
            "goal_x": 17.0, "goal_y": 4.0,
            "vfh_enabled": False,
            # guard fail-safe（2026-08-17 穿货架事故）：
            # stop_dist 必须 > scan_filter 滤除阈值(0.35)，否则滤后管道永远
            # 报不出更近的回波，硬停不可达，车 0.05m/s 顶进货架。
            "guard_stop_dist": 0.40,
            # 全向有效回波低于此数 = gpu_lidar 劣化至全盲（消息仍准点发布，
            # stale/empty 检测拦不住），fail-safe 硬停而非按"空旷场地"放行。
            # 健康 ~300 回波 / 扇区失明 ~160 / 全盲 0；0 = 禁用（真机默认）。
            "guard_min_valid_echoes": 50,
        }])

    return LaunchDescription([
        ExecuteProcess(cmd=['rm', '-f', '/dev/shm/amr_metrics_registry'], shell=False),
        DeclareLaunchArgument("use_sim_time", default_value="true",
                              description="Use Gazebo /clock"),
        gazebo, spawn_amr, bridge, scan_filter, mock_amcl, factory_markers, robot_markers, static_tf_lidar, compute, patrol_3c, foxglove,
    ])
