#!/bin/bash
# ── Build + Test (no coverage analysis) ──────────────────────────────
# Run from quality.sh or standalone:
#   quality/scripts/run_tests.sh
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QUALITY_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_DIR="$(dirname "$QUALITY_DIR")"
WS_DIR="$(dirname "$PROJECT_DIR")"

source /opt/ros/jazzy/setup.bash 2>/dev/null || true

echo "[run_tests] Building with coverage flags..."
cd "$WS_DIR"

colcon build \
  --packages-select ros2_robot_middleware \
  --cmake-clean-first \
  --cmake-args \
    -DCMAKE_CXX_FLAGS="--coverage -g -O0" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
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
