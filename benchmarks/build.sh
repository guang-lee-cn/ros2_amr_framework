#!/usr/bin/env bash
# benchmarks 构建入口：colcon 不向包目录内递归（仓库根即包），须显式 base-path。
# 产物落在仓库根的 build/install 下，与主包共用（scripts/*.sh 均按此寻径）。
set -eo pipefail
cd "$(dirname "$0")/.."
source /opt/ros/jazzy/setup.bash
colcon build --base-paths benchmarks/bench_ipc \
  --cmake-args -DCMAKE_BUILD_TYPE=Release "$@"
