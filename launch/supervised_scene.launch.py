"""Supervised scene_simulator form — 72h soak 专用（云端无 GPU 环境）。

与 supervised_sim.launch.py 同一监管模式，但传感器源为 scene_simulator
（纯 CPU 合成传感，无 Gazebo/gpu_lidar 依赖）——适合云服务器 72h soak。
节点树：supervisor → { scene_simulator → compute → patrol }
"""
import os
import shutil

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess
from launch_ros.actions import Node

ROS2 = shutil.which("ros2") or "/opt/ros/jazzy/bin/ros2"


def generate_launch_description():
    pkg = get_package_share_directory("ros2_robot_middleware")
    pkg_root = os.path.dirname(os.path.dirname(pkg))
    exe = lambda name: os.path.join(pkg_root, "lib", "ros2_robot_middleware", name)  # noqa: E731

    sim = lambda extra="": ["--ros-args", "-p", "use_sim_time:=false"] + (extra.split() if extra else [])  # noqa: E731

    p = {
        "supervisor.children": ["scene_simulator", "compute", "patrol"],
        "supervisor.scene_simulator.cmd": [exe("scene_simulator")],
        "supervisor.scene_simulator.max_restarts": 10,
        # compute: 传感器 type=simulated（SensorFactory 进程内实例化）,
        # 消费 scene_simulator 发布的 /odom /amcl_pose
        "supervisor.compute.cmd": [exe("compute_container"), *sim(
            "-p goal_x:=17.0 -p goal_y:=4.0"
            " -p guard_min_valid_echoes:=0")],  # scene_sim 不发 /scan（进程内合成）
        "supervisor.compute.depends_on": ["scene_simulator"],
        "supervisor.patrol.cmd": [exe("patrol_3c"), *sim()],
        "supervisor.patrol.depends_on": ["compute"],
    }

    return LaunchDescription([
        ExecuteProcess(cmd=["rm", "-f", "/dev/shm/amr_metrics_registry"], shell=False),
        Node(package="ros2_robot_middleware", executable="amr_supervisor",
             name="supervisor", parameters=[p], output="screen"),
    ])
