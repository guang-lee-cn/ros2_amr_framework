// 基准一(跨进程)：ping-pong 往返时延。payload 头 12 字节: seq(u32) + t_send_ns(u64)。
// 串行发-收：每收到 pong 才发下一个 ping，测的是纯链路时延，不含排队。
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/u_int8_multi_array.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

static void encode_header(std_msgs::msg::UInt8MultiArray & m, uint32_t seq, uint64_t t_ns)
{
  m.data[0] = seq & 0xFF; m.data[1] = (seq >> 8) & 0xFF;
  m.data[2] = (seq >> 16) & 0xFF; m.data[3] = (seq >> 24) & 0xFF;
  for (int i = 0; i < 8; ++i) m.data[4 + i] = (t_ns >> (8 * i)) & 0xFF;
}
static uint32_t decode_seq(const std_msgs::msg::UInt8MultiArray & m)
{
  return m.data[0] | (m.data[1] << 8) | (m.data[2] << 16) | (uint32_t(m.data[3]) << 24);
}
static uint64_t decode_t_ns(const std_msgs::msg::UInt8MultiArray & m)
{
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v |= uint64_t(m.data[4 + i]) << (8 * i);
  return v;
}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("bench_ping");

  int size = node->declare_parameter<int>("size", 1024);
  int samples = node->declare_parameter<int>("samples", 3000);
  int warmup = node->declare_parameter<int>("warmup", 300);
  bool stream = node->declare_parameter<bool>("stream", false);
  // U3 QoS 矩阵: reliable(默认) | best_effort（两侧需一致——best_effort 发布者与
  // reliable 订阅者不兼容，反向可以）
  const std::string rel =
      node->declare_parameter<std::string>("reliability", "reliable");
  std::string prefix = node->declare_parameter<std::string>("prefix", "bench");

  uint32_t seq = 0;
  bool waiting = false;
  std::vector<double> rtts_us;
  rtts_us.reserve(samples);

  auto qos = rclcpp::QoS(10);
  if (rel == "best_effort") qos.best_effort();
  else qos.reliable();
  auto ping_pub = node->create_publisher<std_msgs::msg::UInt8MultiArray>(prefix + "/ping", qos);
  auto pong_sub = node->create_subscription<std_msgs::msg::UInt8MultiArray>(
    prefix + "/pong", qos,
    [&](const std_msgs::msg::UInt8MultiArray::SharedPtr m) {
      if (decode_seq(*m) != seq) return;
      auto now_ns = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()).count());
      double rtt_us = double(now_ns - decode_t_ns(*m)) / 1000.0;
      if (seq >= uint32_t(warmup)) {
        rtts_us.push_back(rtt_us);
        if (stream) { printf("S %u %.1f\n", seq, rtt_us); fflush(stdout); }
      }
      waiting = false;
      if (int(rtts_us.size()) >= samples) rclcpp::shutdown();
    });

  auto msg = std_msgs::msg::UInt8MultiArray();
  msg.data.resize(std::max(size, 16));
  std::fill(msg.data.begin(), msg.data.end(), 0xA5);

  // 等 pong 上线：先探测发一条；discovery 未完成时帧会丢，100ms 超时重发同 seq
  uint64_t last_send_ns = 0;
  auto timer = node->create_wall_timer(2ms, [&]() {
    if (!rclcpp::ok()) return;
    auto now_ns = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
      Clock::now().time_since_epoch()).count());
    if (waiting) {
      if (now_ns - last_send_ns < 100000000ULL) return;
      encode_header(msg, seq, now_ns);  // 同 seq 重发，取新时间戳
      ping_pub->publish(msg);
      last_send_ns = now_ns;
      return;
    }
    if (!ping_pub->get_subscription_count()) {
      encode_header(msg, 0, 0);  // 探测帧，pong 不回 seq=0
      ping_pub->publish(msg);
      return;
    }
    waiting = true;
    encode_header(msg, ++seq, now_ns);
    ping_pub->publish(msg);
    last_send_ns = now_ns;
  });
  encode_header(msg, 0, 0);
  ping_pub->publish(msg);

  rclcpp::spin(node);

  if (rtts_us.empty()) { fprintf(stderr, "no samples\n"); return 1; }
  std::sort(rtts_us.begin(), rtts_us.end());
  auto pct = [&](double p) {
    size_t i = std::min(rtts_us.size() - 1, size_t(p * rtts_us.size()));
    return rtts_us[i];
  };
  double sum = 0; for (double v : rtts_us) sum += v;
  const char * rmw = std::getenv("RMW_IMPLEMENTATION");
  const char * fdd = std::getenv("FASTDDS_BUILTIN_TRANSPORTS");
  printf("{\"bench\":\"ipc_inter\",\"reliability\":\"%s\",\"rmw\":\"%s\",\"fastdds_transport\":\"%s\","
         "\"payload_bytes\":%d,\"samples\":%zu,"
         "\"rtt_us\":{\"min\":%.1f,\"p50\":%.1f,\"p90\":%.1f,\"p99\":%.1f,\"p999\":%.1f,\"max\":%.1f,\"mean\":%.1f},"
         "\"one_way_us\":{\"p50\":%.1f,\"p99\":%.1f}}\n",
         rel.c_str(), rmw ? rmw : "rmw_fastrtps_cpp(default)", fdd ? fdd : "default",
         size, rtts_us.size(),
         rtts_us.front(), pct(0.50), pct(0.90), pct(0.99), pct(0.999), rtts_us.back(), sum / rtts_us.size(),
         pct(0.50) / 2.0, pct(0.99) / 2.0);
  return 0;
}
