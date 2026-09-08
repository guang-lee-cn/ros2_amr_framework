#!/bin/bash
# ── Gazebo 回归运行器（2026-09-08 翻案裁决后的形态）──────────────────
# 裁决：gz-sim 8.14 无 CPU lidar（官方 Wrn 拒绝），渲染死亡不可根治
# （三环境 1-8min 实证）。Gazebo 以"受监管公民"回归：物理保真层，
# 死亡看护自动重启。CPU 场景仿真器仍是 CI/soak/长跑骨干（分层双底座）。
#
# 用法: ./scripts/run_gz_demo.sh [--watchdog]
#   --watchdog  扫描死亡（全 inf/停流 2 次探测）→ 整栈重启（~2min 周期）
set -o pipefail  # 注意无 -u：setup.bash 在 nounset 下会炸
SCRIPT=$(readlink -f "$0")
PKG_DIR=$(dirname "$(dirname "$SCRIPT")")       # scripts/ 的上级 = 仓库根
WS_DIR=$(dirname "$(dirname "$PKG_DIR")")       # 仓库根的上两级 = 工作区
source /opt/ros/jazzy/setup.bash
source "$WS_DIR/install/setup.bash"

probe_alive() {  # 返回 1=扫描健康 0=死亡/无流
  timeout 10 python3 -c "
import rclpy, time
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import LaserScan
rclpy.init(); n = Node('gzwd')
got = [None]
n.create_subscription(LaserScan, '/scan_raw', lambda m: got.__setitem__(0, m), qos_profile_sensor_data)
end = time.time()+5
while got[0] is None and time.time() < end:
    rclpy.spin_once(n, timeout_sec=0.2)
m = got[0]
if m is None:
    print(0)
elif not any(r == r and 0.05 < r < m.range_max for r in m.ranges):
    print(0)  # 有消息但全 inf = 渲染线程死亡签名
else:
    print(1)
rclpy.shutdown()" 2>/dev/null | grep -E '^[01]$' | tail -n1
}

ROUND=0
while true; do
  ROUND=$((ROUND+1))
  echo "[gz-demo] 第 $ROUND 轮启动 $(date +%T)"
  rm -f /dev/shm/fastrtps_* /dev/shm/sem.fastrtps* 2>/dev/null
  ros2 launch ros2_robot_middleware nav2_demo.launch.py > /tmp/gz_demo_round.log 2>&1 &
  LAUNCH_PID=$!
  # 等扫描上线（最多 90s）
  for i in $(seq 1 18); do [ "$(probe_alive)" = "1" ] && break; sleep 5; done
  if [ "$(probe_alive)" != "1" ]; then
    echo "[gz-demo] ⚠️ 本轮扫描未上线，重启"
    kill $LAUNCH_PID 2>/dev/null; pkill -9 -f "[g]z sim|[n]av2_demo|[s]lam_toolbox|[b]t_nav|[c]ontroller_server|[p]lanner_server|[b]ehavior_server|[c]md_vel_guard|[f]oxglove|[l]ifecycle|[p]arameter_bridge|[r]obot_state" 2>/dev/null
    sleep 3; continue
  fi
  echo "[gz-demo] ✅ 扫描健康，运行中（看护每 15s 探测）"
  if [ "${1:-}" != "--watchdog" ]; then
    wait $LAUNCH_PID; exit 0  # 无看护模式：前台跟随
  fi
  DEAD=0
  while kill -0 $LAUNCH_PID 2>/dev/null; do
    sleep 15
    if [ "$(probe_alive)" = "1" ]; then DEAD=0; else DEAD=$((DEAD+1)); fi
    if [ $DEAD -ge 2 ]; then
      echo "[gz-demo] ⛔ 扫描死亡确认（渲染线程签名），第 $ROUND 轮终结于 $(date +%T), 重启"
      break
    fi
  done
  kill $LAUNCH_PID 2>/dev/null; pkill -9 -f "[g]z sim|[n]av2_demo|[s]lam_toolbox|[b]t_nav|[c]ontroller_server|[p]lanner_server|[b]ehavior_server|[c]md_vel_guard|[f]oxglove|[l]ifecycle|[p]arameter_bridge|[r]obot_state" 2>/dev/null
  sleep 3
done
