#!/bin/bash
# ── Static Analysis: cppcheck ────────────────────────────────────────
# Run before tests — exits non-zero on real errors (not style/info).
# CI: runs as blocking step before test.
# Local: ./quality/scripts/static_analysis.sh
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QUALITY_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_DIR="$(dirname "$QUALITY_DIR")"
INCLUDE_DIR="$PROJECT_DIR/include"
SRC_DIR="$PROJECT_DIR/src"

echo "=== cppcheck ==="

TMP_LOG=$(mktemp)
cppcheck --enable=warning,performance,portability \
  --suppress=missingIncludeSystem \
  --suppress=unmatchedSuppression \
  --suppress=unusedFunction \
  --suppress=missingInclude \
  --suppress=useStlAlgorithm \
  --inline-suppr \
  -I "$INCLUDE_DIR" \
  "$SRC_DIR" "$INCLUDE_DIR" \
  2>"$TMP_LOG" || true

# Only fail on errors (not style/info/performance notes)
ERRORS=$(grep -c "(error)" "$TMP_LOG" 2>/dev/null || echo 0)
WARNINGS=$(grep -c "(warning)" "$TMP_LOG" 2>/dev/null || echo 0)

cat "$TMP_LOG"
rm -f "$TMP_LOG"

echo ""
echo "cppcheck: errors=$ERRORS warnings=$WARNINGS"

if [ "$ERRORS" -gt 0 ]; then
  echo "FAIL — cppcheck found $ERRORS error(s)"
  exit 1
fi

echo "Static analysis passed."
