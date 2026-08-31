#!/bin/bash
# ── Quality Gate: Build → Test → Coverage → Gate ────────────────────
# Usage: ./quality.sh [html]
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# 与 run_tests.sh 同源解析（其 colcon 构建根 = 本包目录的上一级：CI 里是
# $GITHUB_WORKSPACE/src，本地是 ~/code/ros2_ws/src）。旧版在 CI 里误用
# GITHUB_WORKSPACE 根 → lcov 抓空目录 → 覆盖率在 CI 从未真正测过
# （旧静默放行掩护了它，2026-08-25 B4 硬门禁首次暴露）。
WS_DIR="$(dirname "$PROJECT_DIR")"

BUILD_DIR="$WS_DIR/build/ros2_robot_middleware"
COV_DIR="$SCRIPT_DIR/data"
COV_INFO="/tmp/amr_cov.info"
COV_FILTERED="/tmp/amr_cov_filtered.info"
COV_FILE="$COV_DIR/coverage.txt"
COV_PREV="$COV_DIR/coverage.prev.txt"
COV_FULL="$COV_DIR/coverage_full.txt"

mkdir -p "$COV_DIR"

# ── Step 1: Build + Test ────────────────────────────────────────────
echo "[quality] step 1/2: build + test (mode: ${1:-coverage})..."
bash "$SCRIPT_DIR/scripts/run_tests.sh" "${1:-coverage}"

# ── Step 2: Coverage ────────────────────────────────────────────────
echo "[quality] step 2/2: coverage + gate..."
COV_FAIL=false

# B4 修复（2026-08-25 外部审计 P1-a）：lcov 失败不再静默放行——度量链断则门禁红。
# 同一 commit 曾出现 summary 84.5% 而 --list 明细全 ≤61.9% 的矛盾（lcov 2.0
# 报表 per-file 数字与 .info DA 数据不符），故明细/聚合/badge 一律由
# coverage_report.py 从 .info 直算，单一口径可复算；.info 原始件由 CI 上传归档。
#
# capture 并行化（2026-08-27 CI 耗时自查）：capture 占覆盖率链大头（CI 实测
# ~3m51s，geninfo 串行逐个跑 gcov；remove 仅 ~1s 非瓶颈）。lcov ≥2.0 的
# --parallel 本地 A/B 实测 19.6s→3.3s，.info 排序后逐字节一致；1.x 无此选项，
# 探测不到则回退串行（脚本不因此失败）。
LCOV_PAR=""
if lcov --help 2>&1 | grep -q -- '--parallel'; then
  LCOV_PAR="--parallel $(nproc)"
fi

# P0-I（三审 2026-08-31）：先 --initial 捕获全部 gcno（含零覆盖文件——
# supervisor/health_monitor 等从未进测试的目标不再从分母消失），再捕获
# 运行时 gcda，合并 = 真分母。数字会下降——那是真话。
lcov --initial --capture --directory "$BUILD_DIR" \
  --rc geninfo_gcov_all_blocks=0 \
  --output-file /tmp/amr_cov_initial.info >/dev/null 2>&1 || true

if ! lcov --capture --directory "$BUILD_DIR" \
  --output-file "$COV_INFO" \
  --ignore-errors empty,unused,mismatch,gcov \
  --rc geninfo_gcov_all_blocks=0 \
  $LCOV_PAR \
  >/dev/null; then
  echo "ERROR: lcov capture failed — 度量链断，门禁失败（不再静默放行）"
  exit 1
fi

# 合并（initial 非空时）：零覆盖文件以 0% 进分母
if [ -s /tmp/amr_cov_initial.info ]; then
  lcov -a /tmp/amr_cov_initial.info -a "$COV_INFO" \
    --output-file "$COV_INFO.merged" >/dev/null 2>&1 && \
    mv "$COV_INFO.merged" "$COV_INFO"
fi

if ! lcov --remove "$COV_INFO" \
    --ignore-errors empty,unused \
    '/usr/*' '/opt/*' '*/rosidl*' '*/gtest*' '*/build/*' \
    '*/quality/*' '*/main.cpp' \
    '*/fleet_manager*' '*/compute_container*' \
    '*/observability/log_worker*' '*/observability/log_event*' \
    --output-file "$COV_FILTERED" \
    >/dev/null; then
  echo "ERROR: lcov filter failed — 度量链断，门禁失败"
  exit 1
fi

if [ -s "$COV_FILTERED" ]; then
  LINE_COV=$(python3 "$SCRIPT_DIR/scripts/coverage_report.py" "$COV_FILTERED" "$COV_DIR" \
    | head -1 | grep -oE '[0-9]+\.[0-9]%')

  if [ -n "$LINE_COV" ]; then
    # Save（明细 coverage_full.txt 与 badge.json 已由 coverage_report.py 生成）
    if [ -f "$COV_FILE" ]; then cp "$COV_FILE" "$COV_PREV"; fi
    echo "$LINE_COV" > "$COV_FILE"

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
  echo "ERROR: filtered coverage info 为空 — 度量链断，门禁失败"
  exit 1
fi

echo ""
echo "Results: $COV_FILE"
echo "Detail:  $COV_FULL"
echo "Badge:   $COV_DIR/badge.json"

if [ "$COV_FAIL" = true ]; then
  echo ""
  echo "ERROR: Coverage dropped below previous run while under 80%."
  exit 1
fi
