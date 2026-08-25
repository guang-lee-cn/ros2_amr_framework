#include "ros2_robot_middleware/infrastructure/supervisor_node.hpp"

#include <rclcpp/rclcpp.hpp>

/// amr_supervisor — 进程级监管入口。
/// supervisor 自身死了没人拉（真机 systemd unit 兜底，见 ADR「后果与边界」），
/// 但它退出时绝不泄漏子进程：spin 返回后无条件 teardown（2026-08-16 教训）。
int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<amr::infrastructure::SupervisorNode>();
  node->configure();
  node->activate();
  rclcpp::spin(node->get_node_base_interface());
  node->teardown_children();  // Ctrl-C/launch 停止：无条件清场
  rclcpp::shutdown();
  return 0;
}
