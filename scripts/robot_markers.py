#!/usr/bin/env python3
"""车体 MarkerArray（底盘+lidar+双轮）锚 amr/chassis — Foxglove 看车在哪。

复用 scene_simulator build_robot_markers 的几何（frame amr/chassis，Foxglove 用 /tf
把 marker 摆到车位置）。gz world 的 amr.sdf 模型 Foxglove 看不到（无 ros 话题），
此节点补车体可视化。
"""
import math
import rclpy
from rclpy.node import Node
from visualization_msgs.msg import MarkerArray, Marker

# 轮子绕 x 轴 π/2（cylinder 轴 Z→Y，对齐轮轴）: quaternion (sin(π/4), 0, 0, cos(π/4))
_S = math.sin(math.pi / 4.0)
_C = math.cos(math.pi / 4.0)


def _m(id_: int, mtype: int, x: float, y: float, z: float,
       sx: float, sy: float, sz: float, r: float, g: float, b: float,
       qx: float = 0.0, qy: float = 0.0, qz: float = 0.0, qw: float = 1.0) -> Marker:
    m = Marker()
    m.header.frame_id = "amr/chassis"
    m.ns = "robot"
    m.id = id_
    m.type = mtype
    m.action = Marker.ADD
    m.pose.position.x = float(x); m.pose.position.y = float(y); m.pose.position.z = float(z)
    m.pose.orientation.x = float(qx); m.pose.orientation.y = float(qy)
    m.pose.orientation.z = float(qz); m.pose.orientation.w = float(qw)
    m.scale.x = float(sx); m.scale.y = float(sy); m.scale.z = float(sz)
    m.color.r = float(r); m.color.g = float(g); m.color.b = float(b); m.color.a = 1.0
    return m


class RobotMarkers(Node):
    def __init__(self):
        super().__init__("robot_markers")
        self.pub = self.create_publisher(MarkerArray, "/robot_model", 10)
        self.create_timer(0.5, self._publish)

    def _publish(self) -> None:
        ma = MarkerArray()
        _now = self.get_clock().now().to_msg()
        # 底盘 box 0.6×0.4×0.2（中心离地 0.1m）蓝
        ma.markers.append(_m(0, Marker.CUBE, 0, 0, 0.10, 0.6, 0.4, 0.2, 0.20, 0.35, 0.90))
        # lidar 圆柱 r0.05 h0.05，挂点 (0.25,0,0.30) 黑
        ma.markers.append(_m(1, Marker.CYLINDER, 0.25, 0, 0.30, 0.10, 0.10, 0.05, 0.15, 0.15, 0.15))
        # 双轮 r0.075 宽0.04，挂点 (0,±0.25,-0.05)，轴沿 y 黑
        ma.markers.append(_m(2, Marker.CYLINDER, 0, 0.25, -0.05, 0.15, 0.15, 0.04, 0.10, 0.10, 0.10, qx=_C, qw=_S))
        ma.markers.append(_m(3, Marker.CYLINDER, 0, -0.25, -0.05, 0.15, 0.15, 0.04, 0.10, 0.10, 0.10, qx=_C, qw=_S))
        for _mk in ma.markers:
            _mk.header.stamp = _now
        self.pub.publish(ma)


def main() -> None:
    rclpy.init()
    rclpy.spin(RobotMarkers())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
