"""Launch the full AMR pipeline — 自研栈形态（A/B 基线；非生产导航）。

> 收敛注记（2026-09-04 ADR）：生产导航已移交 NAV2（nav2_localized）。
> 本 launch 启动的 fusion→decision→motor 管线是 A/B 对照基线与测试
> 载体；真机 NAV2 接线（本文件的 NAV2 化改写）为 roadmap。
> 原 docstring 自称 production process layout ——四审文档一致性项修正。

Process model:
  Process 1: lidar_node       — independent (driver isolation)
  Process 2: imu_node         — independent (driver isolation)
  Process 3: camera_node      — independent (driver isolation)
  Process 4: compute_container — fusion + decision + motor_ctrl (zero-copy)
  Process 5: ekf_node          — robot_localization EKF (pose estimation)
  Process 6: health_monitor   — independent (monitoring isolation)

9 nodes → 6 processes. Compute nodes share memory via shared_ptr;
sensor drivers and health monitor remain isolated for fault containment.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess
from launch_ros.actions import Node, LifecycleNode


def generate_launch_description():
    pkg_dir = get_package_share_directory("ros2_robot_middleware")
    ekf_config = os.path.join(pkg_dir, "config", "ekf.yaml")

    return LaunchDescription([
        # Clean stale shared memory from previous abnormal exit
        ExecuteProcess(
            cmd=['rm', '-f', '/dev/shm/amr_metrics_registry'],
            name='clean_shm',
            shell=False,
        ),

        # ── Sensor Layer — independent processes for driver fault isolation ──
        LifecycleNode(
            package='ros2_robot_middleware',
            executable='lidar_node',
            name='lidar',
            namespace='',
            output='screen',
        ),
        LifecycleNode(
            package='ros2_robot_middleware',
            executable='imu_node',
            name='imu',
            namespace='',
            output='screen',
        ),
        LifecycleNode(
            package='ros2_robot_middleware',
            executable='camera_node',
            name='camera',
            namespace='',
            output='screen',
        ),

        # ── Compute Layer — fusion + decision + motor_ctrl in single process ──
        # Sensor types declared via ROS2 params (default: simulated).
        # Override in config/sensors.yaml — no recompilation needed.
        Node(
            package='ros2_robot_middleware',
            executable='compute_container',
            name='compute',
            namespace='',
            output='screen',
        ),

        # ── TF — static sensor-to-base_link transforms ──
        # Required for robot_localization to transform IMU data to base_link.
        # base_link ←→ imu_link: IMU is co-located with chassis center.
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='tf_imu_link',
            output='screen',
            arguments=['0', '0', '0', '0', '0', '0', 'base_link', 'imu_link'],
        ),
        # base_link ←→ lidar_frame: LiDAR mounted 0.2m above chassis center.
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='tf_lidar_frame',
            output='screen',
            arguments=['0', '0', '0.2', '0', '0', '0', 'base_link', 'lidar_frame'],
        ),

        # ── Localization — robot_localization EKF (IMU + odom → /odom) ──
        # Feeds MotorCtrlNode.current for closed-loop control (P1f).
        # Default output is /odometry/filtered — remap to /odom.
        Node(
            package='robot_localization',
            executable='ekf_node',
            name='ekf_filter_node',
            namespace='',
            output='screen',
            parameters=[ekf_config],
            remappings=[
                ('/odometry/filtered', '/odom'),
                ('/odom', '/odom'),  # keep tf output unchanged
            ],
        ),

        # ── Infrastructure — independent, must not share fate with monitored ──
        LifecycleNode(
            package='ros2_robot_middleware',
            executable='health_monitor_node',
            name='health_monitor',
            namespace='',
            output='screen',
            respawn=True,
            respawn_delay=2.0,
        ),
    ])
