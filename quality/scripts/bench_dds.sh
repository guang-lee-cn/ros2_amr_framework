#!/bin/bash
# DDS Benchmark — compare Fast-DDS vs CycloneDDS under the same workload.
#
# Metrics collected from Prometheus HTTP endpoint (:9090/metrics):
#   - Sensor rates (lidar, imu, camera)
#   - Fusion/decision/motor/e2e latency histograms
#   - Degradation level, object count
#
# Usage:
#   bench_dds.sh <rmw_impl> <duration_s> <output_dir>
#
# Example:
#   bench_dds.sh rmw_fastrtps_cpp 60 results/fastdds
#   bench_dds.sh rmw_cyclonedds_cpp 60 results/cyclonedds

set -eo pipefail

RMW="${1:?Usage: bench_dds.sh <rmw_impl> <duration_s> <output_dir>}"
DURATION="${2:?}"
OUTDIR="${3:?}"

PROM_URL="http://localhost:9090/metrics"
LAUNCH_FILE="ros2_robot_middleware system.launch.py"
METRICS_CSV="${OUTDIR}/metrics.csv"
SUMMARY="${OUTDIR}/summary.txt"

mkdir -p "${OUTDIR}"

echo "=== DDS Benchmark ==="
echo "RMW: ${RMW}"
echo "Duration: ${DURATION}s"
echo "Output: ${OUTDIR}"

# Launch system in background
export RMW_IMPLEMENTATION="${RMW}"
source /opt/ros/jazzy/setup.bash
source /home/guang/code/ros2_ws/install/setup.bash

echo "Starting system..."
timeout ${DURATION} ros2 launch ${LAUNCH_FILE} > /tmp/bench_launch.log 2>&1 &
LAUNCH_PID=$!

# Wait for system to start and metrics to be available
sleep 8

# Collect metrics
echo "timestamp,rmw,lidar_rate,imu_rate,camera_rate,fusion_lat_us,decision_lat_us,motor_lat_us,e2e_lat_us,degradation,objects,fusion_cycles" > "${METRICS_CSV}"

ELAPSED=0
while [ ${ELAPSED} -lt $((DURATION - 10)) ]; do
  METRICS=$(curl -s "${PROM_URL}" 2>/dev/null || echo "")

  if [ -z "${METRICS}" ]; then
    echo "WARN: metrics endpoint not ready at t=${ELAPSED}s"
    sleep 1
    ELAPSED=$((ELAPSED + 1))
    continue
  fi

  # Parse Prometheus text format
  LIDAR_RATE=$(echo "${METRICS}" | grep 'amr_sensor_rate_hz{sensor="lidar"}' | awk '{print $NF}')
  IMU_RATE=$(echo "${METRICS}" | grep 'amr_sensor_rate_hz{sensor="imu"}' | awk '{print $NF}')
  CAMERA_RATE=$(echo "${METRICS}" | grep 'amr_sensor_rate_hz{sensor="camera"}' | awk '{print $NF}')

  FUSION_LAT=$(echo "${METRICS}" | grep 'amr_fusion_latency_seconds_sum' | awk '{print $NF}')
  DECISION_LAT=$(echo "${METRICS}" | grep 'amr_decision_latency_seconds_sum' | awk '{print $NF}')
  MOTOR_LAT=$(echo "${METRICS}" | grep 'amr_motor_latency_seconds_sum' | awk '{print $NF}')
  E2E_LAT=$(echo "${METRICS}" | grep 'amr_e2e_latency_seconds_sum' | awk '{print $NF}')

  FUSION_COUNT=$(echo "${METRICS}" | grep 'amr_fusion_latency_seconds_count' | awk '{print $NF}')
  DECISION_COUNT=$(echo "${METRICS}" | grep 'amr_decision_latency_seconds_count' | awk '{print $NF}')
  MOTOR_COUNT=$(echo "${METRICS}" | grep 'amr_motor_latency_seconds_count' | awk '{print $NF}')
  E2E_COUNT=$(echo "${METRICS}" | grep 'amr_e2e_latency_seconds_count' | awk '{print $NF}')

  DEGRADATION=$(echo "${METRICS}" | grep 'amr_degradation_level' | awk '{print $NF}')
  OBJECTS=$(echo "${METRICS}" | grep 'amr_object_count' | awk '{print $NF}')
  FUSION_CYCLES=$(echo "${METRICS}" | grep 'amr_fusion_cycles_total' | awk '{print $NF}')

  # Average latency (convert sum from seconds to microseconds)
  if [ -n "${FUSION_LAT}" ] && [ -n "${FUSION_COUNT}" ] && [ "${FUSION_COUNT}" != "0" ]; then
    FUSION_AVG_US=$(echo "scale=1; ${FUSION_LAT} * 1000000 / ${FUSION_COUNT}" | bc)
  else
    FUSION_AVG_US="0"
  fi

  if [ -n "${MOTOR_LAT}" ] && [ -n "${MOTOR_COUNT}" ] && [ "${MOTOR_COUNT}" != "0" ]; then
    MOTOR_AVG_US=$(echo "scale=1; ${MOTOR_LAT} * 1000000 / ${MOTOR_COUNT}" | bc)
  else
    MOTOR_AVG_US="0"
  fi

  if [ -n "${E2E_LAT}" ] && [ -n "${E2E_COUNT}" ] && [ "${E2E_COUNT}" != "0" ]; then
    E2E_AVG_US=$(echo "scale=1; ${E2E_LAT} * 1000000 / ${E2E_COUNT}" | bc)
  else
    E2E_AVG_US="0"
  fi

  echo "${ELAPSED},${RMW},${LIDAR_RATE:-0},${IMU_RATE:-0},${CAMERA_RATE:-0},${FUSION_AVG_US},0,${MOTOR_AVG_US},${E2E_AVG_US},${DEGRADATION:-0},${OBJECTS:-0},${FUSION_CYCLES:-0}" >> "${METRICS_CSV}"

  sleep 2
  ELAPSED=$((ELAPSED + 2))
done

# Wait for launch to finish
wait ${LAUNCH_PID} 2>/dev/null || true

# Compute summary statistics
echo "=== Summary: ${RMW} ===" > "${SUMMARY}"
echo "Duration: ${DURATION}s" >> "${SUMMARY}"
echo "" >> "${SUMMARY}"

# Average sensor rates
awk -F',' 'NR>1 {lr+=$3; ir+=$4; cr+=$5; n++} END {
  printf "Avg Sensor Rates:\n  LiDAR:  %.1f Hz\n  IMU:    %.1f Hz\n  Camera: %.1f Hz\n", lr/n, ir/n, cr/n
}' "${METRICS_CSV}" >> "${SUMMARY}"

# Average latencies
awk -F',' 'NR>1 && $6>0 {fl+=$6; fn++} NR>1 && $8>0 {ml+=$8; mn++} NR>1 && $9>0 {el+=$9; en++} END {
  printf "Avg Latency (μs):\n  Fusion: %.0f\n  Motor:  %.0f\n  E2E:    %.0f\n", fl/fn, ml/mn, el/en
}' "${METRICS_CSV}" >> "${SUMMARY}"

# Degradation stability
awk -F',' 'NR>1 {if($10==0) ok++; n++} END {printf "Degradation Stability: %.0f%% at level 0\n", ok/n*100}' "${METRICS_CSV}" >> "${SUMMARY}"

cat "${SUMMARY}"
echo ""
echo "Raw metrics: ${METRICS_CSV}"
echo "Summary: ${SUMMARY}"
