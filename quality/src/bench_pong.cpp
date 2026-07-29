/// @file bench_pong.cpp — DDS latency benchmark: echo subscriber.
/// Usage: bench_pong [--ros-args -p qos:=reliable]
#include <chrono>
#include <cstring>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/byte_multi_array.hpp"

class PongNode : public rclcpp::Node {
public:
  PongNode() : Node("bench_pong") {
    std::string qos_str = declare_parameter("qos", std::string("reliable"));

    auto qos = (qos_str == "best_effort")
      ? rclcpp::QoS(1000).best_effort() : rclcpp::QoS(1000).reliable();

    pub_ = create_publisher<std_msgs::msg::ByteMultiArray>("/bench/pong", qos);
    sub_ = create_subscription<std_msgs::msg::ByteMultiArray>(
      "/bench/ping", qos,
      [this](std_msgs::msg::ByteMultiArray::SharedPtr msg) {
        // Echo back without modification (timestamp preserved by ping)
        pub_->publish(*msg);
      });
  }

private:
  rclcpp::Publisher<std_msgs::msg::ByteMultiArray>::SharedPtr pub_;
  rclcpp::Subscription<std_msgs::msg::ByteMultiArray>::SharedPtr sub_;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<PongNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
