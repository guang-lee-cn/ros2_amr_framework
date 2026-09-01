#!/bin/bash
# cloud_soak_all.sh — 云服务器自主 72h soak（自包含，无需人工介入）
# 设计约束：本地电脑断电后，服务器必须自主完成部署→构建→soak→报告全流程。
# 幂等：重启后自动检测已完成的阶段并跳过（build 存在→跳过 build；soak 在跑→不重启）。
set -o pipefail

LOG=/root/soak_autostart.log
SOAK_DATA=/root/soak
CONTAINER=amr_soak
echo "$(date '+%F %T') [autostart] 触发" >> $LOG

# ── Docker 就绪检查 ────────────────────────────────────────────────────
if ! command -v docker &>/dev/null; then
  echo "$(date '+%F %T') 安装 Docker..." >> $LOG
  curl -fsSL https://get.docker.com | sh >> $LOG 2>&1
fi
systemctl start docker 2>/dev/null

# Docker 镜像加速（国内必需）
mkdir -p /etc/docker
if ! grep -q daocloud /etc/docker/daemon.json 2>/dev/null; then
  echo '{"registry-mirrors": ["https://docker.m.daocloud.io", "https://dockerhub.icu"]}' \
    > /etc/docker/daemon.json
  systemctl restart docker
  sleep 3
fi

# ── 幂等：容器在跑 → 检查内部 soak 是否活 → 活则退出 ────────────────────
if docker ps --format '{{.Names}}' | grep -q "^${CONTAINER}$"; then
  # 容器在跑，检查 soak 进程
  if docker exec $CONTAINER pgrep -f soak_run.sh >/dev/null 2>&1; then
    echo "$(date '+%F %T') soak 已在运行，跳过" >> $LOG
    exit 0
  fi
  # 容器在但 soak 死了 → 重启容器（重新进入 soak 启动逻辑）
  echo "$(date '+%F %T') 容器在但 soak 已死，重启容器" >> $LOG
  docker rm -f $CONTAINER
fi

# ── 镜像就绪 ────────────────────────────────────────────────────────────
docker pull ros:jazzy-ros-base >> $LOG 2>&1

# ── 启动容器（挂载数据卷 + 内存限制防 OOM）──────────────────────────────
mkdir -p $SOAK_DATA
docker run -d \
  --name $CONTAINER \
  --shm-size=256m \
  --memory=1400m \
  --memory-swap=1400m \
  -v $SOAK_DATA:/soak_data \
  ros:jazzy-ros-base \
  bash /soak_entry.sh

echo "$(date '+%F %T') 容器已启动" >> $LOG

# ── 容器内脚本：自包含部署 + 构建 + soak（幂等）─────────────────────────
docker exec $CONTAINER bash -c 'cat > /soak_entry.sh << "INNER_EOF"
#!/bin/bash
set -o pipefail
exec > /soak_data/container.log 2>&1
echo "$(date) [container] 启动"

WS=/ws
SRC=$WS/src/ros2_amr_framework

# 幂等：soak 已在跑则退出
if pgrep -f soak_run.sh >/dev/null; then
  echo "soak 已在运行"
  exit 0
fi

# ── 依赖（幂等）────────────────────────────────────────────────────────
if [ ! -f /tmp/deps_done ]; then
  echo "安装构建依赖..."
  apt-get update -qq
  apt-get install -y -qq git build-essential \
    python3-colcon-common-extensions \
    ros-jazzy-hardware-interface ros-jazzy-pluginlib \
    ros-jazzy-lifecycle-msgs ros-jazzy-diagnostic-msgs \
    ros-jazzy-action-msgs ros-jazzy-visualization-msgs \
    ros-jazzy-nav-msgs ros-jazzy-tf2-ros 2>&1 | tail -1
  touch /tmp/deps_done
  echo "依赖安装完成"
fi

# ── 克隆（幂等）────────────────────────────────────────────────────────
if [ ! -d $SRC ]; then
  echo "克隆仓库..."
  mkdir -p $WS/src
  cd $WS/src
  git clone --depth 1 https://github.com/guang-lee-cn/ros2_amr_framework.git
fi

# ── 构建（幂等：install 存在则跳过）─────────────────────────────────────
if [ ! -f $WS/install/setup.bash ]; then
  echo "构建（单线程防 OOM）..."
  cd $WS
  source /opt/ros/jazzy/setup.bash
  CMAKE_BUILD_PARALLEL_LEVEL=1 colcon build \
    --packages-select ros2_robot_middleware \
    --cmake-args ' -DCMAKE_BUILD_TYPE=Release' 2>&1 | tail -3
  echo "构建完成"
else
  echo "构建产物已存在，跳过"
fi

# ── 启动 soak ──────────────────────────────────────────────────────────
source /opt/ros/jazzy/setup.bash
source $WS/install/setup.bash
cd $SRC
export SCENE_MODE=scene
export VICTIMS=scene_simulator,compute
export INJECT_INTERVAL_MIN=30
export DURATION_MIN=4320
export OUT=/soak_data/soak_run

echo "启动 72h soak..."
exec ./scripts/soak_run.sh
INNER_EOF
chmod +x /soak_entry.sh'

echo "$(date '+%F %T') soak_entry.sh 已注入，容器内开始执行" >> $LOG
