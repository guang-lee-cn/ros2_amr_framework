#include "ros2_robot_middleware/infrastructure/motor_ctrl_node.hpp"

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<MotorCtrlNode>();
    node->configure();
    node->activate();
    // 三审 P0-L：单线程 spin 会令 20Hz execute() 循环饿死全部回调——
    // on_scan 永不执行 → 碰撞保护旁路 + odom 恒无效。独立可执行形态必须
    // MultiThreadedExecutor（execute 的 callback group 隔离才真正生效）。
    rclcpp::executors::MultiThreadedExecutor exec;
    exec.add_node(node->get_node_base_interface());
    exec.spin();
    rclcpp::shutdown();
    return 0;
}
