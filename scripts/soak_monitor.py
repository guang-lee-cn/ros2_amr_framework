#!/usr/bin/env python3
"""soak_monitor.py — soak 期间的业务观测采样器（与 soak_run.sh 配套）。

订阅 /goal_pose（吞吐事件）、/amcl_pose（位姿）、/cmd_vel（运动活性），
写两类 CSV 供 soak_report.py 汇总：
  goals.csv   — 每个 /goal_pose 一行：ts,goal_x,goal_y
  monitor.csv — 每 5s 一行：ts,pose_x,pose_y,cmd_vel_hz,amcl_hz

设计约束：独立于仿真栈的常驻进程——栈被 kill/重启后 DDS 断连自动恢复，
patrol 重启即重发首个 goal（goals.csv 出现新事件）——soak_run.sh 以
「scan 健康 + 注入后新 goal 事件」作为业务恢复判据。
"""
import os
import sys
import time

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseStamped, PoseWithCovarianceStamped, Twist


class SoakMonitor(Node):
    def __init__(self, outdir: str):
        super().__init__("soak_monitor")
        self.cmd_count = 0
        self.pose_count = 0
        self.pose = (float("nan"), float("nan"))
        self.last_window = time.time()
        self.goals_f = open(os.path.join(outdir, "goals.csv"), "a", buffering=1)
        self.mon_f = open(os.path.join(outdir, "monitor.csv"), "a", buffering=1)
        self.goals_f.write("ts,goal_x,goal_y\n")
        self.mon_f.write("ts,pose_x,pose_y,cmd_vel_hz,amcl_hz\n")
        # 壁钟时间（不用仿真 /clock）——soak 度量的是真实世界时长
        self.create_subscription(PoseStamped, "/goal_pose", self._on_goal, 20)
        self.create_subscription(PoseWithCovarianceStamped, "/amcl_pose", self._on_pose, 10)
        self.create_subscription(Twist, "/cmd_vel", self._on_cmd, 50)
        self.create_timer(5.0, self._tick)
        self.get_logger().info(f"soak_monitor 采样中 → {outdir}")

    def _on_goal(self, msg: PoseStamped) -> None:
        self.goals_f.write(
            f"{time.time():.1f},{msg.pose.position.x:.2f},{msg.pose.position.y:.2f}\n")

    def _on_pose(self, msg: PoseWithCovarianceStamped) -> None:
        self.pose = (msg.pose.pose.position.x, msg.pose.pose.position.y)
        self.pose_count += 1

    def _on_cmd(self, msg: Twist) -> None:
        self.cmd_count += 1

    def _tick(self) -> None:
        now = time.time()
        win = max(now - self.last_window, 1e-6)
        px, py = self.pose
        self.mon_f.write(
            f"{now:.1f},{px:.3f},{py:.3f},{self.cmd_count / win:.2f},{self.pose_count / win:.2f}\n")
        self.cmd_count = 0
        self.pose_count = 0
        self.last_window = now


def main() -> None:
    outdir = sys.argv[1] if len(sys.argv) > 1 else "/tmp/amr_soak_monitor"
    os.makedirs(outdir, exist_ok=True)
    rclpy.init()
    node = SoakMonitor(outdir)
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
