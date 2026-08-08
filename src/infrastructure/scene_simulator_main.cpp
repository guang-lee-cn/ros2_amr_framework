#include "ros2_robot_middleware/infrastructure/scene_simulator_node.hpp"

#include <memory>
#include <rclcpp/rclcpp.hpp>

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SceneSimulatorNode>());
  rclcpp::shutdown();
  return 0;
}
