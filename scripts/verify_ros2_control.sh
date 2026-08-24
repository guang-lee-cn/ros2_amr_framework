#!/usr/bin/env bash
# U4 验证链：cmd_vel → diff_drive_controller → DiffDriveSystem 插件 → /joint_states
# 外加 claim 独占实证：第二个控制器抢同一 velocity 接口，预期被 controller_manager 拒绝。
# 前置：sudo apt install ros-jazzy-ros2-control ros-jazzy-ros2-controllers
#        且本包已 colcon build 并 source install/setup.bash
set -uo pipefail
cd "$(dirname "$0")/.."
PKG=ros2_robot_middleware
SHARE=install/$PKG/share/$PKG

# 入口清场：杀掉残留的 cm/rsp（上轮异常退出会留孤儿，双 cm 并存会让 spawner 打错实例）
pkill -f "controller_manager/ros2_control_node" 2>/dev/null
pkill -x robot_state_publisher 2>/dev/null
sleep 1

echo "== 1. 硬件接口清单 =="
timeout 5 ros2 control list_hardware_interfaces >/dev/null 2>&1 || true

echo "== 2. 启动 robot_state_publisher + controller_manager（后台） =="
# Jazzy 的 cm 从 /robot_description 话题取描述（不吃参数），必须先起 rsp
ros2 run robot_state_publisher robot_state_publisher \
  --ros-args -p robot_description:="$(cat "$SHARE/urdf/diff_drive_ros2_control_test.urdf")" \
  > /tmp/rsp_verify.log 2>&1 &
RSP_PID=$!

ros2 run controller_manager ros2_control_node \
  --ros-args \
  --params-file "$SHARE/config/diff_drive_ros2_control.yaml" \
  > /tmp/cm_ros2_control.log 2>&1 &
CM_PID=$!
sleep 3

echo "== 3. 激活 joint_state_broadcaster + diff_drive_controller =="
ros2 run controller_manager spawner joint_state_broadcaster
ros2 run controller_manager spawner diff_drive_controller
sleep 1

echo "== 4. 发 cmd_vel（前进 0.2 m/s；Jazzy 的 ddc 顶层话题收 TwistStamped） =="
timeout 3 ros2 topic pub -r 10 /diff_drive_controller/cmd_vel geometry_msgs/msg/TwistStamped \
  '{twist: {linear: {x: 0.2}}}' >/dev/null 2>&1 &

sleep 3
echo "== 5. /joint_states 采样（轮速应非零） =="
timeout 3 ros2 topic echo --once /joint_states

echo "== 6. 控制器/接口状态 =="
ros2 control list_controllers
ros2 control list_hardware_interfaces

echo "== 7. claim 独占实证：加载抢接口的第二个控制器 =="
ros2 run controller_manager spawner claim_conflict_controller 2>&1 | tail -5
echo "（预期上方报 command interface 已被认领/无法 claim 的错误——这就是独占语义）"

echo "== 8. 清理 =="
kill $CM_PID $RSP_PID 2>/dev/null
echo "完整日志: /tmp/cm_ros2_control.log /tmp/rsp_verify.log"
