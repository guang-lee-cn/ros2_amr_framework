#!/usr/bin/env python3
"""ab_runner.py — A/B 跑台驱动器（四审工件入库版，原 /tmp 副本三度被清理）。

同场景同目标同判定（/odom 0.3m + 静止 2s 内为到达）驱动两侧：
  nav2 模式: /navigate_to_pose action goal（站间冷却 2.5s 防 BT 未重置
             假 SUCCEEDED——0.5s 连发实证过 0.57s 假成功；12s 无位移重发）
  custom 模式: /goal_pose 话题（自研栈决策入口，多发 3 次保证送达）

用法（手册 doc/subsystems/nav2-stack.md §七）:
  Side A: ros2 launch ros2_robot_middleware nav2_scene.launch.py scene:=rack_3c
          python3 toolkit/ab/ab_runner.py nav2
  Side B: ros2 launch ros2_robot_middleware ab_custom.launch.py
          python3 toolkit/ab/ab_runner.py custom
基线数据（2026-09-03 rack_3c 四站）: NAV2 4/4 · 48.8s · 96% 里程效率；
自研栈 0/4 · 287.9s（货架角冻结重规划 128 次）。
"""
import math
import sys
import time

import rclpy
from nav_msgs.msg import Odometry
from rclpy.action import ActionClient
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data

from geometry_msgs.msg import PoseStamped
from nav2_msgs.action import NavigateToPose

LEGS = [(17.0, 0.0), (8.0, 3.8), (17.0, -2.2), (2.0, 0.0)]
MODE = sys.argv[1] if len(sys.argv) > 1 else "nav2"


class Runner(Node):
    def __init__(self):
        super().__init__('ab_runner')
        self.create_subscription(Odometry, '/odom', self.cb, qos_profile_sensor_data)
        self.pose = None
        self.speed = 0.0
        self.goal_pub = None
        self.ac = None
        if MODE == "nav2":
            self.ac = ActionClient(self, NavigateToPose, '/navigate_to_pose')
            while not self.ac.wait_for_server(timeout_sec=2.0) and rclpy.ok():
                pass
        else:
            self.goal_pub = self.create_publisher(PoseStamped, '/goal_pose', 10)

    def cb(self, m):
        self.pose = (m.pose.pose.position.x, m.pose.pose.position.y)
        self.speed = math.hypot(m.twist.twist.linear.x, m.twist.twist.linear.y)

    def arrived(self, gx, gy):
        return (self.pose is not None and
                math.hypot(self.pose[0] - gx, self.pose[1] - gy) < 0.3 and
                self.speed < 0.05)

    def spin_for(self, s):
        t0 = time.time()
        while time.time() - t0 < s and rclpy.ok():
            rclpy.spin_once(self)


def main():
    rclpy.init()
    n = Runner()
    n.spin_for(3.0)
    print(f"=== A/B run mode={MODE} legs={LEGS} ===", flush=True)
    total = 0.0
    for i, (gx, gy) in enumerate(LEGS):
        time.sleep(2.5)  # 站间冷却：BT 重置 + goal checker 状态清理
        t0 = time.time()
        if MODE == "nav2":
            goal = NavigateToPose.Goal()
            goal.pose.header.frame_id = 'map'
            goal.pose.pose.position.x = gx
            goal.pose.pose.position.y = gy
            goal.pose.pose.orientation.w = 1.0
            n.ac.send_goal_async(goal)
            n.spin_for(1.0)
        else:
            for _ in range(3):
                g = PoseStamped()
                g.header.frame_id = 'map'
                g.pose.position.x = gx
                g.pose.position.y = gy
                g.pose.orientation.w = 1.0
                n.goal_pub.publish(g)
                n.spin_for(0.2)
        last_resend = 0.0
        while time.time() - t0 < 90 and rclpy.ok() and not n.arrived(gx, gy):
            n.spin_for(0.1)
            # 假成功/丢目标重发（三轮观测教训：无节流时每 0.1s 重发=机枪，
            # 每个新目标抢占上一个造成 Begin 假象 + CPU 负载反馈恶化——
            # 节流 15s 一次）
            if (MODE == "nav2" and time.time() - t0 > 12 and n.speed < 0.01
                    and time.time() - last_resend > 15):
                n.ac.send_goal_async(goal)
                last_resend = time.time()
                n.spin_for(1.0)
        status = "ARRIVED" if n.arrived(gx, gy) else "TIMEOUT"
        dt = time.time() - t0
        total += dt
        print(f"leg{i+1} ({gx},{gy}): {status} {dt:.1f}s", flush=True)
    print(f"TOTAL {total:.1f}s", flush=True)


if __name__ == '__main__':
    main()
