#!/usr/bin/env bash
# 节点↔进程映射：ros2 node list 交叉 /proc 进程表
# 解决的缺口：ros2 node list 不显示 PID——运维要知道杀哪个进程、看哪个线程
# 用法: ./scripts/node_map.sh （需已 source ROS 环境与工作区）
set -o pipefail

echo "═══ ROS 节点清单 ═══"
ros2 node list 2>/dev/null || { echo "(ros2 不可用：先 source 环境)"; exit 1; }
echo

echo "═══ ROS 相关进程（PID / 线程数 / CPU% / 命令）═══"
for pid in $(pgrep -f 'ros2_robot_middleware|ros2 run|ros2 launch' | sort -n); do
  [ -r /proc/$pid/cmdline ] || continue
  # 跳过编排脚本自身（pgrep -f 会匹配到 shell 包装）
  grep -qE 'zcode|node_map|/bin/(ba)?sh -c' "/proc/$pid/cmdline" 2>/dev/null && continue
  cmd=$(tr '\0' ' ' < /proc/$pid/cmdline | cut -c1-72)
  [ -n "$cmd" ] || continue
  threads=$(ls /proc/$pid/task 2>/dev/null | wc -l)
  cpu=$(ps -o %cpu= -p "$pid" 2>/dev/null | tr -d ' ')
  printf "%-8s %-6s %-6s %s\n" "$pid" "$threads" "${cpu:-?}%" "$cmd"
done
echo

echo "═══ 启发式映射（节点名 ↔ 进程命令行）═══"
for node in $(ros2 node list 2>/dev/null | tr -d '/'); do
  [ -z "$node" ] && continue
  hit=$(pgrep -af 'ros2_robot_middleware' | grep -m1 "$node" || true)
  if [ -n "$hit" ]; then
    printf "%-22s → %s\n" "$node" "$(echo "$hit" | cut -c1-60)"
  else
    printf "%-22s → (在多节点容器内，命令行不可见——见下方提示)\n" "$node"
  fi
done
echo
echo "提示: 手排组合容器(compute_container)的内部节点对 ros2 component list 不可见；"
echo "      迁 rclcpp_components 容器后即可用 ros2 component list 原生观测。"
echo "提示: 回调↔线程映射用 ros2_tracing(LTTng)——callback 事件自带线程号。"
