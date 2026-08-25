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
LAUNCH_FILE=${LAUNCH_FILE:-simulation.launch.py}   # supervised 形态: supervised_sim.launch.py（B1 进程监管）

clean() {
    pkill -9 -f 'ros2_robot_middleware/lib' 2>/dev/null
    pkill -9 -f 'ros2 launch' 2>/dev/null
    pkill -9 -f 'gz sim' 2>/dev/null
    pkill -9 -f 'parameter_bridge' 2>/dev/null
    # 08-16 教训重演（2026-08-25 实证）：清单不全的清场留孤儿——foxglove_bridge
    # 占着 8765 让后续每次启动 Bind Error 崩溃循环。静态 TF 同理（路径不在本包 lib 下）。
    pkill -9 -f 'foxglove_bridge' 2>/dev/null
    pkill -9 -f 'static_transform_publisher' 2>/dev/null
    sleep 3
    # FastDDS SHM 实际文件名：fastrtps_* 段 + sem.fastrtps_port*_mutex 端口锁
    # （fastdds* 是多年无效 glob；残留锁致新参与者 open_and_lock_file 报错）
    rm -f /dev/shm/fastdds* /dev/shm/fastrtps_* /dev/shm/sem.fastrtps* 2>/dev/null
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
    nohup ros2 launch ros2_robot_middleware "$LAUNCH_FILE" > "$LOG" 2>&1 &
    echo $! > /tmp/sim_run.pid
    echo "[run_sim] 尝试 $try/$MAX_TRIES 启动中(等待25s)... 日志: $LOG"
    sleep 25
    # FastDDS C++ 日志走 stdout 会混入 probe 输出——只取纯数字行
    V=$(probe | grep -E '^[0-9]+$' | tail -n1); V=${V:-0}
    if [ "${V:-0}" -ge "$MIN_VALID" ]; then
        echo "[run_sim] ✅ Healthy (valid echoes $V ≥ $MIN_VALID), PID=$(cat /tmp/sim_run.pid)"
        echo "[run_sim] Foxglove: bridge starts with simulation, connect directly to ws://localhost:8765"
        exit 0
    fi
    echo "[run_sim] ❌ 不健康(有效回波 $V < $MIN_VALID), 清场重试..."
done
clean
echo "[run_sim] ⛔ $MAX_TRIES 次均不健康。建议: Windows 侧 wsl --shutdown 重置 GPU 透传后重跑本脚本。"
exit 1
