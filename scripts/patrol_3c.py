#!/usr/bin/env python3
"""3C 车间送料往返 patrol：起点 → 机台1 → 机台2 → 起点 → 循环。

订阅 /amcl_pose，到当前 goal（距离 < 0.5m）后 ros2 param set 切下一个 goal。
decision 收新 goal_x/y → A* 重规划 + dispatch（gate dedup 允许，因每次 goal != last）。
模拟 3C 车间送料闭环：取料 → 送机台1 → 送机台2 → 回起点。
"""
import math
import subprocess

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseWithCovarianceStamped

# goal 序列：(x, y, 名) — 与 factory_3c.sdf 布局一致
GOALS = [
    (17.0, 4.0, "machine1"),   # 机台1（送料）
    (17.0, -4.0, "machine2"),  # 机台2（送料）
    (1.0, 0.0, "home"),        # 回起点（取料）
]
ARRIVE_DIST = 0.5  # 到 goal 判定距离（m）


class Patrol3C(Node):
    def __init__(self):
        super().__init__("patrol_3c")
        self.idx = 0
        self.create_subscription(PoseWithCovarianceStamped, "/amcl_pose", self._on_pose, 10)
        self._set_goal(GOALS[0])  # 起步设机台1

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
        subprocess.run(["ros2", "param", "set", "/decision", "goal_x", str(gx)],
                       check=False, capture_output=True)
        subprocess.run(["ros2", "param", "set", "/decision", "goal_y", str(gy)],
                       check=False, capture_output=True)
        self.get_logger().info(f"set goal → {name} ({gx},{gy})")


def main() -> None:
    rclpy.init()
    rclpy.spin(Patrol3C())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
