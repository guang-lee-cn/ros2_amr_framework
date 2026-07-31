#!/bin/bash
# AMR 仿真演示 — Foxglove 可视化版
#
# Gazebo headless (物理+传感器，省 CPU) → ROS2 topics
# → foxglove_bridge (WebSocket :8765) → Foxglove Studio (浏览器)
#
# 用法:
#   ./quality/scripts/demo_sim_foxglove.sh
#
# 前提:
#   - ros-jazzy-foxglove-bridge 已安装
#   - 浏览器打开 http://localhost:8765 (Foxglove WebSocket)
#
# 按 Ctrl-C 退出

set -e

WS_DIR="$(cd "$(dirname "$0")/../../../.." && pwd)"
cd "$WS_DIR"
source /opt/ros/jazzy/setup.bash
source install/setup.bash

echo "=============================================="
echo " AMR 仿真演示 (Foxglove 可视化)"
echo " Gazebo headless (省 CPU) → Foxglove (浏览器)"
echo "=============================================="

# 清理残留
pkill -9 -f "gz sim" 2>/dev/null || true
pkill -9 -f "foxglove_bridge" 2>/dev/null || true
pkill -9 -f "ros_gz_bridge" 2>/dev/null || true
pkill -9 -f "compute_container" 2>/dev/null || true
rm -f /dev/shm/fastrtps_* 2>/dev/null || true
sleep 2

echo " 启动 Gazebo (headless) + 业务管线..."
ros2 launch ros2_robot_middleware simulation.launch.py &
SIM_PID=$!
sleep 18

echo " 启动 Foxglove Bridge (:8765)..."
ros2 run foxglove_bridge foxglove_bridge &
BRIDGE_PID=$!
sleep 3

echo ""
echo " ✅ 仿真运行中。"
echo "    浏览器打开: http://localhost:8765"
echo "    或: Foxglove Studio → Connect → WebSocket → localhost:8765"
echo ""
echo "    推荐布局:"
echo "      - LaserScan → 3D 面板 (红色点云)"
echo "      - /planning/path → 3D 面板 (绿色路径)"
echo "      - /odom → 3D 面板 (蓝色位姿)"
echo "      - /perception/objects → 3D 面板 (物体标记)"
echo ""
echo " 按 Ctrl-C 停止所有进程"

trap 'echo " 清理中..."; kill $SIM_PID $BRIDGE_PID 2>/dev/null; exit 0' INT TERM
wait
