#!/bin/bash
# run_sim.sh — 带健康门控的仿真启动器（WSL2 gpu_lidar 输出质量随机的兜底运维）
#
# 背景(2026-08-16 排障, 详见 docs/change_journal.md):
#   gz gpu_lidar 在 WSL2/EGL 下输出质量随启动与运行时间劣化(伪影/全inf)。
#   另: pkill 'gz sim' 只杀 gz 本体会泄漏整套 ros2 launch 节点栈, CPU 超载会
#   加速传感器失效 —— 本脚本每次启动前做全量清场。
#
# 用法:
#   ./run_sim.sh [最大尝试次数, 默认3]
# 健康标准: /scan_raw 有效回波 >= MIN_VALID(默认100)
# 输出: 健康则后台运行并打印日志路径; 连续失败则退出码1(考虑 wsl --shutdown 后重试)
#
source /opt/ros/jazzy/setup.bash
source "$(dirname "$(readlink -f "$0")")/../../../../install/setup.bash" 2>/dev/null || \
    source ~/code/ros2_ws/install/setup.bash

MIN_VALID=${MIN_VALID:-100}
MAX_TRIES=${1:-3}

clean() {
    pkill -9 -f 'ros2_robot_middleware/lib' 2>/dev/null
    pkill -9 -f 'ros2 launch' 2>/dev/null
    pkill -9 -f 'gz sim' 2>/dev/null
    pkill -9 -f 'parameter_bridge' 2>/dev/null
    sleep 3
    rm -f /dev/shm/fastdds* 2>/dev/null
}

probe() {  # 输出 /scan_raw 有效回波数
python3 - <<'PYEOF'
import rclpy, time, math
from sensor_msgs.msg import LaserScan
rclpy.init()
node = rclpy.create_node('sim_health_probe%d' % int(time.time()))
got = {}
node.create_subscription(LaserScan, '/scan_raw', lambda m: got.update(m=m), 10)
end = time.time() + 6
while time.time() < end and 'm' not in got:
    rclpy.spin_once(node, timeout_sec=0.2)
if 'm' not in got:
    print(0); rclpy.shutdown(); raise SystemExit
m = got['m']
valid = [(r, math.degrees(m.angle_min+i*m.angle_increment)) for i, r in enumerate(m.ranges) if 0.01 < r < m.range_max]
n = len(valid)
# 扇区覆盖: 8x45°, 要求 ≥6 个扇区有 ≥3 个回波 (防"半平面失明": 计数够但一半方位全死)
sectors = [0]*8
for _, a in valid:
    sectors[int(((a + 180) % 360) // 45)] += 1
alive = sum(1 for v in sectors if v >= 3)
print(n if alive >= 6 else 0)
rclpy.shutdown()
PYEOF
}

for try in $(seq 1 "$MAX_TRIES"); do
    clean
    LOG=/tmp/sim_run_$(date +%H%M%S)_try$try.log
    nohup ros2 launch ros2_robot_middleware simulation.launch.py > "$LOG" 2>&1 &
    echo $! > /tmp/sim_run.pid
    echo "[run_sim] 尝试 $try/$MAX_TRIES 启动中(等待25s)... 日志: $LOG"
    sleep 25
    V=$(probe)
    if [ "${V:-0}" -ge "$MIN_VALID" ]; then
        echo "[run_sim] ✅ 健康(有效回波 $V ≥ $MIN_VALID), PID=$(cat /tmp/sim_run.pid)"
        echo "[run_sim] Foxglove: ros2 launch foxglove_bridge foxglove_bridge_launch.xml 后连 ws://localhost:8765"
        exit 0
    fi
    echo "[run_sim] ❌ 不健康(有效回波 $V < $MIN_VALID), 清场重试..."
done
clean
echo "[run_sim] ⛔ $MAX_TRIES 次均不健康。建议: Windows 侧 wsl --shutdown 重置 GPU 透传后重跑本脚本。"
exit 1
