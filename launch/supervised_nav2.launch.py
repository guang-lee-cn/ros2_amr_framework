"""Supervised NAV2 生产形态 —— supervisor 监管整栈（F2）。

形态（2026-09-09 判别性实验裁决，见 docs/design F2 ADR）：
  supervisor → { health_monitor, nav2 }

为什么 nav2 是**一个**子进程而不是逐 server spawn：
  - lifecycle_manager 自己 configure/activate 全部 server，逐 server spawn 无人激活；
  - F2 的故障是「进程活着不工作」（死锁），launch 的 respawn 与 supervisor 的
    waitpid 都看不见它——只有进程组级 kill+respawn 能恢复；
  - `ros2 launch` 不调 setsid（site-packages 全量 grep 零命中），孙进程与 launch
    同组，W4 的 kill(-pgid) 覆盖整栈（test_supervisor_node 已锁定该契约）。

健康门：nav2 的 health_nodes = 6 个 NAV2 生命周期节点，health_monitor 用
lifecycle get_state 探针 1Hz 判定，**全部 ACTIVE 才算就位**（HealthFeed 聚合语义，
防「map_server 先 ACTIVE 就把整栈判成就位」的半死栈漏洞）。

health_monitor 只报告不处置（restart_enabled=false）：它的 lifecycle 四步链会与
lifecycle_manager 的状态跟踪脱节（manager 以为 server 是 active，实际被单独重启
过）。处置权唯一归 supervisor 的进程级 kill+respawn。

health_monitor 与 nav2 之间**不建 depends_on**：health_monitor 产出的是监管信息
而非 nav2 消费的数据，它挂了由 supervisor 自己重启，nav2 的进程级探视照常。
"""
import os
import shutil

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

# posix_spawn 不查 PATH：ros2 CLI 必须绝对路径（同 supervised_scene.launch.py）
ROS2 = shutil.which("ros2") or "/opt/ros/jazzy/bin/ros2"

# 与 nav2_localized.launch.py 的 lifecycle_manager.node_names 同源，改动需同步
NAV2_NODES = ["map_server", "amcl", "controller_server", "planner_server",
              "behavior_server", "bt_navigator"]


def generate_launch_description():
    pkg = get_package_share_directory("ros2_robot_middleware")
    pkg_root = os.path.dirname(os.path.dirname(pkg))
    exe = lambda name: os.path.join(pkg_root, "lib", "ros2_robot_middleware", name)  # noqa: E731

    probe_args = []
    for n in NAV2_NODES:
        probe_args += ["-p", f"health_monitor.{n}.probe:=lifecycle",
                       "-p", f"health_monitor.{n}.timeout_s:=3.0"]

    p = {
        "supervisor.children": ["health_monitor", "nav2"],
        "supervisor.health_monitor.cmd": [
            exe("health_monitor_node"), "--ros-args",
            "-p", "health_monitor.nodes:=[{}]".format(", ".join(NAV2_NODES)),
            *probe_args,
            "-p", "health_monitor.restart_enabled:=false",  # 只报告不处置
        ],
        "supervisor.nav2.cmd": [ROS2, "launch", "ros2_robot_middleware",
                                "nav2_localized.launch.py"],
        "supervisor.nav2.health_nodes": NAV2_NODES,
        "supervisor.nav2.startup_timeout_s": 120.0,  # 整栈冷启动（含 AMCL 初始化）
        "supervisor.nav2.backoff_base_ms": 2000,
        "supervisor.nav2.max_restarts": 5,
        "supervisor.nav2.window_s": 600.0,
    }

    return LaunchDescription([
        Node(package="ros2_robot_middleware", executable="amr_supervisor",
             name="supervisor", parameters=[p], output="screen"),
    ])
