"""SROS2 secure launch — DDS-Security keys for all nodes.

Prerequisites (one-time):
  cd config/sros2
  ros2 security create_keystore .
  for n in /lidar /imu /camera /compute /health_monitor; do
    ros2 security create_enclave . $n
  done

Verification:
  ros2 topic echo /sensor/lidar  # unauthorized nodes cannot subscribe
  tcpdump -i lo -A | grep "lidar"  # data is encrypted
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess
from launch_ros.actions import Node, LifecycleNode


def generate_launch_description():
    pkg_dir = get_package_share_directory("ros2_robot_middleware")
    keystore = os.path.join(pkg_dir, "config", "sros2")

    env = {
        "ROS_SECURITY_KEYSTORE": keystore,
        "ROS_SECURITY_ENABLE": "true",
        "ROS_SECURITY_STRATEGY": "Enforce",
    }

    return LaunchDescription([
        ExecuteProcess(
            cmd=['rm', '-f', '/dev/shm/amr_metrics_registry'],
            name='clean_shm',
            shell=False,
        ),
        # Sensor drivers (independent, per-driver enclave)
        LifecycleNode(
            package="ros2_robot_middleware",
            executable="lidar_node",
            name="lidar",
            namespace="",
            additional_env=env,
            output="screen",
        ),
        LifecycleNode(
            package="ros2_robot_middleware",
            executable="imu_node",
            name="imu",
            namespace="",
            additional_env=env,
            output="screen",
        ),
        LifecycleNode(
            package="ros2_robot_middleware",
            executable="camera_node",
            name="camera",
            namespace="",
            additional_env=env,
            output="screen",
        ),

        # Compute container (fusion + decision + motor_ctrl in one process)
        Node(
            package="ros2_robot_middleware",
            executable="compute_container",
            name="compute",
            namespace="",
            additional_env=env,
            output="screen",
        ),

        # Health monitor (independent, must not share fate with monitored)
        LifecycleNode(
            package="ros2_robot_middleware",
            executable="health_monitor_node",
            name="health_monitor",
            namespace="",
            additional_env=env,
            output="screen",
        ),
    ])
