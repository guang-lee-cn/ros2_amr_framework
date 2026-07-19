#!/bin/bash
# ── Static Analysis: cppcheck + clang-tidy ───────────────────────────
# Run manually: ./quality/scripts/static_analysis.sh
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QUALITY_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_DIR="$(dirname "$QUALITY_DIR")"
INCLUDE_DIR="$PROJECT_DIR/include"
SRC_DIR="$PROJECT_DIR/src"

echo "=== cppcheck ==="
cppcheck --enable=all --suppress=missingIncludeSystem \
  --suppress=unmatchedSuppression --suppress=unusedFunction \
  --inline-suppr \
  -I "$INCLUDE_DIR" \
  "$SRC_DIR" "$INCLUDE_DIR" \
  2>&1 | grep -v "^Checking" || true

echo ""
echo "=== clang-tidy ==="
if command -v clang-tidy &>/dev/null; then
  find "$SRC_DIR" "$INCLUDE_DIR" -name "*.cpp" -o -name "*.hpp" \
    | xargs clang-tidy -p "$PROJECT_DIR/build/ros2_robot_middleware" \
        --quiet 2>&1 | head -50 || true
else
  echo "(clang-tidy not installed — skip)"
fi

echo ""
echo "Static analysis done."
