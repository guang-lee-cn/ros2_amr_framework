#!/bin/bash
# ── Quality Gate: Build → Test → Coverage → Gate ────────────────────
# Usage: ./quality.sh [html]
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

if [ -n "${GITHUB_WORKSPACE:-}" ]; then
  WS_DIR="$GITHUB_WORKSPACE"
else
  WS_DIR="$(dirname "$PROJECT_DIR")"
fi

BUILD_DIR="$WS_DIR/build/ros2_robot_middleware"
COV_DIR="$SCRIPT_DIR/data"
COV_INFO="/tmp/amr_cov.info"
COV_FILTERED="/tmp/amr_cov_filtered.info"
COV_FILE="$COV_DIR/coverage.txt"
COV_PREV="$COV_DIR/coverage.prev.txt"
COV_FULL="$COV_DIR/coverage_full.txt"

mkdir -p "$COV_DIR"

# ── Step 1: Build + Test ────────────────────────────────────────────
echo "[quality] step 1/2: build + test..."
bash "$SCRIPT_DIR/scripts/run_tests.sh"

# ── Step 2: Coverage ────────────────────────────────────────────────
echo "[quality] step 2/2: coverage + gate..."
HAS_COVERAGE=true
COV_FAIL=false

lcov --capture --directory "$BUILD_DIR" \
  --output-file "$COV_INFO" \
  --ignore-errors empty,unused,mismatch,gcov \
  --rc geninfo_gcov_all_blocks=0 \
  2>&1 | tail -1 || HAS_COVERAGE=false

if $HAS_COVERAGE && [ -s "$COV_INFO" ]; then
  lcov --remove "$COV_INFO" \
    --ignore-errors empty,unused \
    '/usr/*' '/opt/*' '*/rosidl*' '*/gtest*' '*/build/*' \
    '*/quality/*' '*/main.cpp' \
    '*/fleet_manager*' '*/compute_container*' \
    '*/observability/log_worker*' '*/observability/log_event*' \
    --output-file "$COV_FILTERED" \
    2>&1 | tail -1 || HAS_COVERAGE=false
fi

if $HAS_COVERAGE && [ -s "$COV_FILTERED" ]; then
  LINE_COV=$(lcov --summary "$COV_FILTERED" 2>&1 | grep "lines" | awk '{print $2}' || echo "N/A")

  if [ "$LINE_COV" != "N/A" ] && [ -n "$LINE_COV" ]; then
    # Save
    if [ -f "$COV_FILE" ]; then cp "$COV_FILE" "$COV_PREV"; fi
    echo "$LINE_COV" > "$COV_FILE"

    # Per-file detail
    lcov --list "$COV_FILTERED" 2>&1 \
      | grep "\.hpp\|\.cpp" | grep -v "test_" \
      | awk '{printf "%-70s %s\n", $1, $2}' \
      > "$COV_FULL" 2>/dev/null || true

    # Report
    echo ""
    echo "══════════════════════════════════════════════════"
    echo "  Line Coverage: $LINE_COV"
    echo "══════════════════════════════════════════════════"

    COV_NUM=${LINE_COV%%\%}
    if [ -f "$COV_PREV" ]; then
      PREV=$(cat "$COV_PREV")
      PREV_NUM=${PREV%%\%}
      DELTA=$(awk "BEGIN { printf \"%+.1f\", ${COV_NUM} - ${PREV_NUM} }")
      echo "  Previous:     $PREV"
      echo "  Delta:        ${DELTA}pp"

      if awk "BEGIN { exit (${COV_NUM} >= 80.0) ? 0 : 1 }"; then
        echo "  Gate:         SKIP (>= 80%)"
      else
        if awk "BEGIN { exit (${COV_NUM} >= ${PREV_NUM}) ? 0 : 1 }"; then
          echo "  Gate:         PASS (>= previous ${PREV})"
        else
          echo "  Gate:         FAIL — dropped from ${PREV} to ${LINE_COV} (<80%)"
          COV_FAIL=true
        fi
      fi
    else
      echo "  (first run)"
    fi

    # HTML
    if [[ "${1:-}" == "html" ]]; then
      genhtml "$COV_FILTERED" --output-directory "$COV_DIR/html" 2>&1 | tail -1 || true
      echo "  HTML: $COV_DIR/html/index.html"
    fi
  fi
else
  echo "[quality] coverage data unavailable — skipping gate"
fi

echo ""
echo "Results: $COV_FILE"
echo "Detail:  $COV_FULL"

if [ "$COV_FAIL" = true ]; then
  echo ""
  echo "ERROR: Coverage dropped below previous run while under 80%."
  exit 1
fi
