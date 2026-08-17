#!/usr/bin/env python3
"""factory_3c 障碍 MarkerArray（墙+料架+机台）— Foxglove 可视化。

gz world 的 static 障碍不发 ros 话题，Foxglove 看不到；此节点周期发 MarkerArray
（/obstacles），让 Foxglove 3D 面板显示固定障碍布局（对标 scene_simulator 的 /obstacles）。
位置与 worlds/factory_3c.sdf 严格一致。
"""
import rclpy
from rclpy.node import Node
from visualization_msgs.msg import MarkerArray, Marker


def _box(id_: int, x: float, y: float, sx: float, sy: float, sz: float,
         r: float, g: float, b: float) -> Marker:
    m = Marker()
    m.header.frame_id = "map"
    m.ns = "factory_3c"
    m.id = id_
    m.type = Marker.CUBE
    m.action = Marker.ADD
    m.pose.position.x = float(x)
    m.pose.position.y = float(y)
    m.pose.position.z = float(sz) / 2.0
    m.pose.orientation.w = 1.0
    m.scale.x = float(sx); m.scale.y = float(sy); m.scale.z = float(sz)
    m.color.r = float(r); m.color.g = float(g); m.color.b = float(b); m.color.a = 0.6
    return m


class FactoryMarkers(Node):
    def __init__(self):
        super().__init__("factory_markers")
        self.pub = self.create_publisher(MarkerArray, "/obstacles", 10)
        self.create_timer(1.0, self._publish)

    def _publish(self) -> None:
        ma = MarkerArray()
        _now = self.get_clock().now().to_msg()
        # 墙（灰）— 车间边界 20×12
        ma.markers.append(_box(0, 0, 0, 0.2, 12, 2, 0.5, 0.5, 0.5))      # west x=0
        ma.markers.append(_box(1, 20, 0, 0.2, 12, 2, 0.5, 0.5, 0.5))     # east x=20
        ma.markers.append(_box(2, 10, -6, 20, 0.2, 2, 0.5, 0.5, 0.5))    # south y=-6
        ma.markers.append(_box(3, 10, 6, 20, 0.2, 2, 0.5, 0.5, 0.5))     # north y=6
        # 料架（棕）— 3 排 6m×0.5m，x 中心 7（x=4-10），y=2.2/0/-2.2（通道 1.2m）
        ma.markers.append(_box(4, 7, 2.2, 6, 0.5, 1, 0.6, 0.5, 0.3))
        ma.markers.append(_box(5, 7, 0, 6, 0.5, 1, 0.6, 0.5, 0.3))
        ma.markers.append(_box(6, 7, -2.2, 6, 0.5, 1, 0.6, 0.5, 0.3))
        # 机台（蓝）— 东墙旁，对接 goal 在西侧
        ma.markers.append(_box(7, 18, 4, 1, 1, 1, 0.2, 0.4, 0.6))        # machine1
        ma.markers.append(_box(8, 18, -4, 1, 1, 1, 0.2, 0.4, 0.6))       # machine2
        for _mk in ma.markers:
            _mk.header.stamp = _now
        self.pub.publish(ma)


def main() -> None:
    rclpy.init()
    rclpy.spin(FactoryMarkers())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
