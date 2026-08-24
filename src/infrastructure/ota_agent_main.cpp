#include "ros2_robot_middleware/infrastructure/ota_agent_node.hpp"

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  // 参数驱动型节点：无生命周期流转，param set 即触发——挂机等待指令
  auto node = std::make_shared<amr::infrastructure::OtaAgentNode>();
  rclcpp::spin(node->get_node_base_interface());
  rclcpp::shutdown();
  return 0;
}
