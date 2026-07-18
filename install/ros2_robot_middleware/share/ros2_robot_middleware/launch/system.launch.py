"""Launch the full AMR pipeline — production process layout.

Process model:
  Process 1: lidar_node       — independent (driver isolation)
  Process 2: imu_node         — independent (driver isolation)
  Process 3: camera_node      — independent (driver isolation)
  Process 4: compute_container — fusion + decision + motor_ctrl (zero-copy)
  Process 5: health_monitor   — independent (monitoring isolation)

8 nodes → 5 processes. Compute nodes share memory via shared_ptr;
sensor drivers and health monitor remain isolated for fault containment.
"""

from launch import LaunchDescription
from launch_ros.actions import Node, LifecycleNode


def generate_launch_description():
    return LaunchDescription([
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
        # Zero-copy via shared_ptr between nodes (no DDS serialization overhead).
        # MultiThreadedExecutor for parallel callback processing.
        Node(
            package='ros2_robot_middleware',
            executable='compute_container',
            name='compute',
            namespace='',
            output='screen',
        ),

        # ── Infrastructure — independent, must not share fate with monitored ──
        LifecycleNode(
            package='ros2_robot_middleware',
            executable='health_monitor_node',
            name='health_monitor',
            namespace='',
            output='screen',
        ),
    ])
