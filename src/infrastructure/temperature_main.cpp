// D1-RUN2: 第三方开发者扩展验收实验——温度传感器独立进程入口。
// 进 CMake INDEPENDENT_NODES 列表（CLAUDE.md「节点形态」）。
#include "ros2_robot_middleware/infrastructure/temperature_node.hpp"

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TemperatureNode>();
    node->configure();
    node->activate();
    rclcpp::spin(node->get_node_base_interface());
    rclcpp::shutdown();
    return 0;
}
