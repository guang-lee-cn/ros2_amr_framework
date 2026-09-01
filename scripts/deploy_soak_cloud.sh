#!/bin/bash
# deploy_soak_cloud.sh — 云服务器一键部署 + 启动 72h soak（scene_simulator 形态）
# 适用：阿里云 ECS e-c1m1.large（2C2G Ubuntu 22.04）
# 用法：scp 到服务器后 bash 执行；或直接在 SSH 终端粘贴整段
set -eo pipefail

echo "══════════ 1/4 安装 ROS2 Jazzy（~3 分钟）══════════"
sudo apt-get update -qq
sudo apt-get install -y -qq curl gnupg lsb-release git build-essential python3-pip
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
  -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" \
  | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null
sudo apt-get update -qq
sudo apt-get install -y -qq ros-jazzy-ros-base ros-jazzy-nav-msgs \
  ros-jazzy-tf2-ros ros-jazzy-robot-state_publisher \
  python3-colcon-common-extensions

echo "══════════ 2/4 克隆仓库（~30 秒）══════════"
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src
if [ ! -d ros2_amr_framework ]; then
  git clone --depth 1 https://github.com/guang-lee-cn/ros2_amr_framework.git
fi

echo "══════════ 3/4 构建（~3 分钟）══════════"
cd ~/ros2_ws
source /opt/ros/jazzy/setup.bash
colcon build --packages-select ros2_robot_middleware \
  --cmake-args -DCMAKE_BUILD_TYPE=Release -DCMAKE_BUILD_PARALLEL_LEVEL=2 \
  2>&1 | tail -3
source install/setup.bash

echo "══════════ 4/4 启动 72h soak ══════════"
cd ~/ros2_ws/src/ros2_amr_framework
export SCENE_MODE=scene
export VICTIMS=scene_simulator,compute
export INJECT_INTERVAL_MIN=30
export DURATION_MIN=4320
nohup ./scripts/soak_run.sh > /tmp/soak.log 2>&1 &
SOAK_PID=$!
echo ""
echo "════════════════════════════════════════════════"
echo "  72h soak 已启动（PID $SOAK_PID）"
echo "  日志: tail -f /tmp/soak.log"
echo "  数据: ls /tmp/amr_soak_*/"
echo "  72h 后报告: /tmp/amr_soak_*/summary.md"
echo "  停止: kill $SOAK_PID && pkill -f amr_supervisor"
echo "════════════════════════════════════════════════"
