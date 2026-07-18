#!/bin/bash
# ------------------------------------------------------------------
# Run tests with coverage instrumentation, save results, compare delta.
#
# Output:
#   test/coverage/coverage.txt       — latest run (line %)
#   test/coverage/coverage.prev.txt  — previous run (for comparison)
#   test/coverage/coverage_full.txt  — per-file breakdown (latest)
#
# Usage:
#   ./test/coverage.sh           # run + print delta vs previous
#   ./test/coverage.sh html      # also generate HTML report
# ------------------------------------------------------------------
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"   # ros2_amr_framework

# Workspace root: CI uses GITHUB_WORKSPACE, local uses heuristic
if [ -n "${GITHUB_WORKSPACE:-}" ]; then
  WS_DIR="$GITHUB_WORKSPACE"
else
  WS_DIR="$(dirname "$SCRIPT_DIR")"           # ros2_ws root (project is in src/)
fi

BUILD_DIR="$WS_DIR/build/ros2_robot_middleware"
COV_DIR="$SCRIPT_DIR/test/coverage"
COV_INFO="/tmp/amr_cov.info"
COV_FILTERED="/tmp/amr_cov_filtered.info"
COV_FILE="$COV_DIR/coverage.txt"
COV_PREV="$COV_DIR/coverage.prev.txt"
COV_FULL="$COV_DIR/coverage_full.txt"

mkdir -p "$COV_DIR"

# ── Source ROS2 ──────────────────────────────────────────────────────
# (must be before set -u — ROS2 scripts reference unbound vars)
source /opt/ros/jazzy/setup.bash 2>/dev/null || true

# ── [1/5] Build with coverage ────────────────────────────────────────
echo "[1/5] Building with --coverage ..."
cd "$WS_DIR"

colcon build \
  --packages-select ros2_robot_middleware \
  --cmake-clean-first \
  --cmake-args \
    -DCMAKE_CXX_FLAGS="--coverage -g -O0" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
  --no-warn-unused-cli \
  2>&1 | grep -E "Finished|Failed|Summary" || true

# ── [2/5] Run tests ─────────────────────────────────────────────────
echo "[2/5] Running tests ..."
source install/setup.bash 2>/dev/null || true
colcon test \
  --packages-select ros2_robot_middleware \
  --return-code-on-test-failure \
  2>&1 | grep -E "Finished|Failed|Summary" || true

# ── [3/5] Collect coverage ──────────────────────────────────────────
echo "[3/5] Collecting coverage data ..."
HAS_COVERAGE=true

lcov --capture --directory "$BUILD_DIR" \
  --output-file "$COV_INFO" \
  --ignore-errors empty,unused,mismatch,gcov \
  --rc geninfo_gcov_all_blocks=0 \
  2>&1 | tail -1 || HAS_COVERAGE=false

# ── [4/5] Filter to project code ────────────────────────────────────
if $HAS_COVERAGE && [ -s "$COV_INFO" ]; then
  lcov --remove "$COV_INFO" \
    --ignore-errors empty,unused \
    '/usr/*' '/opt/*' '*/rosidl*' '*/gtest*' '*/build/*' \
    '*/test/*' '*/main.cpp' \
    '*/fleet_manager*' '*/compute_container*' \
    '*/observability/log_worker*' '*/observability/log_event*' \
    --output-file "$COV_FILTERED" \
    2>&1 | tail -1 || HAS_COVERAGE=false
fi

# ── [5/5] Save + compare ────────────────────────────────────────────
if $HAS_COVERAGE && [ -s "$COV_FILTERED" ]; then
  LINE_COV=$(lcov --summary "$COV_FILTERED" 2>&1 | grep "lines" | awk '{print $2}' || echo "N/A")
else
  LINE_COV="N/A"
  echo "[coverage] lcov data unavailable — skipping coverage report"
fi

if $HAS_COVERAGE && [ "$LINE_COV" != "N/A" ]; then
  # Save previous as .prev
  if [ -f "$COV_FILE" ]; then
    cp "$COV_FILE" "$COV_PREV"
  fi

  # Save latest
  echo "$LINE_COV" > "$COV_FILE"

  # Per-file detail
  lcov --list "$COV_FILTERED" 2>&1 \
    | grep "\.hpp\|\.cpp" | grep -v "test_" \
    | awk '{printf "%-70s %s\n", $1, $2}' \
    > "$COV_FULL" 2>/dev/null || true

  # ── Report ──────────────────────────────────────────────────────────
  echo ""
  echo "══════════════════════════════════════════════════"
  echo "  Line Coverage: $LINE_COV"
  echo "══════════════════════════════════════════════════"

  if [ -f "$COV_PREV" ]; then
    PREV=$(cat "$COV_PREV")
    DELTA=$(awk "BEGIN { printf \"%+.1f\", ${LINE_COV%\%} - ${PREV%\%} }")
    echo "  Previous:     $PREV"
    echo "  Delta:        ${DELTA}pp"
    if awk "BEGIN { exit (${LINE_COV%\%} >= ${PREV%\%}) ? 0 : 1 }"; then
      echo "  Trend:        ▲ UP"
    else
      echo "  Trend:        ▼ DOWN"
    fi
  else
    echo "  (first run — no baseline to compare)"
  fi
  echo ""

  if [ -f "$COV_FULL" ]; then
    echo "Per-file top/bottom 5:"
    head -5 "$COV_FULL"
    echo "..."
    tail -5 "$COV_FULL"
  fi

  # ── HTML (optional) ────────────────────────────────────────────────
  if [[ "${1:-}" == "html" ]]; then
    OUT_DIR="$COV_DIR/html"
    mkdir -p "$OUT_DIR"
    genhtml "$COV_FILTERED" --output-directory "$OUT_DIR" 2>&1 | tail -1 || true
    echo "HTML: $OUT_DIR/index.html"
  fi
fi

echo ""
echo "Results saved: $COV_FILE"
echo "Full detail:   $COV_FULL"
