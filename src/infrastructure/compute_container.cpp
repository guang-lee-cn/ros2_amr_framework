// Production compute container — fusion + decision + motor_ctrl in one process.
// Zero-copy communication via shared_ptr between nodes (no DDS serialization).
// Sensor drivers (lidar/imu/camera) remain independent for fault isolation.
//
// TODO(zero-copy, 2026-08-15): the claim on line 2 is currently FALSE.
// The three nodes talk via plain create_publisher/create_subscription topics
// and no NodeOptions enables use_intra_process_comms, so messages still take
// the full DDS serialization path. Same-process WITHOUT intra-process comms
// gives up fault isolation (fusion crash kills decision/motor) for zero
// performance gain — worst of both layouts.
//
// Fix (preferred): construct FusionNode/DecisionNode/MotorCtrlNode with a
// shared rclcpp::NodeOptions().use_intra_process_comms(true) so the line-2
// claim becomes true. Requirements:
//   1. Verify on Jazzy that LifecycleNode pub/sub actually takes the
//      intra-process path — lifecycle + intra-process had bugs in some
//      distros; confirm with a latency/throughput measurement, not just
//      "tests pass".
//   2. Zero-copy path needs publishers to pass std::unique_ptr and
//      subscribers to take std::shared_ptr; by-value publishes fall back to
//      a copy (still no DDS serialization, but not zero-copy).
// Fallback: if IPC cannot be enabled cleanly, split fusion/decision/motor
// back into separate processes (restore isolation) and delete the false
// zero-copy claim.
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

  auto fusion   = std::make_shared<FusionNode>();
  auto decision = std::make_shared<DecisionNode>();
  auto motor    = std::make_shared<MotorCtrlNode>();

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
