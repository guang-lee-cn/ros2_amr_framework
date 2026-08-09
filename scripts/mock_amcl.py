#!/usr/bin/env python3
"""Mock AMCL — gz /odom 完美定位 → /amcl_pose(frame=map) + TF map→amr/odom identity。

仿真用（gz DiffDrive 的 /odom 即真实位姿，map≡amr/odom 对齐）；产品级换真 AMCL
（有定位噪声/累积漂移）。补 decision 的 A* 起点 + dispatch 坐标变换所需 map 帧。
"""
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseWithCovarianceStamped, TransformStamped
from nav_msgs.msg import Odometry
from tf2_ros import TransformBroadcaster


class MockAmcl(Node):
    def __init__(self):
        super().__init__('mock_amcl')
        self.amcl_pub = self.create_publisher(PoseWithCovarianceStamped, '/amcl_pose', 10)
        self.tf_broad = TransformBroadcaster(self)
        self.create_subscription(Odometry, '/odom', self.on_odom, 10)

    def on_odom(self, msg: Odometry) -> None:
        stamp = msg.header.stamp if msg.header.stamp.sec > 0 else self.get_clock().now().to_msg()
        a = PoseWithCovarianceStamped()
        a.header.stamp = stamp
        a.header.frame_id = 'map'
        a.pose.pose = msg.pose.pose
        self.amcl_pub.publish(a)
        t = TransformStamped()
        t.header.stamp = stamp
        t.header.frame_id = 'map'
        t.child_frame_id = 'amr/odom'
        t.transform.rotation.w = 1.0
        self.tf_broad.sendTransform(t)


def main() -> None:
    rclpy.init()
    rclpy.spin(MockAmcl())
    rclpy.shutdown()


if __name__ == '__main__':
    main()
