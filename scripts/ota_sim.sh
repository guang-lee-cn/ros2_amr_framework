#!/usr/bin/env bash
# A/B 双分区 OTA 端到端模拟（目录级，无 Yocto 也能演示完整机制）
# 用法: ota_sim.sh <新版本号> <ok|fail>    # ok/fail = 重启后健康检查结果
# 演示点：① 原子标记切换（危险窗口最小化）② 健康门失败自动回滚（回滚时刻）
set -eo pipefail
STATE="${OTA_SIM_DIR:-/tmp/amr_ota_sim}"
NEW_VER="$1"; HEALTH="$2"
[ -z "$NEW_VER" ] || [ -z "$HEALTH" ] && { echo "用法: $0 <新版本号> <ok|fail>"; exit 1; }

other() { [ "$1" = A ] && echo B || echo A; }

# ── 初始化（首次运行）：A 槽 v10（活动），B 槽 v9，引导标记指向 A ──
if [ ! -f "$STATE/boot_target" ]; then
  mkdir -p "$STATE/slots/slotA" "$STATE/slots/slotB"
  echo 10 > "$STATE/slots/slotA/version.txt"
  echo 9  > "$STATE/slots/slotB/version.txt"
  echo "A" > "$STATE/boot_target"
  echo "[init] slotA=v10(活动) slotB=v9 引导标记=A"
fi

ACTIVE=$(cat "$STATE/boot_target")
INACTIVE=$(other "$ACTIVE")
ACTIVE_VER=$(cat "$STATE/slots/slot$ACTIVE/version.txt")
echo "[现状] 活动槽=$ACTIVE(v$ACTIVE_VER) 非活动槽=$INACTIVE → 目标 v$NEW_VER 健康预期=$HEALTH"

# ── 1. 防降级检查 ──
SEC_FILE="$STATE/security_counter"
[ -f "$SEC_FILE" ] || echo 8 > "$SEC_FILE"
SEC=$(cat "$SEC_FILE")
if [ "$NEW_VER" -lt "$SEC" ]; then
  echo "[拒绝] v$NEW_VER < 安全计数器 $SEC（防降级），分区未动"; exit 2
fi

# ── 2. 安装：只写非活动槽（不变量 I1） ──
echo "$NEW_VER" > "$STATE/slots/slot$INACTIVE/version.txt"
echo "[安装] v$NEW_VER → slot$INACTIVE（活动槽 $ACTIVE 未被触碰）"

# ── 3. 原子切换引导标记（危险窗口 = 这一行） ──
echo "$INACTIVE" > "$STATE/boot_target"
echo "[切换] 引导标记: $ACTIVE → $INACTIVE（原子写，断电安全点）"

# ── 4. 重启进入候选分区 + 健康门 ──
BOOTED=$(cat "$STATE/boot_target")
BOOTED_VER=$(cat "$STATE/slots/slot$BOOTED/version.txt")
echo "[重启] bootloader 读标记 → 启动 slot$BOOTED (v$BOOTED_VER)，健康门开始计时…"

if [ "$HEALTH" = ok ]; then
  # 5a. 健康通过 → 提交（安全计数器前移）
  [ "$NEW_VER" -gt "$SEC" ] && echo "$NEW_VER" > "$SEC_FILE"
  echo "[提交] ✓ COMMITTED: slot$BOOTED v$BOOTED_VER 固化为活动槽"
else
  # 5b. 健康失败/超时 → 回滚标记，重启回旧槽（回滚时刻！）
  echo "$ACTIVE" > "$STATE/boot_target"
  echo "[回滚] ✗ 健康门未过 → 引导标记拨回 $ACTIVE，重启 → v$ACTIVE_VER"
  echo "[回滚] ROLLED_BACK: 新版本未提交前随时可退（不变量 I3）"
fi
