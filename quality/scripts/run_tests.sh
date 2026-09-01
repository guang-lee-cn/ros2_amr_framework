#!/bin/bash
# ── Build + Test ─────────────────────────────────────────────────────
# Usage:
#   quality/scripts/run_tests.sh           # coverage build (default)
#   quality/scripts/run_tests.sh asan      # AddressSanitizer + UBSan build
#   quality/scripts/run_tests.sh release   # release build (no extra flags)
set -eo pipefail

MODE="${1:-coverage}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QUALITY_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_DIR="$(dirname "$QUALITY_DIR")"
WS_DIR="$(dirname "$PROJECT_DIR")"

source /opt/ros/jazzy/setup.bash 2>/dev/null || true

case "$MODE" in
  coverage)
    # -fprofile-update=atomic：多线程测试（e2e 后台线程/MultiThreadedExecutor）
    # 并发写 gcda 计数器会撕裂出负值，geninfo 报 negative count（CI cyclonedds
    # 腿 2026-08-26 实证）；atomic 更新为官方解法
    CXX_FLAGS="--coverage -g -O0 -fprofile-update=atomic"
    LD_FLAGS="--coverage -fprofile-update=atomic"
    echo "[run_tests] Mode: coverage (profile-update=atomic)"
    ;;
  asan)
    CXX_FLAGS="-fsanitize=address,undefined -g -O1 -fno-omit-frame-pointer"
    LD_FLAGS="-fsanitize=address,undefined"
    echo "[run_tests] Mode: ASan + UBSan + LSan (suppressed: DDS/ROS2 internals)"
    export LSAN_OPTIONS="suppressions=$PROJECT_DIR/quality/lsan.supp"
    # 2026-08-30 WSL OOM 事故教训：ASAN 编译单 cc1plus ~2.1GB，12 路并行
    # 峰值 ~25GB 撞穿 22GB WSL 上限 → 全局 OOM 杀掉 IDE 桥接进程。
    # 限幅仅对 WSL/本地开发环境生效——CI runner 按自身资源配置：
    #   GitHub Actions ubuntu-latest = 4 核 16GB → -j4 是满并行（不减慢）
    #   本地 WSL2 22GB 12 核 → 必须限 -j4（OOM 纪律）
    if [ -n "${GITHUB_ACTIONS:-}" ]; then
      export CMAKE_BUILD_PARALLEL_LEVEL=${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc)}
      echo "[run_tests] CI 环境: 并行 = $(nproc)（runner 自有资源）"
    else
      export CMAKE_BUILD_PARALLEL_LEVEL=${CMAKE_BUILD_PARALLEL_LEVEL:-4}
      export MAKEFLAGS="-j${CMAKE_BUILD_PARALLEL_LEVEL}"
      echo "[run_tests] 本地环境: 并行限幅 $CMAKE_BUILD_PARALLEL_LEVEL（OOM 纪律）"
    fi
    ;;
  release)
    CXX_FLAGS="-O2 -DNDEBUG"
    LD_FLAGS=""
    echo "[run_tests] Mode: release"
    ;;
  *)
    echo "Unknown mode: $MODE (use: coverage | asan | release)"
    exit 1
    ;;
esac

echo "[run_tests] Building..."
cd "$WS_DIR"

# shellcheck disable=SC2086
colcon build \
  --packages-select ros2_robot_middleware \
  --cmake-clean-first \
  --cmake-args \
    -DCMAKE_CXX_FLAGS="$CXX_FLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="$LD_FLAGS" \
  --no-warn-unused-cli \
  2>&1 | tail -5
BUILD_RC=${PIPESTATUS[0]}
if [ "$BUILD_RC" -ne 0 ]; then
  echo "[run_tests] BUILD FAILED (exit $BUILD_RC)"
  exit 1
fi

echo "[run_tests] Running tests..."
source install/setup.bash 2>/dev/null || true
# test_motor_ctrl re-enabled (2026-07-31): action execute now runs with
# SpinHelper multi-threaded executor + PurePursuit final-approach fix.
colcon test \
  --packages-select ros2_robot_middleware \
  --return-code-on-test-failure \
  2>&1 | tail -3

echo "[run_tests] Done."
