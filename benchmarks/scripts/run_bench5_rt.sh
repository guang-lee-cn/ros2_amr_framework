#!/usr/bin/env bash
# 基准五：RT 内核复测套件（U8 后续 / PREEMPT_RT 加分项闭环）
# 产出：内核元数据行 + cyclictest 延迟 + DDS 关键矩阵子集（结果自动带 RT 标签）
# 前置：裸机 Ubuntu 24.04+（WSL2 不可用——时钟虚拟化）; 可选 apt install rt-tests
cd "$(dirname "$0")/../.." || exit 1
source /opt/ros/jazzy/setup.bash 2>/dev/null || true
[ -f install/setup.bash ] && source install/setup.bash
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
unset FASTDDS_BUILTIN_TRANSPORTS
mkdir -p benchmarks/results
OUT="benchmarks/results/bench5_rt_$(date +%Y%m%d_%H%M%S).jsonl"
echo "output: $OUT"

# ── 内核元数据（每份结果自带出身证明） ──
KVER=$(uname -r)
if uname -v | grep -q "PREEMPT_RT"; then RT_FLAG=true; else RT_FLAG=false; fi
echo "{\"bench\":\"meta\",\"kernel\":\"$KVER\",\"preempt_rt\":$RT_FLAG,\"host\":\"$(hostname)\",\"rmw\":\"$RMW_IMPLEMENTATION\"}" | tee -a "$OUT"

# ── cyclictest：调度延迟（无 rt-tests 则跳过并记录） ──
if command -v cyclictest >/dev/null 2>&1; then
  echo "--- cyclictest 30s（加载背景: hackbench 若可用） ---"
  if command -v hackbench >/dev/null 2>&1; then hackbench -l 10000 & HB=$!; fi
  CYC_OUT=$(cyclictest -m -Sp95 -i1000 -h400 -q -D 30s 2>/dev/null | awk '/Max/ {for(i=1;i<=NF;i++) if($i=="Max:") print $(i+1)}' | sort -n | tail -1)
  [ -n "${HB:-}" ] && kill $HB 2>/dev/null
  echo "{\"bench\":\"cyclictest\",\"duration_s\":30,\"max_latency_us\":${CYC_OUT:-null},\"loaded\":${HB:+true}}" | tee -a "$OUT"
else
  echo "{\"bench\":\"cyclictest\",\"error\":\"rt-tests not installed (apt install rt-tests)\"}" | tee -a "$OUT"
fi

# ── DDS 关键矩阵子集（与 bench1/bench4 同口径，可纵向对照） ──
PING=install/bench_ipc/lib/bench_ipc/ping
PONG=install/bench_ipc/lib/bench_ipc/pong
INTRA=install/bench_ipc/lib/bench_ipc/bench_intra
pkill -x ping 2>/dev/null; pkill -x pong 2>/dev/null; sleep 0.5

run_inter() { # rel size samples
  taskset -c 4 "$PONG" --ros-args -p reliability:="$1" >/dev/null 2>&1 &
  local pid=$!
  sleep 1.5
  taskset -c 2 timeout 240 "$PING" --ros-args -p size:="$2" -p samples:="$3" -p warmup:=200 -p reliability:="$1" | tee -a "$OUT"
  kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
}

echo "--- inter 1K reliable ---"; run_inter reliable 1024 2000
echo "--- inter 1M reliable ---"; run_inter reliable 1048576 800
echo "--- inter 1M best_effort ---"; run_inter best_effort 1048576 800
echo "--- intra 1M（零拷贝） ---"
taskset -c 2 "$INTRA" --ros-args -p size:=1048576 -p samples:=800 -p warmup:=200 | tee -a "$OUT"
echo "done: $OUT （RT=$RT_FLAG kernel=$KVER）"
