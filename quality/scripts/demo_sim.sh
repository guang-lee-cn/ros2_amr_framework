#!/bin/bash
# AMR 仿真演示 — 前台运行，WSLg 显示 Gazebo + RViz 窗口
#
# 用法:
#   ./quality/scripts/demo_sim.sh          # 完整仿真 + RViz 可视化
#
# 前提:
#   - WSL2 + WSLg (DISPLAY 已设置)
#   - Gazebo Harmonic (ros_gz_sim) + rviz2 已安装
#
# 演示内容:
#   1. Gazebo: warehouse 世界 + AMR 机器人 (物理传感器)
#   2. RViz:   LiDAR 扫描 / 规划路径 / 机器人位姿 / 感知物体
#
# 按 Ctrl-C 退出

set -e

# ROS2 workspace 根目录（脚本放在 <ws>/src/ros2_amr_framework/quality/scripts/）
WS_DIR="$(cd "$(dirname "$0")/../../../.." && pwd)"
cd "$WS_DIR"
source /opt/ros/jazzy/setup.bash
source install/setup.bash

echo "=============================================="
echo " AMR 仿真演示"
echo " Gazebo (物理世界) + RViz (数据可视化)"
echo "=============================================="
echo " 演示数据流:"
echo "   Gazebo 传感器 → bridge → /sensor/*"
echo "   → fusion/decision/motor → /perception/objects"
echo "   → /planning/path → /odom"
echo ""
echo " 启动 Gazebo + 业务管线..."
ros2 launch ros2_robot_middleware simulation.launch.py &
SIM_PID=$!

# 等仿真起来
echo " 等待仿真就绪 (15s)..."
sleep 15

echo " 启动 RViz 可视化..."
rviz2 -d config/amr.rviz &
RVIZ_PID=$!

echo ""
echo " ✅ 演示运行中。查看窗口:"
echo "   - Gazebo Sim: 物理世界 + AMR 机器人"
echo "   - RViz:       LiDAR 点云 / 路径 / 位姿"
echo ""
echo " 按 Ctrl-C 停止所有进程"

# 前台等待，任一进程退出则清理
trap 'echo " 清理中..."; kill $SIM_PID $RVIZ_PID 2>/dev/null; exit 0' INT TERM
wait
