#!/usr/bin/env bash
# 快速替换 DDS（RMW 切换）验证：同一份代码，两个 DDS 实现，一键对比。
# 价值声明（简历/面试口径）：rmw 防腐层让 DDS 厂商切换 = 改一个环境变量；
# 本脚本 + CI 双 RMW 矩阵让"可替换"成为被持续验证的事实而非口头承诺。
# 用法: scripts/rmw_matrix.sh [快速样本数，默认 300]
set -o pipefail
cd "$(dirname "$0")/.."
source /opt/ros/jazzy/setup.bash
source install/setup.bash
SAMPLES="${1:-300}"
PING=install/bench_ipc/lib/bench_ipc/ping
PONG=install/bench_ipc/lib/bench_ipc/pong

pkill -x ping 2>/dev/null; pkill -x pong 2>/dev/null; sleep 0.5

run_one() { # rmw_impl label
  export RMW_IMPLEMENTATION="$1"
  unset FASTDDS_BUILTIN_TRANSPORTS
  taskset -c 4 "$PONG" >/dev/null 2>&1 &
  local pid=$!
  sleep 1.2
  taskset -c 2 timeout 120 "$PING" --ros-args -p size:=1024 -p samples:="$SAMPLES" -p warmup:=50 \
    | sed "s/^/{\"rmw_label\":\"$2\",/" | sed 's/"rmw":"rmw_/"rmw2":"rmw_/'
  kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
}

echo "== RMW 矩阵（$SAMPLES 样本 × 1KB，均在本机 WSL2）=="
run_one rmw_cyclonedds_cpp cyclonedds
run_one rmw_fastrtps_cpp  fastrtps
echo "提示: 阈值判断看 P99 尾部与可用性(discovery 成败)，不只看 P50。"
pkill -x pong 2>/dev/null
