// Production compute container — fusion + decision + motor_ctrl in one process.
// Zero-copy communication via shared_ptr between nodes (no DDS serialization).
// Sensor drivers (lidar/imu/camera) remain independent for fault isolation.
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
#include "ros2_robot_middleware/observability/logging.hpp"

#include <memory>
#include <rclcpp/rclcpp.hpp>

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);

  // Observability: start background log worker
  amr::observability::Logging::init();

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

  // Drain remaining log events before exit
  amr::observability::Logging::shutdown();
  return 0;
}
