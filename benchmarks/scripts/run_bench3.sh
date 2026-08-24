#!/usr/bin/env bash
# 基准三：故障恢复时间（kill -9 pong → 拉起新实例 → ping 恢复收帧）
cd "$(dirname "$0")/../.."
source /opt/ros/jazzy/setup.bash
[ -f install/setup.bash ] && source install/setup.bash || source install/bench_ipc/setup.bash 2>/dev/null || true
mkdir -p results
OUT="results/bench3_$(date +%Y%m%d_%H%M%S).jsonl"
python3 bench_fault/fault_recovery.py --rounds 5 | tee "$OUT"
echo "done: $OUT"
