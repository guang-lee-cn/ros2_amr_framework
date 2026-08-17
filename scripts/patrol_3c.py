#!/usr/bin/env python3
"""3C 车间送料往返 patrol：起点 → 机台1 → 机台2 → 起点 → 循环。

订阅 /amcl_pose，到当前 goal（距离 < 0.5m）后 publish /goal_pose 切下一个 goal。
decision 订阅 /goal_pose → set_parameter goal_x/y → A* 重规划 + dispatch。
原 ros2 param set /decision 失败（lifecycle node base interface add executor 不暴露
param server），改 topic 绕开。
"""
import math

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseWithCovarianceStamped, PoseStamped

GOALS = [
    (17.0, 4.0, "machine1"),
    (17.0, -4.0, "machine2"),
    (1.0, 0.0, "home"),
]
ARRIVE_DIST = 0.5


class Patrol3C(Node):
    def __init__(self):
        super().__init__("patrol_3c")
        self.idx = 0
        self.goal_pub_ = self.create_publisher(PoseStamped, "/goal_pose", 10)
        self.create_subscription(PoseWithCovarianceStamped, "/amcl_pose", self._on_pose, 10)
        self._set_goal(GOALS[0])

    def _on_pose(self, msg: PoseWithCovarianceStamped) -> None:
        px = msg.pose.pose.position.x
        py = msg.pose.pose.position.y
        gx, gy, name = GOALS[self.idx]
        if math.hypot(px - gx, py - gy) < ARRIVE_DIST:
            self.get_logger().info(f"reached {name} ({px:.2f},{py:.2f}) → next")
            self.idx = (self.idx + 1) % len(GOALS)
            self._set_goal(GOALS[self.idx])

    def _set_goal(self, goal) -> None:
        gx, gy, name = goal
        msg = PoseStamped()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = "map"
        msg.pose.position.x = float(gx)
        msg.pose.position.y = float(gy)
        msg.pose.orientation.w = 1.0
        self.goal_pub_.publish(msg)
        self.get_logger().info(f"set goal → {name} ({gx},{gy})")


def main() -> None:
    rclpy.init()
    rclpy.spin(Patrol3C())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
