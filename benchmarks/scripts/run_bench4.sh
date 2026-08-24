#!/usr/bin/env bash
# 基准四：QoS 矩阵（U3 DDS 深度）
# A: reliability ∈ {reliable, best_effort} × 载荷 {1K, 1M} 的 ping-pong 时延（CycloneDDS）
# B: durability ∈ {transient_local, volatile} 的晚加入补帧实测
cd "$(dirname "$0")/../.."
source /opt/ros/jazzy/setup.bash
[ -f install/setup.bash ] && source install/setup.bash || source install/bench_ipc/setup.bash 2>/dev/null || true
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
unset FASTDDS_BUILTIN_TRANSPORTS
mkdir -p results
OUT="results/bench4_$(date +%Y%m%d_%H%M%S).jsonl"
echo "output: $OUT"
PING=install/bench_ipc/lib/bench_ipc/ping
PONG=install/bench_ipc/lib/bench_ipc/pong
DUR=install/bench_ipc/lib/bench_ipc/bench_durability

pkill -x pong; pkill -x ping; pkill -x bench_durability; sleep 0.5

run_qos() { # rel size samples
  taskset -c 4 "$PONG" --ros-args -p reliability:="$1" >/dev/null 2>&1 &
  local pid=$!
  sleep 1.5
  taskset -c 2 timeout 240 "$PING" --ros-args -p size:="$2" -p samples:="$3" \
    -p warmup:=200 -p reliability:="$1" | tee -a "$OUT"
  kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
}

echo "--- A: QoS 时延矩阵 ---"
run_qos reliable    1024    2000
run_qos best_effort 1024    2000
run_qos reliable    1048576  800
run_qos best_effort 1048576  800

echo "--- B: durability 晚加入 ---"
run_dur() { # durability
  taskset -c 4 "$DUR" --ros-args -p mode:=pub -p durability:="$1" -p k:=5 >/dev/null 2>&1 &
  local pid=$!
  sleep 2   # 发布方 5 帧已发完，订阅者此刻才启动
  taskset -c 2 "$DUR" --ros-args -p mode:=sub -p durability:="$1" -p k:=5 | tee -a "$OUT"
  kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
}
run_dur transient_local
run_dur volatile
echo "done: $OUT"
