#!/usr/bin/env python3
"""闭环巡图 — 订阅 /odom 反馈，比例控制车依次到达目标点。

解决开环速度-时间控制漂移问题：每步读真实 /odom 位姿，先转向对齐目标方向，
再前进，到位（<0.2m）切下一目标。warehouse 第一堵墙在 x=3，目标限制 x<2.5。

用法：source install/setup.bash && python3 scripts/patrol_closedloop.py
"""

import math
import threading

import rclpy
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from rclpy.node import Node

# 覆盖 warehouse x<3 区域（墙前）的目标点，蛇形覆盖 y∈[-1.5,1.5]
TARGETS = [
    (0.5, 0.0), (0.5, 1.5), (1.2, 1.5), (1.2, -1.5),
    (1.9, -1.5), (1.9, 1.5), (2.4, 1.5), (2.4, -1.5), (2.4, 0.0),
]

MAX_SPEED = 0.5       # m/s
MAX_TURN = 1.0        # rad/s
ARRIVE_DIST = 0.18    # m
YAW_THRESHOLD = 0.15  # rad


class ClosedLoopPatrol(Node):
    def __init__(self):
        super().__init__("patrol_closedloop")
        self.pub = self.create_publisher(Twist, "/cmd_vel", 10)
        self.sub = self.create_subscription(Odometry, "/odom", self.on_odom, 10)
        self.pose = None
        self.spin_rate = self.create_rate(20)

    def on_odom(self, msg):
        q = msg.pose.pose.orientation
        yaw = math.atan2(2 * (q.w * q.z + q.x * q.y),
                         1 - 2 * (q.y * q.y + q.z * q.z))
        self.pose = (msg.pose.pose.position.x,
                     msg.pose.pose.position.y, yaw)

    def _cmd(self, lin, ang):
        msg = Twist()
        msg.linear.x = lin
        msg.angular.z = ang
        self.pub.publish(msg)

    def _stop(self):
        self._cmd(0.0, 0.0)

    def _wait_pose(self, timeout_s=10.0):
        start = self.get_clock().now()
        while self.pose is None and self.get_clock().now() - start < \
                rclpy.duration.Duration(seconds=timeout_s):
            self.spin_rate.sleep()

    def goto(self, tx, ty):
        while rclpy.ok():
            if self.pose is None:
                self._wait_pose()
                continue
            x, y, yaw = self.pose
            dx, dy = tx - x, ty - y
            dist = math.hypot(dx, dy)
            if dist < ARRIVE_DIST:
                self._stop()
                return
            target_yaw = math.atan2(dy, dx)
            yaw_err = math.atan2(math.sin(target_yaw - yaw),
                                 math.cos(target_yaw - yaw))
            if abs(yaw_err) > YAW_THRESHOLD:
                # 优先转向对齐目标方向
                ang = max(-MAX_TURN, min(MAX_TURN, 2.0 * yaw_err))
                self._cmd(0.0, ang)
            else:
                ang = max(-MAX_TURN, min(MAX_TURN, 2.0 * yaw_err))
                lin = min(MAX_SPEED, max(0.0, dist * 2.0))  # 减速到位
                self._cmd(lin, ang)
            self.spin_rate.sleep()
        self._stop()

    def patrol(self):
        self._wait_pose()
        self.get_logger().info(f"patrol start, pose={self.pose}")
        for i, (tx, ty) in enumerate(TARGETS):
            self.goto(tx, ty)
            self.get_logger().info(f"[{i + 1}/{len(TARGETS)}] reached ({tx},{ty})")
        self._stop()
        self.get_logger().info(f"patrol done, final pose={self.pose}")


def main():
    rclpy.init()
    p = ClosedLoopPatrol()
    # 独立线程 spin executor，控制循环才能收到 /odom 回调
    executor = rclpy.executors.MultiThreadedExecutor()
    executor.add_node(p)
    thread = threading.Thread(target=executor.spin, daemon=True)
    thread.start()
    p.patrol()
    executor.shutdown()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
