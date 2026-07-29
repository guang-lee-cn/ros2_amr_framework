#!/bin/bash
# DDS RTT micro-benchmark — ping/pong latency under varying QoS settings.
set -eo pipefail

source /opt/ros/jazzy/setup.bash
source /home/guang/code/ros2_ws/install/setup.bash

echo "rmw,qos,size,received,loss_pct,avg_us,p50_us,p99_us,max_us"

for RMW in rmw_fastrtps_cpp rmw_cyclonedds_cpp; do
  export RMW_IMPLEMENTATION=${RMW}
  for QOS in reliable best_effort; do
    for SIZE in 256 4096 65536; do
      # Start pong
      ros2 run ros2_robot_middleware bench_pong --ros-args -p qos:=${QOS} \
        > /tmp/pong.log 2>&1 &
      PONG_PID=$!
      sleep 2  # wait for DDS discovery

      # Run ping
      OUTPUT=$(ros2 run ros2_robot_middleware bench_ping --ros-args \
        -p rate:=100 -p count:=200 -p size:=${SIZE} -p qos:=${QOS} 2>&1) || true

      kill ${PONG_PID} 2>/dev/null
      wait ${PONG_PID} 2>/dev/null || true

      # Parse output
      RESULT=$(echo "${OUTPUT}" | grep "BENCH_RESULT:" || echo "BENCH_RESULT: no_output")
      RECV=$(echo "${RESULT}" | grep -oP 'received=\K\d+')
      AVG=$(echo "${RESULT}" | grep -oP 'avg_us=\K\d+')
      P50=$(echo "${RESULT}" | grep -oP 'p50_us=\K\d+')
      P99=$(echo "${RESULT}" | grep -oP 'p99_us=\K\d+')
      MAX=$(echo "${RESULT}" | grep -oP 'max_us=\K\d+')
      LOSS=$(python3 -c "print(f'{(1-${RECV:-0}/200)*100:.1f}')" 2>/dev/null || echo "N/A")

      RMW_SHORT=$(echo ${RMW} | sed 's/rmw_//;s/_cpp//')
      echo "${RMW_SHORT},${QOS},${SIZE},${RECV:-0},${LOSS},${AVG:-0},${P50:-0},${P99:-0},${MAX:-0}"
      sleep 1
    done
  done
done
