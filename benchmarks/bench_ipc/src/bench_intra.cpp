// 基准一(进程内零拷贝)：ping/pong 两个节点同进程，use_intra_process_comms(true)，
// 全链路 unique_ptr 发布/订阅——ROS2 当前可用的零拷贝路径（无需 iceoryx）。
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/u_int8_multi_array.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
using Msg = std_msgs::msg::UInt8MultiArray;

static void encode_header(Msg & m, uint32_t seq, uint64_t t_ns)
{
  m.data[0] = seq & 0xFF; m.data[1] = (seq >> 8) & 0xFF;
  m.data[2] = (seq >> 16) & 0xFF; m.data[3] = (seq >> 24) & 0xFF;
  for (int i = 0; i < 8; ++i) m.data[4 + i] = (t_ns >> (8 * i)) & 0xFF;
}
static uint32_t decode_seq(const Msg & m)
{
  return m.data[0] | (m.data[1] << 8) | (m.data[2] << 16) | (uint32_t(m.data[3]) << 24);
}
static uint64_t decode_t_ns(const Msg & m)
{
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v |= uint64_t(m.data[4 + i]) << (8 * i);
  return v;
}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::NodeOptions opts;
  opts.use_intra_process_comms(true);

  auto ping = std::make_shared<rclcpp::Node>("bench_ping_intra", opts);
  int size = ping->declare_parameter<int>("size", 1024);
  int samples = ping->declare_parameter<int>("samples", 3000);
  int warmup = ping->declare_parameter<int>("warmup", 300);
  auto pong = std::make_shared<rclcpp::Node>("bench_pong_intra", opts);

  uint32_t seq = 0;
  bool waiting = false;
  std::vector<double> rtts_us;
  rtts_us.reserve(samples);

  auto ping_pub = ping->create_publisher<Msg>("bench/intra_ping", rclcpp::QoS(10));
  auto pong_pub = pong->create_publisher<Msg>("bench/intra_pong", rclcpp::QoS(10));
  auto pong_sub = ping->create_subscription<Msg>(
    "bench/intra_pong", rclcpp::QoS(10),
    [&](const Msg::ConstSharedPtr & m) {
      if (decode_seq(*m) != seq) return;
      auto now_ns = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()).count());
      double rtt_us = double(now_ns - decode_t_ns(*m)) / 1000.0;
      if (seq >= uint32_t(warmup)) rtts_us.push_back(rtt_us);
      waiting = false;
      if (int(rtts_us.size()) >= samples) rclcpp::shutdown();
    });
  auto ping_sub = pong->create_subscription<Msg>(
    "bench/intra_ping", rclcpp::QoS(10),
    [&](Msg::UniquePtr m) {
      if (decode_seq(*m) == 0) return;
      pong_pub->publish(std::move(m));  // 零拷贝转发
    });

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(ping);
  exec.add_node(pong);

  auto msg = std::make_unique<Msg>();
  msg->data.resize(std::max(size, 16));
  std::fill(msg->data.begin(), msg->data.end(), 0xA5);

  auto timer = ping->create_wall_timer(2ms, [&]() {
    if (waiting || !rclcpp::ok()) return;
    waiting = true;
    auto t_ns = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
      Clock::now().time_since_epoch()).count());
    encode_header(*msg, ++seq, t_ns);
    ping_pub->publish(std::move(msg));   // unique_ptr 发布 = 零拷贝入口
    msg = std::make_unique<Msg>();      // 重新构造下一帧
    msg->data.resize(std::max(size, 16));
    std::fill(msg->data.begin(), msg->data.end(), 0xA5);
  });
  exec.spin();

  if (rtts_us.empty()) { fprintf(stderr, "no samples\n"); return 1; }
  std::sort(rtts_us.begin(), rtts_us.end());
  auto pct = [&](double p) {
    size_t i = std::min(rtts_us.size() - 1, size_t(p * rtts_us.size()));
    return rtts_us[i];
  };
  double sum = 0; for (double v : rtts_us) sum += v;
  printf("{\"bench\":\"ipc_intra\",\"payload_bytes\":%d,\"samples\":%zu,"
         "\"rtt_us\":{\"min\":%.1f,\"p50\":%.1f,\"p90\":%.1f,\"p99\":%.1f,\"p999\":%.1f,\"max\":%.1f,\"mean\":%.1f},"
         "\"one_way_us\":{\"p50\":%.1f,\"p99\":%.1f}}\n",
         size, rtts_us.size(),
         rtts_us.front(), pct(0.50), pct(0.90), pct(0.99), pct(0.999), rtts_us.back(), sum / rtts_us.size(),
         pct(0.50) / 2.0, pct(0.99) / 2.0);
  return 0;
}
