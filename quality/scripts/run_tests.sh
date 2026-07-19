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
    CXX_FLAGS="--coverage -g -O0"
    LD_FLAGS="--coverage"
    echo "[run_tests] Mode: coverage"
    ;;
  asan)
    CXX_FLAGS="-fsanitize=address,undefined -g -O1 -fno-omit-frame-pointer"
    LD_FLAGS="-fsanitize=address,undefined"
    echo "[run_tests] Mode: ASan + UBSan + LSan (suppressed: DDS/ROS2 internals)"
    export LSAN_OPTIONS="suppressions=$PROJECT_DIR/quality/lsan.supp"
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
colcon test \
  --packages-select ros2_robot_middleware \
  --return-code-on-test-failure \
  2>&1 | tail -3

echo "[run_tests] Done."
