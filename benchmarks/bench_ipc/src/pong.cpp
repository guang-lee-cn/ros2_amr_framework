// 基准一(跨进程)回声端：收到 ping(seq>0) 原样立即回发 pong。
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/u_int8_multi_array.hpp>

using namespace std::chrono_literals;

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("bench_pong");
  std::string prefix = node->declare_parameter<std::string>("prefix", "bench");

  const std::string rel =
      node->declare_parameter<std::string>("reliability", "reliable");
  auto qos = rclcpp::QoS(10);
  if (rel == "best_effort") qos.best_effort();
  else qos.reliable();
  auto pub = node->create_publisher<std_msgs::msg::UInt8MultiArray>(prefix + "/pong", qos);
  auto sub = node->create_subscription<std_msgs::msg::UInt8MultiArray>(
    prefix + "/ping", qos,
    [&](const std_msgs::msg::UInt8MultiArray::SharedPtr m) {
      if (m->data.size() < 12) return;
      uint32_t seq = m->data[0] | (m->data[1] << 8) | (m->data[2] << 16) | (uint32_t(m->data[3]) << 24);
      if (seq == 0) return;  // 探测帧不回
      pub->publish(*m);
    });
  rclcpp::spin(node);
  return 0;
}
