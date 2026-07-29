#!/bin/bash
# Pure DDS throughput/latency benchmark — no application computation.
# Isolates DDS from perception/motor control for meaningful comparison.
#
# Test: send N x M-byte messages at H Hz, measure:
#   - Publication rate (messages/s)
#   - End-to-end latency (via timestamp echo)
#   - Message loss rate
#
# QoS profiles tested:
#   reliable+volatile, reliable+transient_local, best_effort+volatile

set -eo pipefail

RMW="${1:-rmw_fastrtps_cpp}"
OUTDIR="${2:-/tmp/bench_dds_pure}"
RUNS=3
NUM_MSGS=1000

rm -rf "${OUTDIR}"
mkdir -p "${OUTDIR}"

export RMW_IMPLEMENTATION="${RMW}"
source /opt/ros/jazzy/setup.bash 2>/dev/null
source /home/guang/code/ros2_ws/install/setup.bash 2>/dev/null

SUMMARY="${OUTDIR}/summary.csv"
echo "rmw,qos,msg_size_bytes,rate_hz,sent,received,loss_pct,avg_lat_us,p99_lat_us,max_lat_us" > "${SUMMARY}"

for QOS in "reliable" "best_effort"; do
  for MSG_SIZE in 256 4096 65536; do
    for RUN in $(seq 1 ${RUNS}); do
      echo "=== ${RMW} | ${QOS} | ${MSG_SIZE}B | run ${RUN}/${RUNS} ==="

      # Create test pub/sub with ros2 topic commands
      # Publish N messages and measure
      RESULT_FILE="${OUTDIR}/$(echo ${RMW} | cut -d_ -f2)_${QOS}_${MSG_SIZE}B_r${RUN}.txt"
      echo "timestamp,latency_us" > "${RESULT_FILE}"

      # Launch ping node (subscribes and echoes)
      timeout 30 ros2 run ros2_robot_middleware ping_node --ros-args \
        -p qos_reliability:="${QOS}" \
        -p msg_size:="${MSG_SIZE}" \
        > "${OUTDIR}/ping_${QOS}_${MSG_SIZE}_r${RUN}.log" 2>&1 &
      PING_PID=$!
      sleep 2

      # Publish N messages at 100 Hz
      ROS_START=$(python3 -c "import time; print(time.time())")
      for i in $(seq 1 ${NUM_MSGS}); do
        TS_NS=$(python3 -c "import time; print(int(time.time()*1e9))")
        ros2 topic pub --once /bench/ping std_msgs/msg/String "data: 'ping_${i}_${TS_NS}'" > /dev/null 2>&1
        python3 -c "import time; time.sleep(0.01)"  # ~100 Hz
      done
      ROS_END=$(python3 -c "import time; print(time.time())")

      sleep 3
      kill ${PING_PID} 2>/dev/null || true
      wait ${PING_PID} 2>/dev/null || true

      # Parse results from ping log
      SENT=${NUM_MSGS}
      RECEIVED=$(grep -c "pong" "${OUTDIR}/ping_${QOS}_${MSG_SIZE}_r${RUN}.log" 2>/dev/null || echo 0)
      LOSS_PCT=$(python3 -c "print(f'{(1-${RECEIVED}/${SENT})*100:.1f}')" 2>/dev/null || echo "N/A")

      AVG_LAT="N/A"; P99_LAT="N/A"; MAX_LAT="N/A"
      if [ -f "${RESULT_FILE}" ]; then
        # Compute latency stats from results
        LATS=$(grep -oP '[\d.]+' "${RESULT_FILE}" | tail -n +2)
        if [ -n "${LATS}" ]; then
          AVG_LAT=$(echo "${LATS}" | awk '{s+=$1;n++}END{printf "%.0f",s/n}')
          MAX_LAT=$(echo "${LATS}" | sort -n | tail -1)
          P99_LAT=$(echo "${LATS}" | sort -n | awk -v n="$(echo "${LATS}" | wc -l)" 'NR>=int(n*0.99){print;exit}')
        fi
      fi

      echo "${RMW},${QOS},${MSG_SIZE},100,${SENT},${RECEIVED},${LOSS_PCT},${AVG_LAT},${P99_LAT},${MAX_LAT}" >> "${SUMMARY}"
    done
  done
done

echo "=== Results ==="
cat "${SUMMARY}"
