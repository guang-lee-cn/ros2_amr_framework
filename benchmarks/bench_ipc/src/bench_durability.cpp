// 基准四：Durability=TransientLocal 晚加入补帧实测。
// pub 模式: 以指定 durability 发 K 帧后挂住; sub 模式: 晚启动, 收 3 秒, 报补帧结果。
// 预期: transient_local(depth=K) → 晚加入者收到最后 K 帧; volatile → 收到 0 帧。
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/u_int8_multi_array.hpp>

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace std::chrono_literals;

static void encode_seq(std_msgs::msg::UInt8MultiArray & m, uint32_t seq)
{
  m.data.resize(16);
  m.data[0] = seq & 0xFF; m.data[1] = (seq >> 8) & 0xFF;
  m.data[2] = (seq >> 16) & 0xFF; m.data[3] = (seq >> 24) & 0xFF;
}
static uint32_t decode_seq(const std_msgs::msg::UInt8MultiArray & m)
{
  if (m.data.size() < 4) return 0;
  return m.data[0] | (m.data[1] << 8) | (m.data[2] << 16) | (uint32_t(m.data[3]) << 24);
}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("bench_durability");

  const std::string mode = node->declare_parameter<std::string>("mode", "pub");
  const std::string durability = node->declare_parameter<std::string>("durability", "transient_local");
  const int k = node->declare_parameter<int>("k", 5);

  rclcpp::QoS qos(k);
  qos.reliable();
  if (durability == "transient_local") qos.transient_local();
  else qos.durability_volatile();

  if (mode == "pub") {
    auto pub = node->create_publisher<std_msgs::msg::UInt8MultiArray>("bench/dur", qos);
    // 等 matcher 兜一圈（transient_local 的匹配语义不依赖订阅者在线）
    for (int i = 0; i < 100 && rclcpp::ok(); ++i) rclcpp::spin_some(node);
    auto msg = std_msgs::msg::UInt8MultiArray();
    for (int i = 1; i <= k; ++i) {
      encode_seq(msg, uint32_t(i));
      pub->publish(msg);
      rclcpp::sleep_for(10ms);
    }
    printf("{\"bench\":\"durability\",\"side\":\"pub\",\"durability\":\"%s\",\"k\":%d,"
           "\"published\":true}\n", durability.c_str(), k);
    fflush(stdout);
    rclcpp::spin(node);  // 挂住保持 writer 存活
    return 0;
  }

  // sub 模式：晚启动，统计补帧
  std::vector<uint32_t> got;
  auto sub = node->create_subscription<std_msgs::msg::UInt8MultiArray>(
    "bench/dur", qos, [&](const std_msgs::msg::UInt8MultiArray::SharedPtr m) {
      got.push_back(decode_seq(*m));
    });
  auto deadline = std::chrono::steady_clock::now() + 3s;
  while (rclcpp::ok() && std::chrono::steady_clock::now() < deadline) {
    rclcpp::spin_some(node);
    rclcpp::sleep_for(10ms);
  }
  uint32_t first = got.empty() ? 0 : got.front();
  uint32_t last = got.empty() ? 0 : got.back();
  printf("{\"bench\":\"durability\",\"side\":\"sub\",\"durability\":\"%s\",\"k_expected\":%d,"
         "\"late_join_received\":%zu,\"first_seq\":%u,\"last_seq\":%u}\n",
         durability.c_str(), k, got.size(), first, last);
  return 0;
}
