"""Multi-AMR cluster launch with Fleet Manager — production process layout.

Per-AMR: 5 processes (3 sensor drivers + compute_container + health_monitor)
Fleet Manager: 1 process (cross-AMR control plane)

Production deployment:
  Each AMR runs its own Docker container with independent ROS_DOMAIN_ID.
  Fleet Manager uses Fast-DDS Partitions or Domain Bridge for cross-domain comms.
"""

from launch import LaunchDescription
from launch.actions import ExecuteProcess
from launch_ros.actions import Node, LifecycleNode

SENSOR_NODES = [
    ("lidar_node",  "lidar"),
    ("imu_node",    "imu"),
    ("camera_node", "camera"),
]


def generate_launch_description():
    desc = []

    # ── AMR-1 Sensor Layer (independent processes) ─────────────────────
    for exec_name, node_name in SENSOR_NODES:
        desc.append(
            LifecycleNode(
                package="ros2_robot_middleware",
                executable=exec_name,
                name=f"amr1_{node_name}",
                namespace="",
                output="screen",
            )
        )

    # ── AMR-1 Compute Container (fusion + decision + motor_ctrl) ──────
    desc.append(
        Node(
            package="ros2_robot_middleware",
            executable="compute_container",
            name="amr1_compute",
            namespace="",
            output="screen",
        )
    )

    # ── AMR-1 Health Monitor (remap output to /amr1/health/report) ────
    desc.append(
        LifecycleNode(
            package="ros2_robot_middleware",
            executable="health_monitor_node",
            name="amr1_health_monitor",
            namespace="",
            remappings=[
                ("/health/report", "/amr1/health/report"),
                ("/health/check",  "/amr1/health/check"),
            ],
            output="screen",
        )
    )

    # ── Fleet Manager (global control plane) ──────────────────────────
    desc.append(
        LifecycleNode(
            package="ros2_robot_middleware",
            executable="fleet_manager_node",
            name="fleet_manager",
            namespace="",
            output="screen",
        )
    )

    return LaunchDescription(desc)
