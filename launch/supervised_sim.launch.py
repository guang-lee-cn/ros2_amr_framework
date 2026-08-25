"""Gazebo 仿真 — supervised 形态：全栈由 amr_supervisor 进程级监管。

与 simulation.launch.py 同一业务栈，但每个长驻进程是 supervisor 的声明式
子项（B1，docs/design/20260825-b1-supervisor-adr.md）：
  - kill -9 任一子进程 → 秒级按策略恢复（退避 500ms 起，对比 watchdog 2-4min）
  - 依赖序：gz 死 → spawn_amr(oneshot 重跑)/bridge/scan_filter/… 逆拓扑让位、
    恢复后正拓扑重生
  - sim_watchdog 仍作外环（渲染劣化是「进程活着但功能死了」，v1 健康门不覆盖）

启动: LAUNCH_FILE=supervised_sim.launch.py ./scripts/run_sim.sh
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
    # share/ros2_robot_middleware → 同级 lib/ros2_robot_middleware（本包可执行文件）
    pkg_root = os.path.dirname(os.path.dirname(pkg))
    exe = lambda name: os.path.join(pkg_root, "lib", "ros2_robot_middleware", name)  # noqa: E731
    world = os.path.join(pkg, "worlds", "factory_3c.sdf")
    amr_sdf = os.path.join(pkg, "worlds", "amr.sdf")
    bridge_yaml = os.path.join(pkg, "config", "gz_bridge.yaml")

    sim = lambda extra="": [  # 仿真侧统一前缀（use_sim_time 对齐 simulation.launch.py） noqa: E731
        "--ros-args", "-p", "use_sim_time:=true"] + (extra.split() if extra else [])

    # ── 声明式子项：cmd / depends_on / oneshot / 退避与预算 ──────────────
    p = {
        "supervisor.children": [
            "gz", "spawn_amr", "bridge", "scan_filter", "mock_amcl",
            "static_tf", "factory_markers", "robot_markers", "foxglove",
            "compute", "patrol",
        ],
        # gz 传感源头（含渲染）。挂了全链让位，恢复后 spawn_amr 重跑放车
        "supervisor.gz.cmd": [
            ROS2, "launch", "ros_gz_sim", "gz_sim.launch.py",
            f"gz_args:=-s -r --headless-rendering -v4 {world}"],
        "supervisor.gz.max_restarts": 10,
        # 放车（一次性）：gz 重启后必须重跑——oneshot + 依赖清除语义
        "supervisor.spawn_amr.cmd": [
            ROS2, "run", "ros_gz_sim", "create",
            "-name", "amr", "-file", amr_sdf, "-x", "1", "-y", "0", "-z", "0"],
        "supervisor.spawn_amr.depends_on": ["gz"],
        "supervisor.spawn_amr.oneshot": True,
        "supervisor.spawn_amr.backoff_base_ms": 2000,
        "supervisor.spawn_amr.max_restarts": 10,
        # 桥（/scan_raw 的发布者）
        "supervisor.bridge.cmd": [
            ROS2, "run", "ros_gz_bridge", "parameter_bridge",
            "--ros-args", "-p", f"config_file:={bridge_yaml}", "-p", "use_sim_time:=true"],
        "supervisor.bridge.depends_on": ["gz"],
        # 仿真专用滤伪影（阈值同 simulation.launch.py）
        "supervisor.scan_filter.cmd": [exe("scan_filter"), *sim("-p min_valid_range:=0.35")],
        "supervisor.scan_filter.depends_on": ["bridge"],
        # gz /odom → /amcl_pose + TF map→amr/odom
        "supervisor.mock_amcl.cmd": [exe("mock_amcl"), *sim()],
        "supervisor.mock_amcl.depends_on": ["bridge"],
        # 可视化叶节点（无依赖，不随业务链让位）
        "supervisor.static_tf.cmd": [
            ROS2, "run", "tf2_ros", "static_transform_publisher",
            "0.25", "0", "1.00", "0", "0", "0",
            "amr/chassis", "amr/chassis/lidar"],
        "supervisor.factory_markers.cmd": [exe("factory_markers"), *sim()],
        "supervisor.robot_markers.cmd": [exe("robot_markers"), *sim()],
        "supervisor.foxglove.cmd": [
            ROS2, "run", "foxglove_bridge", "foxglove_bridge",
            "--ros-args", "-p", "port:=8765"],
        # 计算容器（fusion/decision/motor）——soak compute 注入的恢复主体
        "supervisor.compute.cmd": [exe("compute_container"), *sim(
            "-p sensors.lidar.type:=sick_tim781"
            " -p sensors.lidar.topic:=/scan"
            " -p goal_x:=17.0 -p goal_y:=4.0"
            " -p vfh_enabled:=false"
            " -p guard_stop_dist:=0.40"
            " -p guard_min_valid_echoes:=50")],
        "supervisor.compute.depends_on": ["scan_filter", "mock_amcl"],
        # 送料往返负载
        "supervisor.patrol.cmd": [exe("patrol_3c"), *sim()],
        "supervisor.patrol.depends_on": ["compute"],
    }

    return LaunchDescription([
        # 清掉上次运行的跨进程指标共享内存（与 simulation.launch.py 同款）
        ExecuteProcess(cmd=["rm", "-f", "/dev/shm/amr_metrics_registry"], shell=False),
        Node(package="ros2_robot_middleware", executable="amr_supervisor",
             name="supervisor", parameters=[p], output="screen"),
    ])
