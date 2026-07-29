#!/bin/bash
# DDS benchmark using community-standard ddsperf (CycloneDDS built-in tool).
# Replaces our custom ping/pong with the industry-standard tool.
set -eo pipefail

source /opt/ros/jazzy/setup.bash 2>/dev/null
source /home/guang/code/ros2_ws/install/setup.bash 2>/dev/null

echo "=== Environment ==="
echo "RMW: ${RMW_IMPLEMENTATION:-rmw_fastrtps_cpp (default)}"
echo "ddsperf: $(which ddsperf)"
echo ""

run_rtt() {
  local RMW=$1 QOS_FLAG=$2 SIZE=$3 DUR=$4
  export RMW_IMPLEMENTATION=${RMW}
  echo "=== ${RMW} | ${QOS_FLAG:-reliable} | ${SIZE}B | ${DUR}s ==="

  ddsperf ${QOS_FLAG} pong > /tmp/ddsperf_pong.log 2>&1 &
  PONG_PID=$!
  sleep 2

  timeout $((DUR + 5)) ddsperf ${QOS_FLAG} ping 100Hz size ${SIZE} -D${DUR} 2>&1 || true

  kill ${PONG_PID} 2>/dev/null
  wait ${PONG_PID} 2>/dev/null || true
  echo ""
}

for DUR in 10; do
  # CycloneDDS tests
  for QOS in "" "-u"; do
    QOS_LABEL=${QOS:--reliable}
    for SIZE in 256 4096 65536; do
      run_rtt rmw_cyclonedds_cpp "${QOS}" ${SIZE} ${DUR}
    done
  done

  # Fast-DDS tests
  for SIZE in 256 4096 65536; do
    run_rtt rmw_fastrtps_cpp "" ${SIZE} ${DUR}
  done
done
