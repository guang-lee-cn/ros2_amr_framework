#!/usr/bin/env python3
"""蛇形/转圈巡图 — 控制 AMR 覆盖 warehouse 内部让 LiDAR 建图。

warehouse 第一堵墙在 x=3（横跨 y∈[-2,2]），车限制在 x<2.5 区域。
策略：车依次到几个位置，每个位置原地转 1 圈 —— 360° LiDAR 全向扫周围，
即使转向开环有漂移也能扫全。

用法：source install/setup.bash && python3 scripts/patrol.py
"""

import math
import time

import rclpy
from geometry_msgs.msg import Twist
from rclpy.node import Node


class Patrol(Node):
    def __init__(self):
        super().__init__("patrol")
        self.pub = self.create_publisher(Twist, "/cmd_vel", 10)
        self.speed = 0.4        # m/s
        self.turn_rate = 0.8    # rad/s

    def _move(self, lin, ang, sec):
        msg = Twist()
        msg.linear.x = lin
        msg.angular.z = ang
        self.pub.publish(msg)
        time.sleep(sec)
        stop = Twist()
        self.pub.publish(stop)
        time.sleep(0.2)

    def forward(self, dist):
        self._move(self.speed, 0.0, dist / self.speed)

    def spin(self, revs):
        self._move(0.0, self.turn_rate, 2 * math.pi * revs / self.turn_rate)

    def patrol(self):
        self.get_logger().info("patrol start")
        # 依次到 3 个位置，每个原地转 1 圈（LiDAR 全向扫）
        target_x = [0.0, 1.5, 2.2]
        for i, x in enumerate(target_x):
            if i > 0:
                self.forward(x - target_x[i - 1])
            self.spin(1.0)      # 原地转 360°
        self.get_logger().info("patrol done")


def main():
    rclpy.init()
    p = Patrol()
    p.patrol()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
