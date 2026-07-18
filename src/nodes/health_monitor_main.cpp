#include "ros2_robot_middleware/infrastructure/health_monitor_node.hpp"

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<HealthMonitorNode>();
  node->configure();
  node->activate();
  rclcpp::spin(node->get_node_base_interface());
  rclcpp::shutdown();
  return 0;
}
