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
        # 死区扇区虚拟近障距离(20260818 穿货架复发根因): 扇区失明恰覆盖
        # FOV 方向时 nearest=inf → guard 按"空旷"放行全速, 车盲穿货架
        # (总回波 ~160 绕过 min_valid_echoes 全向检查)。感知不可信的方位
        # 保守处理: 死区 beams 置 0.30m(< guard stop_dist 0.40 必硬停)。
        # 必须小于 min_valid_range? 否——此处赋值在滤除之后, 不会被滤掉。
        self.declare_parameter('dead_sector_range', 0.30)
        self.dead_range = self.get_parameter('dead_sector_range').value
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
        # 扇区死区处置(20260818): 死区扇区回波置虚拟近障(0.30m)。
        # 扇区覆盖 FOV 时 guard nearest 恒 inf → "空旷"放行全速穿货架
        # (总回波 ~160 绕过 min_valid_echoes 全向检查)。虚拟近障 < guard
        # stop_dist(0.40) 必硬停, 且 raytrace 会标进 grid → A* 不派穿
        # 死区路径; 扇区恢复自动解除。仅系统性失明(alive<6, 同告警判据)
        # 时处置 —— alive≥6 的个别自然稀疏扇区(如车间对角 >range_max)
        # 不误置。全盲时 8 扇区全死 → 全向虚拟墙, 行为=硬停等 watchdog。
        sectors = [0] * 8
        sector_all_beams = [[] for _ in range(8)]
        for i, r in enumerate(msg.ranges):
            a = math.degrees(msg.angle_min + i * msg.angle_increment)
            s = int(((a + 180) % 360) // 45)
            sector_all_beams[s].append(i)      # 全 beams 分桶: 死区要整扇区封锁
            if 0.01 < r < msg.range_max:       # 有效回波才计入存活统计
                sectors[s] += 1
        alive = sum(1 for v in sectors if v >= 3)
        dead = [i for i, v in enumerate(sectors) if v < 3]
        if alive < 6:
            self._dead_sector_streak += 1
            for s in dead:
                for j in sector_all_beams[s]:  # 含 inf beams: 整扇区封锁,
                    msg.ranges[j] = self.dead_range  # 否则 A* 从 inf 空隙穿死区
            if self._dead_sector_streak % 50 == 1:
                self.get_logger().warn(
                    f'传感器扇区失明: 仅 {alive}/8 方位有回波(死区中心角 '
                    f'{[i * 45 - 180 for i in dead]}°), 已置虚拟近障 {self.dead_range}m '
                    f'封锁死区方位, 建议重启仿真!')
        else:
            self._dead_sector_streak = 0
        self.pub.publish(msg)


def main() -> None:
    rclpy.init()
    rclpy.spin(ScanFilter())
    rclpy.shutdown()


if __name__ == '__main__':
    main()
