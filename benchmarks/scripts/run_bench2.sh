#!/usr/bin/env bash
# 基准二：zenoh 端云链路（本机 loopback 模拟端云；弱网模拟受 WSL2 权限限制未启用）
cd "$(dirname "$0")/../.."
PY=benchmarks/bench_zenoh/.venv/bin/python
mkdir -p results
OUT="results/bench2_$(date +%Y%m%d_%H%M%S).jsonl"
echo "output: $OUT"

run_latency() { # size samples
  taskset -c 6 $PY bench_zenoh/zenoh_latency.py --role pong >/dev/null 2>&1 &
  local pid=$!
  sleep 2
  taskset -c 8 $PY bench_zenoh/zenoh_latency.py --role ping --size "$1" --samples "$2" | tee -a "$OUT"
  kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
}

run_throughput() { # size duration
  taskset -c 6 $PY bench_zenoh/zenoh_throughput.py --role recv >> "$OUT" 2>&1 &
  local pid=$!
  sleep 2
  taskset -c 8 $PY bench_zenoh/zenoh_throughput.py --role send --size "$1" --duration "$2"
  kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
}

echo "--- latency 256B ---"; run_latency 256 3000
echo "--- latency 16K ---";  run_latency 16384 2000
echo "--- throughput 64KB 15s ---"; run_throughput 65536 15
echo "--- throughput 1MB 15s ---";  run_throughput 1048576 15
echo "done: $OUT"
