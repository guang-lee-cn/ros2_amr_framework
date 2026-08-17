#!/usr/bin/env python3
"""仿真专用 /scan 过滤中继 — 滤掉 WSL2 gpu_lidar 近距伪影回波。

背景(2026-08-16 排障记录, 证据见 docs/change_journal.md):
  gz gpu_lidar 在本 WSL2 环境(headless EGL)输出中存在 0.2-0.3m 的近距幻影弧,
  会进入 CollisionGuard 的 ±45° FOV 触发误停车死锁。
  区分实验证明: 改传感器参数(min_range/安装高度)会令渲染静默失效(全 inf),
  因此只能在桥接层过滤。

分层(责任分层规则):
  本节点属于仿真资产层, 仅 simulation.launch.py 启动; 真机 launch 不含。
  大脑(motor_ctrl/decision)零改动, 订阅的 /scan 语义不变。

数据流: ros_gz bridge /scan_raw → 本节点滤 r<min_valid → /scan
阈值依据: 车身轮廓对角 0.32m(激光前伸 0.25 + 半宽 0.20) + 0.03m 裕量。
"""
import math

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import LaserScan


class ScanFilter(Node):
    def __init__(self):
        super().__init__('scan_filter')
        # 阈值外置为参数: 仿真/车型变化时改 launch 传参, 不改代码
        self.declare_parameter('min_valid_range', 0.35)
        self.min_valid = self.get_parameter('min_valid_range').value
        self.pub = self.create_publisher(LaserScan, '/scan', 10)
        self.create_subscription(LaserScan, '/scan_raw', self._relay, 10)
        self._blind_streak = 0
        self._dead_sector_streak = 0
        self.get_logger().info(f'scan_filter: 滤除 <{self.min_valid}m 回波 (/scan_raw → /scan)')

    def _relay(self, msg: LaserScan) -> None:
        filtered = 0
        for i, r in enumerate(msg.ranges):
            if r < self.min_valid:          # 含 0.0/近距伪影; inf/nan 比较结果为 False, 不受影响
                msg.ranges[i] = float('inf')
                filtered += 1
        self.pub.publish(msg)
        if filtered:
            self.get_logger().debug(f'filtered {filtered} beams')
        # 盲启动/运行中致盲检测: 连续多帧全 inf 则大声告警(WSL2 gpu_lidar 已知劣化模式)
        any_valid = any(r < self.min_valid + 100.0 and r == r and r != float('inf') for r in msg.ranges)
        if not any_valid:
            self._blind_streak += 1
            if self._blind_streak % 50 == 1:   # 10Hz 下约每5s 一条
                self.get_logger().warn(
                    f'传感器疑似失明: 连续 {self._blind_streak} 帧无有效回波 '
                    f'(WSL2 gpu_lidar 劣化, 建议 ./scripts/run_sim.sh 重启抽签)')
        else:
            self._blind_streak = 0
        self._dead_sector_streak = 0
        # 扇区死区检测: 8x45° 中存活扇区 <6 → 大范围方位失明(2026-08-16 穿货架事故根因:
        # 半平面 0 回波但总数仍 ~160, 计数门槛放行 → 车对死区方向障碍完全无视)
        sectors = [0] * 8
        for i, r in enumerate(msg.ranges):
            if 0.01 < r < msg.range_max:
                a = math.degrees(msg.angle_min + i * msg.angle_increment)
                sectors[int(((a + 180) % 360) // 45)] += 1
        alive = sum(1 for v in sectors if v >= 3)
        if alive < 6:
            self._dead_sector_streak += 1
            if self._dead_sector_streak % 50 == 1:
                dead = [i * 45 - 180 for i, v in enumerate(sectors) if v < 3]
                self.get_logger().warn(
                    f'传感器扇区失明: 仅 {alive}/8 方位有回波(死区中心角 {dead}°), '
                    f'该方向障碍不可见, 建议重启仿真!')
        else:
            self._dead_sector_streak = 0


def main() -> None:
    rclpy.init()
    rclpy.spin(ScanFilter())
    rclpy.shutdown()


if __name__ == '__main__':
    main()
