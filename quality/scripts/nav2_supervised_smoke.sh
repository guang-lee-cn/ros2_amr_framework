#!/bin/bash
# ── F2 验证门：受监管 NAV2 形态——挂死可检出 + 进程级可恢复 ──────────────
# F2 的故障故事：NAV2 某 server 死锁（进程活着、不工作）。实测（2026-09-09）：
# NAV2 的 bond 4s 内就能检出并 deactivate 全栈，但**恢复不了**——进程没死，
# launch 的 respawn 与 supervisor 的 waitpid 都看不见它。本脚本锁死端到端闭环：
#   (1) 整栈 bringup（Managed nodes are active）
#   (2) 聚合健康门放行：6 个 NAV2 节点全 ACTIVE 才 STARTING → RUNNING
#   (3) 空闲 30s 不误杀（拉起行数恒为 1）
#   (4) 故障注入 kill -STOP（进程活着不工作 = F2 的故障形状）
#   (5) health_monitor 报 controller_server ERROR
#   (6) supervisor 进程组 kill + 重拉（拉起 2 行 + 第 2 次 Managed nodes are active）
#   (7) 无孤儿：controller_server 恰好 1 个
#   (8) 恢复后可驱车：goal accepted + /cmd_vel 有流量
# 依赖: nav2/amcl/foxglove_bridge 已安装（CI job 负责）。预算 ~180-240s。
set -eo pipefail
source /opt/ros/jazzy/setup.bash
source install/setup.bash

rm -f /dev/shm/fastrtps_* /dev/shm/sem.fastrtps* 2>/dev/null || true  # 上轮残留锁会致 init_port 失败
OUT=/tmp/nav2_supervised.log
: > "$OUT"
echo "[sup-smoke] 启动 supervised_nav2（supervisor 监管整栈）..."
ros2 launch ros2_robot_middleware supervised_nav2.launch.py > "$OUT" 2>&1 &
LAUNCH_PID=$!
# 组杀优先（supervisor 的孙进程不归 launch 直接管）；pkill 一律方括号技巧
trap 'kill $LAUNCH_PID 2>/dev/null || true; pkill -9 -f "[a]mr_supervisor|[s]upervised_nav2|[n]av2_localized|[s]cene_simulator|[/]map_server|[/]amcl|[/]bt_navigator|[/]controller_server|[/]planner_server|[/]behavior_server|[c]md_vel_guard|[h]ealth_monitor_node|[f]oxglove" 2>/dev/null || true' EXIT

T0=$(date +%s)
wait_for() {  # wait_for <pattern> <timeout_s> <描述>——首次出现即返回
  local pat="$1" timeout="$2" what="$3"
  for _ in $(seq 1 "$timeout"); do
    grep -qE "$pat" "$OUT" && return 0
    sleep 1
  done
  echo "::error::超时未出现：$what（pattern: $pat）"
  tail -30 "$OUT"
  exit 1
}

wait_for_count() {  # wait_for_count <pattern> <count> <timeout_s> <描述>
  # 重拉断言必须计数：首次出现的行还在日志里，wait_for 会立刻假绿
  local pat="$1" want="$2" timeout="$3" what="$4" n
  for _ in $(seq 1 "$timeout"); do
    n=$(grep -cE "$pat" "$OUT" || true)
    [ "${n:-0}" -ge "$want" ] && return 0
    sleep 1
  done
  echo "::error::超时未达 $want 次：$what（pattern: $pat，实到 $(grep -cE "$pat" "$OUT" || true)）"
  tail -30 "$OUT"
  exit 1
}

# (1)(2) bringup + 聚合健康门放行
wait_for "Managed nodes are active" 150 "NAV2 整栈激活"
wait_for "nav2: STARTING → RUNNING" 60 "聚合健康门放行（6 节点全 ACTIVE）"
echo "[sup-smoke] ✓ (1)(2) bringup + 聚合健康门放行（$(($(date +%s)-T0))s）"

# (3) 空闲不误杀：生命周期探针在空闲时也必须是 ACTIVE，不得有抖动
BEFORE=$(grep -c "▶ nav2 拉起" "$OUT" || true)
sleep 30
AFTER=$(grep -c "▶ nav2 拉起" "$OUT" || true)
[ "$BEFORE" = "1" ] && [ "$AFTER" = "1" ] || {
  echo "::error::空闲期误杀/抖动（拉起行数 $BEFORE → $AFTER）"; exit 1; }
echo "[sup-smoke] ✓ (3) 空闲 30s 不误杀（$(($(date +%s)-T0))s）"

# (4) 故障注入：SIGSTOP = 进程活着但不再工作（F2 的故障形状）
# 路径锚定：health_monitor 的 argv 含 "health_monitor.controller_server.probe:=..."，
# 裸名匹配会 STOP 错进程（实测踩过：停住了 health_monitor 自己）
CS_PID=$(pgrep -f "[/]controller_server" | head -1)
[ -n "$CS_PID" ] || { echo "::error::未找到 controller_server 进程"; exit 1; }
# (5) 探针先起，避免 ERROR 窗口被错过后假红
timeout 60 python3 -c "
import rclpy, time
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy
from ros2_robot_middleware.msg import HealthReport
rclpy.init(); n = Node('sup_smoke_probe')
hit = []
def cb(m):
    for s in m.nodes:
        if s.node_name == 'controller_server' and s.status in ('ERROR', 'FATAL'):
            hit.append(s.status)
n.create_subscription(HealthReport, '/health/report', cb,
                      QoSProfile(depth=10, reliability=ReliabilityPolicy.RELIABLE))
end = time.time() + 55
while time.time() < end and not hit:
    rclpy.spin_once(n, timeout_sec=0.2)
print('HEALTH_ERROR_SEEN' if hit else 'HEALTH_ERROR_MISS')
rclpy.shutdown()" > /tmp/sup_smoke_probe.out 2>&1 &
PROBE_PID=$!
sleep 2
kill -STOP "$CS_PID"
echo "[sup-smoke] 注入挂死：kill -STOP $CS_PID"

# (5) health_monitor 的 ERROR 必须到达 /health/report
wait "$PROBE_PID" || true
grep -q "HEALTH_ERROR_SEEN" /tmp/sup_smoke_probe.out || {
  echo "::error::health_monitor 未报 controller_server ERROR（探针：$(cat /tmp/sup_smoke_probe.out)）"
  tail -20 "$OUT"; exit 1; }
echo "[sup-smoke] ✓ (5) health_monitor 报 ERROR（$(($(date +%s)-T0))s）"

# (6) 进程级 kill + 重拉（F2 的缺口所在：仅 deactivate 不算恢复）
wait_for "nav2: RUNNING → BACKOFF" 30 "supervisor 收到 HEALTH_LOST"
for _ in $(seq 1 60); do  # 已有 1 行，等第 2 行（wait_for 无法计数，单独轮询）
  [ "$(grep -c '▶ nav2 拉起' "$OUT" || true)" -ge 2 ] && break
  sleep 1
done
[ "$(grep -c '▶ nav2 拉起' "$OUT" || true)" -ge 2 ] || {
  echo "::error::未触发重拉（拉起行数 $(grep -c '▶ nav2 拉起' "$OUT" || true)）"; exit 1; }
wait_for_count "Managed nodes are active" 2 150 "重拉后整栈再次激活"
wait_for_count "nav2: STARTING → RUNNING" 2 60 "重拉后聚合健康门再次放行"
echo "[sup-smoke] ✓ (6) kill + 重拉 + 整栈复活（$(($(date +%s)-T0))s）"

# (7) 无孤儿：组杀必须覆盖孙进程（W4 契约的端到端证明）
sleep 5
N_CS=$(pgrep -cf "[/]controller_server" || true)
[ "${N_CS:-0}" -eq 1 ] || { echo "::error::controller_server 进程数 $N_CS ≠ 1（组杀漏孙进程）"; exit 1; }
echo "[sup-smoke] ✓ (7) 无孤儿（controller_server × 1）"

# (8) 恢复后可驱车：goal accepted + 闸后 /cmd_vel 有流量
ros2 action send_goal /navigate_to_pose nav2_msgs/action/NavigateToPose \
  "{pose: {header: {frame_id: 'map'}, pose: {position: {x: 17.0, y: 0.0}, orientation: {w: 1.0}}}}" \
  > /tmp/sup_smoke_goal.log 2>&1 &
for _ in $(seq 1 30); do
  grep -qE "Goal accepted|Goal was rejected" /tmp/sup_smoke_goal.log && break
  sleep 2
done
if grep -q "Goal was rejected" /tmp/sup_smoke_goal.log; then
  echo "[sup-smoke] 首发被拒（激活边缘竞态）——5s 后重试"
  sleep 5
  ros2 action send_goal /navigate_to_pose nav2_msgs/action/NavigateToPose \
    "{pose: {header: {frame_id: 'map'}, pose: {position: {x: 17.0, y: 0.0}, orientation: {w: 1.0}}}}" \
    > /tmp/sup_smoke_goal.log 2>&1 &
  for _ in $(seq 1 30); do
    grep -qE "Goal accepted|Goal was rejected" /tmp/sup_smoke_goal.log && break
    sleep 2
  done
fi
grep -q "Goal accepted" /tmp/sup_smoke_goal.log || {
  echo "::error::恢复后目标被拒"; cat /tmp/sup_smoke_goal.log; exit 1; }
sleep 6  # 航程中段采样（避开到站停发）
FLOW=$(timeout 15 python3 -c "
import rclpy, time
from rclpy.node import Node
from geometry_msgs.msg import Twist
rclpy.init(); n = Node('sup_flowprobe')
out = [0]
n.create_subscription(Twist, '/cmd_vel', lambda m: out.__setitem__(0, out[0]+1), 10)
end = time.time()+6
while time.time() < end: rclpy.spin_once(n, timeout_sec=0.2)
print(out[0])
rclpy.shutdown()" 2>/dev/null | grep -E '^[0-9]+$' | tail -n1)
[ "${FLOW:-0}" -gt 0 ] || { echo "::error::恢复后 /cmd_vel 零流量"; exit 1; }
echo "[sup-smoke] ✓ (8) 恢复后可驱车（/cmd_vel=$FLOW）"

echo "[sup-smoke] 全过（$(($(date +%s)-T0))s）—— F2 闭环：检出 → 进程级恢复 → 可驱车"
