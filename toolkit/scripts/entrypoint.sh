#!/bin/bash
set -e

NODE="${1:?Usage: entrypoint.sh <node_name>}"
# 直通名特例：无 _node 后缀的可执行（compute_container 不拼后缀）
case "$NODE" in
  compute_container|scene_simulator|amr_supervisor|ota_agent|fusion_standalone)
    NODE_EXEC="$NODE" ;;
  *)
    NODE_EXEC="${NODE}_node" ;;
esac

source /opt/ros/jazzy/setup.bash
source /ws/install/setup.bash

exec "/ws/install/ros2_robot_middleware/lib/ros2_robot_middleware/${NODE_EXEC}"
