// D1 验收实验产物（2026-08-26，19.5min 接入实证）转正为官方扩展范本：
// 见 docs/design/20260826-d1-extension-acceptance.md 与 ultrasonic_sensor.hpp 注册路径示例。
#include "ros2_robot_middleware/infrastructure/ultrasonic_node.hpp"

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<UltrasonicNode>();
    node->configure();
    node->activate();
    rclcpp::spin(node->get_node_base_interface());
    rclcpp::shutdown();
    return 0;
}
