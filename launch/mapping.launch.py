"""Gazebo Harmonic 建图 — slam_toolbox 在线 SLAM 生成 warehouse 静态地图

流程：车巡图 → /map 累积 → 保存 map.pgm/yaml 供 AMCL 定位（G1）

TF 说明：
  gz DiffDrive 发布 /model/amr/tf（amr/odom → amr/chassis），
  经 ros_gz_bridge 桥到 ROS /tf，slam_toolbox 直接用这两个 frame。
  建图输出：map → amr/odom TF。

保存地图：
  ros2 run nav2_map_server map_saver_cli -f ~/amr_map
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, EmitEvent, IncludeLaunchDescription
from launch.events import matches_action
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node as RosNode
from launch_ros.actions import LifecycleNode
from launch_ros.events.lifecycle import ChangeState
from lifecycle_msgs.msg import Transition


def generate_launch_description():
    pkg_dir = get_package_share_directory("ros2_robot_middleware")
    world_path = os.path.join(pkg_dir, "worlds", "warehouse.sdf")
    amr_path = os.path.join(pkg_dir, "worlds", "amr.sdf")
    slam_params = os.path.join(pkg_dir, "config", "slam_mapping.yaml")

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

    bridge_clock = RosNode(
        package="ros_gz_bridge", executable="parameter_bridge",
        name="bridge_clock",
        arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"],
        output="screen",
    )

    bridge_lidar = RosNode(
        package="ros_gz_bridge", executable="parameter_bridge",
        name="bridge_lidar",
        arguments=["/lidar@sensor_msgs/msg/LaserScan[gz.msgs.LaserScan"],
        remappings=[("/lidar", "/scan")],
        output="screen",
    )

    # gz DiffDrive TF（amr/odom → amr/chassis）→ ROS /tf（slam 的 tf listener 监听 /tf）
    bridge_tf = RosNode(
        package="ros_gz_bridge", executable="parameter_bridge",
        name="bridge_tf",
        arguments=["/model/amr/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V"],
        remappings=[("/model/amr/tf", "/tf")],
        output="screen",
    )

    # 里程计桥 — 巡图脚本/定位用闭环位姿
    bridge_odom = RosNode(
        package="ros_gz_bridge", executable="parameter_bridge",
        name="bridge_odom",
        arguments=["/model/amr/odometry@nav_msgs/msg/Odometry[gz.msgs.Odometry"],
        remappings=[("/model/amr/odometry", "/odom")],
        output="screen",
    )

    bridge_cmd_vel = RosNode(
        package="ros_gz_bridge", executable="parameter_bridge",
        name="bridge_cmd_vel",
        arguments=["/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist"],
        output="screen",
    )

    # LiDAR 外参 TF：/scan frame=amr/chassis/lidar，需连到 base（amr/chassis）
    # amr.sdf: <pose>0.25 0 0.05 0 0 0</pose>（sensor 在 chassis 坐标系）
    static_tf_lidar = RosNode(
        package="tf2_ros", executable="static_transform_publisher",
        parameters=[{"use_sim_time": use_sim_time}],
        arguments=["0.25", "0", "0.30", "0", "0", "0",
                   "amr/chassis", "amr/chassis/lidar"],
        output="screen",
    )

    slam_toolbox = LifecycleNode(
        parameters=[slam_params, {"use_sim_time": use_sim_time}],
        package="slam_toolbox", executable="async_slam_toolbox_node",
        name="slam_toolbox", namespace="", output="screen",
    )

    configure = EmitEvent(
        event=ChangeState(
            lifecycle_node_matcher=matches_action(slam_toolbox),
            transition_id=Transition.TRANSITION_CONFIGURE))
    activate = EmitEvent(
        event=ChangeState(
            lifecycle_node_matcher=matches_action(slam_toolbox),
            transition_id=Transition.TRANSITION_ACTIVATE))

    return LaunchDescription([
        declare_use_sim_time,
        gazebo,
        spawn_amr,
        bridge_clock,
        bridge_lidar,
        bridge_tf,
        bridge_odom,
        bridge_cmd_vel,
        static_tf_lidar,
        slam_toolbox,
        configure,
        activate,
    ])
