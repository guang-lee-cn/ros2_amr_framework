#!/bin/bash
# AMR 演示 — Simulated 传感器 + 场景障碍物 + Foxglove 可视化
#
# 不用 Gazebo（WSLg 渲染传感器问题，已标记后续解决）。
# SimulatedLidar 按场景配置生成含障碍物的点云 → 感知/避障 → Foxglove 显示。
#
# 用法:
#   ./quality/scripts/demo_sim_scenario.sh [obstacle|slalom|corridor]
#   （默认 obstacle）
#
# 浏览器: ws://172.24.232.68:8765 (Foxglove WebSocket)

set -e

SCENARIO="${1:-obstacle}"

WS_DIR="$(cd "$(dirname "$0")/../../../.." && pwd)"
cd "$WS_DIR"
source /opt/ros/jazzy/setup.bash
source install/setup.bash

echo "=============================================="
echo " AMR 演示 (Simulated + 场景: $SCENARIO)"
echo "=============================================="

# 清理残留
pkill -9 -f "compute_container" 2>/dev/null || true
pkill -9 -f "foxglove_bridge" 2>/dev/null || true
pkill -9 -f "lidar_node" 2>/dev/null || true
pkill -9 -f "imu_node" 2>/dev/null || true
pkill -9 -f "health_monitor" 2>/dev/null || true
rm -f /dev/shm/fastrtps_* 2>/dev/null || true
sleep 2

echo " 启动业务管线 (场景: $SCENARIO, Simulated 传感器)..."
# compute_container 内 FusionNode 用 Simulated 传感器 + scenario 障碍物，
# 直接读数据（不走 topic），无需独立 sensor 节点。
ros2 run ros2_robot_middleware compute_container \
  --ros-args -p scenario:="$SCENARIO" > /tmp/scenario_demo.log 2>&1 &
SIM_PID=$!
sleep 8

echo " 启动 Foxglove Bridge (:8765)..."
ros2 run foxglove_bridge foxglove_bridge > /tmp/scenario_fox.log 2>&1 &
BRIDGE_PID=$!
sleep 3

echo ""
echo " ✅ 演示运行中。"
echo "    浏览器 Foxglove: ws://172.24.232.68:8765"
echo "    3D 面板订阅:"
echo "      - /sensor/lidar      (红色点云 — 障碍物轮廓)"
echo "      - /planning/path     (绿色路径 — A* 绕障)"
echo "      - /odom              (蓝色位姿 — 机器人移动)"
echo ""
echo " 按 Ctrl-C 停止"

trap 'echo " 清理中..."; kill $SIM_PID $BRIDGE_PID 2>/dev/null; exit 0' INT TERM
wait
