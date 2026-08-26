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

echo "=== python syntax (launch/ + scripts/) ==="
# 2026-08-25 审计 P1-a 落地：system_secure.launch.py 双逗号语法错存在数月
# 无人发现（从未运行过）——语法层防线进静态分析，不再依赖"碰巧运行"
LINT_FAIL=0
while IFS= read -r -d '' pyf; do
  if ! python3 -c "import ast,sys; ast.parse(open(sys.argv[1]).read())" "$pyf" 2>/dev/null; then
    echo "SYNTAX FAIL: $pyf"
    python3 -c "import ast,sys; ast.parse(open(sys.argv[1]).read())" "$pyf" 2>&1 | tail -1
    LINT_FAIL=1
  fi
done < <(find "$PROJECT_DIR/launch" "$PROJECT_DIR/scripts" -name '*.py' -print0 2>/dev/null)
if [ "$LINT_FAIL" -ne 0 ]; then
  echo "FAIL — python syntax errors found"
  exit 1
fi
echo "python files: syntax OK"

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
# 注：grep -c 无匹配时自打印 0 但退出码 1——set -e 会误杀脚本，需 || true
# 掩退出码（原作者的 `|| echo 0` 掩对了退出码但多打一个 0 致比较报错）
ERRORS=$(grep -c "(error)" "$TMP_LOG" 2>/dev/null || true); ERRORS=${ERRORS:-0}
WARNINGS=$(grep -c "(warning)" "$TMP_LOG" 2>/dev/null || true); WARNINGS=${WARNINGS:-0}

cat "$TMP_LOG"
rm -f "$TMP_LOG"

echo ""
echo "cppcheck: errors=$ERRORS warnings=$WARNINGS"

if [ "$ERRORS" -gt 0 ]; then
  echo "FAIL — cppcheck found $ERRORS error(s)"
  exit 1
fi

echo "Static analysis passed."
