// Production compute container — fusion + decision + motor_ctrl in one process.
// Zero-copy communication via shared_ptr between nodes (no DDS serialization).
// Sensor drivers (lidar/imu/camera) remain independent for fault isolation.
//
// Zero-copy (FIXED 2026-08-24): the intra-process TODO from 2026-08-15 is done.
// Shared NodeOptions().use_intra_process_comms(true) + unique_ptr publishes on
// the hot path (PerceptionObjects, path). Verified on Jazzy empirically via
// perf instrumentation (AMR_PERF_INSTRUMENTATION=ON, :9091/metrics, 2 轮):
//   fusion:tick P99 512µs → 256µs（两轮复现，2×）
//   fusion:tick P50 处于量化桶边界 128/256 抖动（方向一致，不宣称幅度）
// 空场景消息最小，序列化收益随载荷增长——大消息量级见 benchmarks 的
// intra 实测（1MB 比 DDS 快 183×，benchmarks/docs/results-2026-08-20.md）。
// LifecycleNode + intra-process 在 Jazzy 可用——本次实测即旧 TODO 要求的验证。
//
// Process layout (production):
//   compute_container (PID 1) ─── fusion → decision → motor_ctrl (shared memory)
//   lidar_node          (PID 2) ─── independent, driver fault isolation
//   imu_node            (PID 3) ─── independent, driver fault isolation
//   camera_node         (PID 4) ─── independent, driver fault isolation
//   health_monitor_node (PID 5) ─── independent, must not share fate with monitored
//   fleet_manager_node  (PID 6) ─── independent, cross-AMR

#include "ros2_robot_middleware/infrastructure/decision_node.hpp"
#include "ros2_robot_middleware/infrastructure/fusion_node.hpp"
#include "ros2_robot_middleware/infrastructure/motor_ctrl_node.hpp"
#include "ros2_robot_middleware/infrastructure/prometheus_http_server.hpp"
#include "generated/perf_instrumentation.hpp"
#include "ros2_robot_middleware/observability/spdlog_adapter.hpp"

#include <memory>
#include <sstream>
#include <rclcpp/rclcpp.hpp>

#ifdef AMR_PERF_INSTRUMENTATION
namespace {

// Perf instrumentation snapshot → Prometheus text format.
// Exposes AMR_PERF_PHASE data (phase latency avg/p50/p99) from this process.
// Only built when AMR_PERF_INSTRUMENTATION=ON — production builds eliminate
// the instrumentation entirely (see generated/perf_instrumentation.hpp).
std::string perf_metrics_text() {
  std::ostringstream out;
  for (const auto &s : amr::observability::PerfRegistry::instance().snapshots()) {
    out << "# HELP amr_phase_" << s.name << " Phase latency (us)\n";
    out << "# TYPE amr_phase_" << s.name << " gauge\n";
    out << "amr_phase_" << s.name << "_count " << s.count << "\n";
    out << "amr_phase_" << s.name << "_avg_us " << s.avg_us() << "\n";
    out << "amr_phase_" << s.name << "_p50_us " << s.p50_us << "\n";
    out << "amr_phase_" << s.name << "_p99_us " << s.p99_us << "\n";
  }
  return out.str();
}

}  // namespace
#endif  // AMR_PERF_INSTRUMENTATION

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);

  // Observability: spdlog async logger (replaces ring buffer)
  amr::observability::Logging::init_spdlog();

#ifdef AMR_PERF_INSTRUMENTATION
  // Perf instrumentation endpoint — separate port from health_monitor's :9090
  PrometheusHttpServer perf_server(9091, perf_metrics_text);
  perf_server.start();
#endif

  auto exec = std::make_shared<rclcpp::executors::MultiThreadedExecutor>();

  // 零拷贝热路径（2026-08-24 落地）：三节点共享同一份开启 intra-process 的
  // NodeOptions，同进程 pub/sub 走 shared_ptr 直通，不走 DDS 序列化。
  // 热路径发布端用 unique_ptr（所有权移交=零拷贝）；传感器→fusion 仍跨进程
  // DDS（故障隔离，故意保留）。MoveToPose action 通道无 intra 零拷贝（低频，可接受）。
  auto opts = rclcpp::NodeOptions().use_intra_process_comms(true);
  auto fusion   = std::make_shared<FusionNode>(opts);
  auto decision = std::make_shared<DecisionNode>(opts);
  auto motor    = std::make_shared<MotorCtrlNode>(opts);

  exec->add_node(fusion->get_node_base_interface());
  exec->add_node(decision->get_node_base_interface());
  exec->add_node(motor->get_node_base_interface());

  fusion->configure();
  fusion->activate();
  decision->configure();
  decision->activate();
  motor->configure();
  motor->activate();

  exec->spin();
  rclcpp::shutdown();

#ifdef AMR_PERF_INSTRUMENTATION
  perf_server.stop();
#endif

  // Drain remaining log events before exit
  amr::observability::Logging::shutdown();
  return 0;
}
