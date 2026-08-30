"""SROS2 secure launch — DDS-Security keys for all nodes.

2026-08-30 修通（此前从未工作过）：per-node 身份必须经 ros args
`--ros-args --enclave /<name>` 传递——仅设 ROS_SECURITY_* 环境变量时
fqn 不会拼入安全目录路径（strace 实证，见 verify_dds_security.sh 头注）。

Prerequisites (one-time; keystore 在 gitignore 内，不入库):
  cd config/sros2
  ros2 security create_keystore .
  for n in /lidar /imu /camera /fusion /decision /motor_ctrl /health_monitor; do
    ros2 security create_enclave . $n
  done
  # 注意: keystore 经 install(DIRECTORY config) 拷入 share——重生成后需
  # colcon build 刷新 install 侧副本（或外部覆盖 ROS_SECURITY_KEYSTORE）。

Verification: ./scripts/verify_dds_security.sh
  （四段断言: 认证互通/野节点被拒/真实栈 Enforce/授权 DENY 拒入域）
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
            arguments=["--ros-args", "--enclave", "/lidar"],
            additional_env=env,
            output="screen",
        ),
        LifecycleNode(
            package="ros2_robot_middleware",
            executable="imu_node",
            name="imu",
            namespace="",
            arguments=["--ros-args", "--enclave", "/imu"],
            additional_env=env,
            output="screen",
        ),
        LifecycleNode(
            package="ros2_robot_middleware",
            executable="camera_node",
            name="camera",
            namespace="",
            arguments=["--ros-args", "--enclave", "/camera"],
            additional_env=env,
            output="screen",
        ),

        # Compute container (fusion + decision + motor_ctrl in one process)
        Node(
            package="ros2_robot_middleware",
            executable="compute_container",
            name="compute",
            namespace="",
            # 容器单进程单身份: fusion/decision/motor 共用 /fusion enclave
            arguments=["--ros-args", "--enclave", "/fusion"],
            additional_env=env,
            output="screen",
        ),

        # Health monitor (independent, must not share fate with monitored)
        # 注：LifecycleNode 无 respawn 参数（那是 Node 的）——生命周期节点的
        # 监管由本节点自身的 watchdog ChangeState 序列承担，不走进程重启。
        # （2026-08-25 审计 P1-a：本文件曾有双逗号语法错且从未运行过，现修通）
        LifecycleNode(
            package="ros2_robot_middleware",
            executable="health_monitor_node",
            name="health_monitor",
            namespace="",
            arguments=["--ros-args", "--enclave", "/health_monitor"],
            additional_env=env,
            output="screen",
        ),
    ])
