#!/usr/bin/env bash
# 基准一 v2：机内 IPC/DDS 链路时延矩阵
# 环境发现(2026-08-20 WSL2)：rmw_fastrtps_cpp 默认/SHM 传输 discovery 不通(UDP 组播被 WSL2 网络栈干扰)，
# rmw_cyclonedds_cpp 开箱即用——矩阵以 cyclonedds + intra(零拷贝) 为主，fastrtps 各传输保留一次失败记录。
cd "$(dirname "$0")/../.."
source /opt/ros/jazzy/setup.bash
[ -f install/setup.bash ] && source install/setup.bash || source install/bench_ipc/setup.bash 2>/dev/null || true
mkdir -p results
OUT="results/bench1_$(date +%Y%m%d_%H%M%S).jsonl"
echo "output: $OUT"

run_inter() { # rmw size samples
  export RMW_IMPLEMENTATION="$1"
  unset FASTDDS_BUILTIN_TRANSPORTS
  taskset -c 4 ros2 run bench_ipc pong >/dev/null 2>&1 &
  local pid=$!
  sleep 1.5
  taskset -c 2 timeout 240 ros2 run bench_ipc ping --ros-args -p size:="$2" -p samples:="$3" -p warmup:=300 | tee -a "$OUT"
  local rc=${PIPESTATUS[0]}
  kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
  [ "$rc" -ne 0 ] && echo "{\"bench\":\"ipc_inter\",\"rmw\":\"$1\",\"error\":\"failed rc=$rc\"}" | tee -a "$OUT"
  return 0
}

run_intra() { # size samples
  export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
  unset FASTDDS_BUILTIN_TRANSPORTS
  taskset -c 2 ros2 run bench_ipc bench_intra --ros-args -p size:="$1" -p samples:="$2" -p warmup:=300 | tee -a "$OUT"
}

# FastDDS 失败记录（一次即可，各默认传输）
run_inter rmw_fastrtps_cpp 1024 50

# 主矩阵：cyclonedds × 3 种载荷
for size in 1024 16384 1048576; do
  samples=3000; [ "$size" -gt 100000 ] && samples=1500
  echo "--- cyclonedds size=$size samples=$samples ---"
  run_inter rmw_cyclonedds_cpp "$size" "$samples"
done

# 进程内零拷贝 × 3 种载荷
for size in 1024 16384 1048576; do
  samples=3000; [ "$size" -gt 100000 ] && samples=1500
  echo "--- intra size=$size samples=$samples ---"
  run_intra "$size" "$samples"
done
echo "done: $OUT"
