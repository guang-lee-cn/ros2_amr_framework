#!/bin/bash
# sim_watchdog.sh — 运行时传感器健康监视 + 自动清场重启。
#
# 背景(2026-08-17 穿货架事故后续, 详见 git 90a1a86):
#   WSL2 gpu_lidar 输出随运行时间劣化(扇区失明→全盲), run_sim.sh 只做
#   启动时健康抽签, 运行中劣化无人管。导航栈 fail-safe 会安全停车并
#   告警等待(scan_filter WARN "传感器疑似失明/扇区失明"), 恢复只能靠
#   重启抽签 —— 本脚本把"人盯日志→人跑 run_sim"自动化。
#
# 用法:  nohup ./sim_watchdog.sh >/tmp/sim_watchdog.log 2>&1 &
# 停止:  pkill -f sim_watchdog.sh
# 判据:  /scan_raw 有效回波 < MIN_VALID(默认50) 连续 FAIL_TRIES 轮
#        (默认4×30s=2min, 覆盖 run_sim 最坏 3×(25s+6s) 启动窗) → 重启。
#        probe 无消息计 0, 因此栈整死(launch 崩/OOM 杀, 8765 消失)同样
#        累计触发拉起 —— 20260818 实况: 栈死后 pgrep 守卫把"死"当
#        "启动间隙"永远跳过, Foxglove 断连无人恢复。手动停仿真请连
#        watchdog 一起停: pkill -f sim_watch。
#        只看 scan 健康, 不看车动不动 —— 车停等可能是业务停车, 误判
#        会重启掉正常任务; 传感器坏了则重启永远是对的。
source /opt/ros/jazzy/setup.bash
source "$(dirname "$(readlink -f "$0")")/../../../../install/setup.bash" 2>/dev/null || \
    source ~/code/ros2_ws/install/setup.bash

INTERVAL=${INTERVAL:-30}
FAIL_TRIES=${FAIL_TRIES:-4}
MIN_VALID=${MIN_VALID:-50}
RUN_SIM="$(dirname "$(readlink -f "$0")")/run_sim.sh"

probe() {  # 输出 /scan_raw 有效回波数(0=无消息/全盲)
python3 - <<'PYEOF'
import rclpy, time
from sensor_msgs.msg import LaserScan
rclpy.init()
node = rclpy.create_node('watchdog_probe%d' % int(time.time()))
got = {}
node.create_subscription(LaserScan, '/scan_raw', lambda m: got.update(m=m), 5)
end = time.time() + 6
while time.time() < end and 'm' not in got:
    rclpy.spin_once(node, timeout_sec=0.2)
print(sum(1 for r in got['m'].ranges if 0.01 < r < 30) if 'm' in got else 0)
rclpy.shutdown()
PYEOF
}

fails=0
echo "[watchdog] 上线: 每${INTERVAL}s 探测, 连续${FAIL_TRIES}轮回波<${MIN_VALID}则重启 (日志 /tmp/sim_watchdog.log)"
while true; do
    sleep "$INTERVAL"
    # 只取纯数字行：FastDDS C++ 日志走 stdout 会污染 probe 输出
    V=$(probe | grep -E '^[0-9]+$' | tail -n1); V=${V:-0}
    if [ "${V:-0}" -ge "$MIN_VALID" ]; then
        [ "$fails" -gt 0 ] && echo "[watchdog] $(date +%H:%M:%S) 恢复健康 (V=$V), 计数清零"
        fails=0
        continue
    fi
    fails=$((fails+1))
    if pgrep -f 'ros2 launch ros2_robot_middleware' >/dev/null; then
        echo "[watchdog] $(date +%H:%M:%S) scan 劣化 V=$V ($fails/$FAIL_TRIES)"
    else
        echo "[watchdog] $(date +%H:%M:%S) 仿真栈不在(V=$V 无消息) ($fails/$FAIL_TRIES)"
    fi
    if [ "$fails" -ge "$FAIL_TRIES" ]; then
        echo "[watchdog] ⛔ $(date +%H:%M:%S) 劣化/栈死确认 — 清场重启"
        fails=0
        "$RUN_SIM"
    fi
done
