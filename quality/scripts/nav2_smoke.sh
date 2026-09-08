#!/bin/bash
# ── M1（商用差距 20260908）：NAV2 冒烟——60s 锁死"绕闸"类缺陷 ──────────
# N-2 类问题（launch 直出 /cmd_vel 绕过安全闸）只有人工审计能抓——本测试
# 让它变成 CI 红灯：启动 nav2_scene 形态，断言 (1) cmd_vel_guard 进程在
# (2) /cmd_vel_raw 有流量（NAV2→闸链路活）(3) /cmd_vel 有流量（闸→底盘活）
# (4) 发一个目标后 90s 内 odom 到达。
# 依赖: nav2/slam_toolbox/foxglove_bridge 已安装（CI job 负责）。
set -eo pipefail
source /opt/ros/jazzy/setup.bash
source install/setup.bash

rm -f /dev/shm/fastrtps_* /dev/shm/sem.fastrtps* 2>/dev/null || true  # 上轮残留锁会致 init_port 失败
echo "[smoke] 启动 nav2_scene（rack_3c）..."
OUT=/tmp/nav2_smoke.log
ros2 launch ros2_robot_middleware nav2_scene.launch.py > "$OUT" 2>&1 &
LAUNCH_PID=$!
trap 'kill $LAUNCH_PID 2>/dev/null || true; pkill -9 -f "[n]av2_scene|[s]cene_simulator|[s]lam_toolbox|[b]t_navigator|[c]ontroller_server|[p]lanner_server|[b]ehavior_server|[c]md_vel_guard|[f]oxglove" 2>/dev/null || true' EXIT

# (1) bringup + guard 存活
for i in $(seq 1 30); do
  grep -q "Managed nodes are active" "$OUT" && grep -q "guard armed" "$OUT" && break
  sleep 2
done
grep -q "Managed nodes are active" "$OUT" || { echo "::error::NAV2 bringup 未完成"; tail -20 "$OUT"; exit 1; }
grep -q "guard armed" "$OUT" || { echo "::error::安全闸未启动（N-2 绕闸复发形态）"; exit 1; }
echo "[smoke] ✓ bringup + guard armed"

# (2)(3) 链路流量断言（发目标驱动流量）
ros2 action send_goal /navigate_to_pose nav2_msgs/action/NavigateToPose \
  "{pose: {header: {frame_id: 'map'}, pose: {position: {x: 17.0, y: 0.0}, orientation: {w: 1.0}}}}" \
  > /tmp/smoke_goal.log 2>&1 &
# 等 goal 被接收再探流量——本机 action 发现可达 30s+（时序教训：固定
# sleep 25s 时目标尚未被接收，控制器零发布，冒烟假红）
WAIT_GOAL() {
  for i in $(seq 1 30); do
    grep -qE "Goal accepted|Goal was rejected" /tmp/smoke_goal.log && return 0
    sleep 2
  done
  return 1
}
WAIT_GOAL || { echo "::error::目标无响应（action 发现超时）"; cat /tmp/smoke_goal.log; exit 1; }
# 激活边缘竞态：bt_navigator 刚 active 时 goal 偶被拒（与假成功同族反向）——重试一次
if grep -q "Goal was rejected" /tmp/smoke_goal.log; then
  echo "[smoke] 首发被拒（激活边缘竞态）——5s 后重试"
  sleep 5
  ros2 action send_goal /navigate_to_pose nav2_msgs/action/NavigateToPose \
    "{pose: {header: {frame_id: 'map'}, pose: {position: {x: 17.0, y: 0.0}, orientation: {w: 1.0}}}}" \
    > /tmp/smoke_goal.log 2>&1 &
  WAIT_GOAL || { echo "::error::重试目标无响应"; cat /tmp/smoke_goal.log; exit 1; }
fi
grep -q "Goal accepted" /tmp/smoke_goal.log || { echo "::error::目标被拒（重试后仍拒）"; cat /tmp/smoke_goal.log; exit 1; }
sleep 6  # 航程中段采样窗口（17m@0.5m/s≈34s，+6~12s 安全避开到站停发）
# 单进程同窗双订阅（时序教训：先后两探针会跨过到站点——17m 目标 ~35s
# 完成，第二探针落在停发期假红；同窗采样 + 航程中段窗口根治）
FLOW=$(timeout 15 python3 -c "
import rclpy, time
from rclpy.node import Node
from geometry_msgs.msg import Twist
rclpy.init(); n = Node('flowprobe')
raw = [0]; out = [0]
n.create_subscription(Twist, '/cmd_vel_raw', lambda m: raw.__setitem__(0, raw[0]+1), 10)
n.create_subscription(Twist, '/cmd_vel', lambda m: out.__setitem__(0, out[0]+1), 10)
end = time.time()+6
while time.time() < end: rclpy.spin_once(n, timeout_sec=0.2)
print(raw[0], out[0])
rclpy.shutdown()" 2>/dev/null | grep -E '^[0-9]+ [0-9]+$' | tail -n1)
RAW=$(echo ${FLOW:-"0 0"} | cut -d' ' -f1)
OUTV=$(echo ${FLOW:-"0 0"} | cut -d' ' -f2)
[ "${RAW:-0}" -gt 0 ] || { echo "::error::/cmd_vel_raw 零流量——NAV2→闸链路死"; exit 1; }
[ "${OUTV:-0}" -gt 0 ] || { echo "::error::/cmd_vel 零流量——闸→底盘链路死"; exit 1; }
echo "[smoke] ✓ 链路流量 raw=$RAW out=$OUTV"

# (4) 目标到达
for i in $(seq 1 12); do
  grep -q "Goal finished with status: SUCCEEDED" /tmp/smoke_goal.log && break
  sleep 5
done
grep -q "SUCCEEDED" /tmp/smoke_goal.log || { echo "::error::冒烟目标未到达"; cat /tmp/smoke_goal.log; exit 1; }
echo "[smoke] ✓ 目标到达 —— NAV2 冒烟全过"
