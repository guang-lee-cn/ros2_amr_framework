#!/bin/bash
# soak_run.sh — A2 长稳测试 harness：持续负载 + 周期故障注入 + 资源监控。
#
# 迭代2 A2（docs/design/20260824-iteration2-audit-goals.md）验收项：
#   连续负载、周期性 kill -9、内存/泄露监控 →
#   soak 报告：吞吐/时延曲线、恢复次数=注入次数、RSS 平稳。
#
# 架构角色（复用现有运维件，不重写）：
#   负载    patrol_3c（simulation.launch.py 自带，无需另起）
#   注入    本脚本 kill -9 受害进程（VICTIMS 轮换，默认 bridge,gz）
#   恢复    sim_watchdog.sh（/scan_raw 断流 2min → run_sim 清场重启）——
#           当前架构唯一自动恢复链。compute_container 注入后无人拉起
#           （迭代2 B1 supervisor 缺口），故不支持该 victim。
#   判定    恢复 = scan 探针 ≥ MIN_VALID 且 注入后出现新 /goal_pose 事件
#           （soak_monitor.py 跨栈重启存活，patrol 重启即重发首 goal）
#   采样    samples.csv(scan) / rss.csv(进程内存) / monitor.csv(位姿·速度)
#           / goals.csv(吞吐) / metrics_*(:9091 时延快照, 端点存在时)
#
# 用法（72h 正式跑）:
#   nohup ./scripts/soak_run.sh >/tmp/soak.log 2>&1 &
# smoke（15min, 5min 一注入）:
#   DURATION_MIN=15 INJECT_INTERVAL_MIN=5 SAMPLE_INTERVAL=15 ./scripts/soak_run.sh
# 结束（SIGINT/超时）自动生成 $OUT/summary.md（soak_report.py）。
#
# 环境变量:
#   DURATION_MIN          总时长(分)            默认 4320 (72h)
#   INJECT_INTERVAL_MIN   注入间隔(分)          默认 30
#   SAMPLE_INTERVAL       采样间隔(秒)          默认 60
#   RECOVERY_TIMEOUT_S    恢复超时(秒)          默认 900
#   MIN_VALID             scan 健康阈值(回波)   默认 50（与 guard/watchdog 同判据）
#   VICTIMS               注入对象轮换表        默认 bridge,gz（支持 bridge/gz）
#   METRICS_EVERY         :9091 快照间隔(tick)  默认 10（0=关）
#   OUT                   数据目录              默认 /tmp/amr_soak_<时间戳>
set -o pipefail

source /opt/ros/jazzy/setup.bash
source "$(dirname "$(readlink -f "$0")")/../../../../install/setup.bash" 2>/dev/null || \
    source ~/code/ros2_ws/install/setup.bash

DURATION_MIN=${DURATION_MIN:-4320}
INJECT_INTERVAL_MIN=${INJECT_INTERVAL_MIN:-30}
SAMPLE_INTERVAL=${SAMPLE_INTERVAL:-60}
RECOVERY_TIMEOUT_S=${RECOVERY_TIMEOUT_S:-900}
MIN_VALID=${MIN_VALID:-50}
VICTIMS=${VICTIMS:-bridge,gz}
METRICS_EVERY=${METRICS_EVERY:-10}
OUT=${OUT:-/tmp/amr_soak_$(date +%Y%m%d_%H%M%S)}
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"

mkdir -p "$OUT"
echo "ts,scan_valid" > "$OUT/samples.csv"
echo "ts,name,rss_kb" > "$OUT/rss.csv"
echo "ts,goal_x,goal_y" > "$OUT/goals.csv"
echo "ts,pose_x,pose_y,cmd_vel_hz,amcl_hz" > "$OUT/monitor.csv"
echo "event,ts,detail,downtime_s" > "$OUT/inject_log.csv"

# ── 前置校验: victim 白名单（compute 等 B1 缺口外对象拒跑, 防 soak 自陷）──
IFS=',' read -ra VICTIM_ARR <<< "$VICTIMS"
for v in "${VICTIM_ARR[@]}"; do
    case "$v" in
        bridge|gz) ;;
        *) echo "[soak] ⛔ 不支持 victim '$v'（无自动恢复链, 见头部注释 B1）"; exit 1 ;;
    esac
done

probe() {  # /scan_raw 有效回波数（0=无消息/全盲），判据与 run_sim/watchdog 同源
python3 - <<'PYEOF'
import rclpy, time, math
from sensor_msgs.msg import LaserScan
rclpy.init()
node = rclpy.create_node('soak_probe%d' % int(time.time()))
got = {}
node.create_subscription(LaserScan, '/scan_raw', lambda m: got.update(m=m), 5)
end = time.time() + 6
while time.time() < end and 'm' not in got:
    rclpy.spin_once(node, timeout_sec=0.2)
if 'm' not in got:
    print(0); rclpy.shutdown(); raise SystemExit
m = got['m']
valid = [(r, math.degrees(m.angle_min+i*m.angle_increment)) for i, r in enumerate(m.ranges) if 0.01 < r < m.range_max]
sectors = [0]*8
for _, a in valid:
    sectors[int(((a + 180) % 360) // 45)] += 1
alive = sum(1 for v in sectors if v >= 3)
print(len(valid) if alive >= 6 else 0)
rclpy.shutdown()
PYEOF
}

rss_sample() {  # 全进程 RSS 快照（ps 一次, 按名聚合多 pid）
    local ts; ts=$(date +%s)
    local ps_out; ps_out=$(ps -eo rss=,args= 2>/dev/null)
    {
        echo "$ps_out" | grep -E 'gz sim |gz-sim-server' | awk -v t="$ts" '{s+=$1} END {if (s) print t",gz,"s}'
        echo "$ps_out" | grep -F 'parameter_bridge' | awk -v t="$ts" '{s+=$1} END {if (s) print t",bridge,"s}'
        echo "$ps_out" | grep -F 'compute_container' | awk -v t="$ts" '{s+=$1} END {if (s) print t",compute,"s}'
        echo "$ps_out" | grep -F 'scan_filter' | awk -v t="$ts" '{s+=$1} END {if (s) print t",scan_filter,"s}'
        echo "$ps_out" | grep -F 'mock_amcl' | awk -v t="$ts" '{s+=$1} END {if (s) print t",mock_amcl,"s}'
        echo "$ps_out" | grep -F 'patrol_3c' | awk -v t="$ts" '{s+=$1} END {if (s) print t",patrol,"s}'
        echo "$ps_out" | grep -F 'foxglove_bridge' | awk -v t="$ts" '{s+=$1} END {if (s) print t",foxglove,"s}'
        echo "$ps_out" | grep -E 'factory_markers|robot_markers' | awk -v t="$ts" '{s+=$1} END {if (s) print t",markers,"s}'
        echo "$ps_out" | grep -F 'ros2 launch ros2_robot_middleware' | awk -v t="$ts" '{s+=$1} END {if (s) print t",launch,"s}'
    } >> "$OUT/rss.csv"
}

inject() {  # $1=victim → kill -9, 输出命中 pid 列表（空=没找到目标）
    local pat
    case "$1" in
        bridge) pat='parameter_bridge' ;;
        gz)     pat='gz sim|gz-sim-server' ;;
    esac
    local pids; pids=$(pgrep -f "$pat" | paste -sd' ' -)
    [ -n "$pids" ] && kill -9 $pids 2>/dev/null
    echo "$pids"
}

# ── 就绪: 栈健康（直接附着）或 run_sim 抽签拉起 ─────────────────────────
V=$(probe)
if [ "${V:-0}" -lt "$MIN_VALID" ]; then
    echo "[soak] 栈不在/不健康(V=$V) → run_sim 拉起"
    "$SCRIPT_DIR/run_sim.sh" || { echo "[soak] ⛔ run_sim 失败, 放弃"; exit 1; }
else
    echo "[soak] 附着健康栈 (V=$V)"
fi

# ── 就绪: watchdog（已有则复用——不动别人的实例）───────────────────────
WD_PID=""
if pgrep -f 'sim_watchdog' >/dev/null 2>&1; then
    echo "[soak] 复用已在运行的 sim_watchdog"
else
    nohup bash "$SCRIPT_DIR/sim_watchdog.sh" > "$OUT/watchdog.log" 2>&1 &
    WD_PID=$!
    echo "[soak] 起独立 sim_watchdog (PID $WD_PID)"
fi

# ── 就绪: 业务采样器 ──────────────────────────────────────────────────
nohup python3 "$SCRIPT_DIR/soak_monitor.py" "$OUT" > "$OUT/monitor.log" 2>&1 &
MON_PID=$!
echo "[soak] soak_monitor 起 (PID $MON_PID)"

# ── 主循环 ────────────────────────────────────────────────────────────
finish() {
    [ -n "$MON_PID" ] && kill "$MON_PID" 2>/dev/null
    [ -n "$WD_PID" ] && kill "$WD_PID" 2>/dev/null
    python3 "$SCRIPT_DIR/soak_report.py" "$OUT" > "$OUT/summary.md" 2> "$OUT/report.err"
    echo "[soak] 报告: $OUT/summary.md（数据目录 $OUT）"
}
trap 'finish; trap - INT TERM EXIT; exit 0' INT TERM
trap 'finish' EXIT

start_ts=$(date +%s)
end_ts=$(( start_ts + DURATION_MIN * 60 ))
next_inject=$(( start_ts + INJECT_INTERVAL_MIN * 60 ))
vict_idx=0
pending_ts=""       # 未决注入的时间戳（空=无未决）
tick=0
echo "[soak] 开始: $DURATION_MIN 分钟, 每 $INJECT_INTERVAL_MIN 分钟注入 [$VICTIMS], 采样 ${SAMPLE_INTERVAL}s → $OUT"

while [ "$(date +%s)" -lt "$end_ts" ]; do
    now=$(date +%s)
    tick=$((tick + 1))
    V=$(probe)
    echo "$now,$V" >> "$OUT/samples.csv"
    rss_sample

    # 恢复判定（注入未决时）: scan 健康 + 注入后新 goal 事件
    if [ -n "$pending_ts" ]; then
        new_goals=$(awk -F, -v t="$pending_ts" 'NR>1 && $1+0 > t' "$OUT/goals.csv" 2>/dev/null | wc -l)
        if [ "${V:-0}" -ge "$MIN_VALID" ] && [ "${new_goals:-0}" -ge 1 ]; then
            echo "recovered,$now,$pending_victim,$(( now - pending_ts ))" >> "$OUT/inject_log.csv"
            echo "[soak] ✅ $pending_victim 注入后 $(( now - pending_ts ))s 恢复"
            pending_ts=""
        elif [ $(( now - pending_ts )) -gt "$RECOVERY_TIMEOUT_S" ]; then
            echo "timeout,$now,$pending_victim,$(( now - pending_ts ))" >> "$OUT/inject_log.csv"
            echo "[soak] ❌ $pending_victim 恢复超时(${RECOVERY_TIMEOUT_S}s) — 记录并继续"
            pending_ts=""
        fi
    else
        # 非注入劣化（WSL2 渲染劣化/栈自亡）——watchdog 应自愈, 这里记账
        if [ "${V:-0}" -lt "$MIN_VALID" ]; then
            if [ -z "$blip_start" ]; then
                blip_start=$now
                echo "[soak] ⚠️ 非注入劣化开始 (V=$V)"
            fi
        elif [ -n "$blip_start" ]; then
            echo "external,$now,degradation,$(( now - blip_start ))" >> "$OUT/inject_log.csv"
            echo "[soak] 非注入劣化持续 $(( now - blip_start ))s 后自愈/被 watchdog 重启"
            blip_start=""
        fi
    fi

    # 注入调度（未决恢复期间不叠加注入）
    if [ -z "$pending_ts" ] && [ "$now" -ge "$next_inject" ]; then
        vict=${VICTIM_ARR[$(( vict_idx % ${#VICTIM_ARR[@]} ))]}
        vict_idx=$(( vict_idx + 1 ))
        pids=$(inject "$vict")
        if [ -n "$pids" ]; then
            # 注入与外部劣化撞窗: 外部劣化记账截止到注入点（后续归注入窗口）
            if [ -n "$blip_start" ]; then
                echo "external,$now,degradation(merged into inject $vict),$(( now - blip_start ))" >> "$OUT/inject_log.csv"
                blip_start=""
            fi
            pending_ts=$now; pending_victim=$vict
            echo "inject,$now,$vict(pids:$pids),0" >> "$OUT/inject_log.csv"
            echo "[soak] 💉 注入 kill -9 $vict (pids: $pids), 等待 watchdog 恢复链"
        else
            echo "skip,$now,$vict,0" >> "$OUT/inject_log.csv"
            echo "[soak] ⚠️ victim $vict 未找到进程, 跳过"
        fi
        next_inject=$(( now + INJECT_INTERVAL_MIN * 60 ))
    fi

    # :9091 时延快照（端点存在才落盘; AMR_PERF_INSTRUMENTATION=ON 构建时可用）
    if [ "$METRICS_EVERY" -gt 0 ] && [ $(( tick % METRICS_EVERY )) -eq 0 ]; then
        curl -m 2 -s "localhost:9091/metrics" > "$OUT/metrics_$now.txt" 2>/dev/null
        [ -s "$OUT/metrics_$now.txt" ] || rm -f "$OUT/metrics_$now.txt"
    fi

    sleep "$SAMPLE_INTERVAL"
done

echo "[soak] 时长到点 ($DURATION_MIN min), 收尾出报告"
