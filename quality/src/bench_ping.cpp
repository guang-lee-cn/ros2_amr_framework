/// @file bench_ping.cpp — DDS latency benchmark: publisher + RTT measurement.
/// Usage: bench_ping [--ros-args -p rate:=100 -p count:=1000 -p size:=1024 -p qos:=reliable]
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include "generated/perf_instrumentation.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/byte_multi_array.hpp"

class PingNode : public rclcpp::Node {
public:
  PingNode() : Node("bench_ping") {
    rate_ = declare_parameter("rate", 100);
    count_ = declare_parameter("count", 1000);
    int size = declare_parameter("size", 1024);
    payload_.assign(static_cast<size_t>(size), 0xAA);

    std::string qos_str = declare_parameter("qos", std::string("reliable"));
    auto qos = (qos_str == "best_effort")
      ? rclcpp::QoS(static_cast<size_t>(count_)).best_effort()
      : rclcpp::QoS(static_cast<size_t>(count_)).reliable();

    pub_ = create_publisher<std_msgs::msg::ByteMultiArray>("/bench/ping", qos);
    sub_ = create_subscription<std_msgs::msg::ByteMultiArray>(
      "/bench/pong", qos,
      [this](std_msgs::msg::ByteMultiArray::SharedPtr msg) {
        AMR_PERF_PHASE("sub:ping_recv");
        if (msg->data.size() < 16) return;
        uint64_t send_ts = 0;
        std::memcpy(&send_ts, msg->data.data() + 8, 8);
        uint64_t rtt_ns = now().nanoseconds() - send_ts;
        latencies_.push_back(rtt_ns);
        AMR_PERF_RECORD("rtt:ping_pong", rtt_ns / 1000);
        received_++;
        // results printed after spin loop completes
      });
  }

  void run() {
    // Wait for DDS discovery (publisher ↔ subscriber matching)
    RCLCPP_INFO(get_logger(), "Waiting for DDS discovery...");
    auto discovery_t0 = std::chrono::steady_clock::now();
    while (pub_->get_subscription_count() == 0) {
      if (std::chrono::steady_clock::now() - discovery_t0 > std::chrono::seconds(10)) {
        RCLCPP_ERROR(get_logger(), "DDS discovery timeout");
        return;
      }
      rclcpp::spin_some(get_node_base_interface());
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    RCLCPP_INFO(get_logger(), "DDS discovery complete, sending %ld msgs", count_);

    // Send and receive concurrently:
    // - Sender thread: publish at target rate
    // - Main thread: spin to process incoming pong responses immediately
    std::atomic<bool> sending_done{false};
    std::thread sender([this, &sending_done]() {
      auto period = std::chrono::nanoseconds(1'000'000'000 / rate_);
      for (int64_t i = 0; i < count_; ++i) {
        auto msg = std_msgs::msg::ByteMultiArray{};
        msg.data = payload_;
        uint64_t seq = static_cast<uint64_t>(i);
        uint64_t ts_ns = now().nanoseconds();
        std::memcpy(msg.data.data(), &seq, 8);
        std::memcpy(msg.data.data() + 8, &ts_ns, 8);
        { AMR_PERF_PHASE("pub:ping");
          pub_->publish(msg); }
        std::this_thread::sleep_for(period);
      }
      sending_done.store(true, std::memory_order_release);
    });

    // Main thread: spin to receive pong responses as they arrive
    auto spin_t0 = std::chrono::steady_clock::now();
    while (received_ < count_) {
      rclcpp::spin_some(get_node_base_interface());
      if (std::chrono::steady_clock::now() - spin_t0 > std::chrono::seconds(30)) {
        RCLCPP_WARN(get_logger(), "Timeout (%ld/%ld)", received_, count_);
        break;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    sender.join();
  }

  void print_results() const {
    if (latencies_.empty()) { std::cout << "BENCH_RESULT: no_responses\n"; return; }
    auto copy = latencies_;
    std::sort(copy.begin(), copy.end());
    size_t n = copy.size();
    uint64_t avg = 0;
    for (auto l : copy) avg += l;
    avg /= n;
    std::cout << "BENCH_RESULT:"
              << " received=" << received_
              << " avg_us=" << (avg / 1000)
              << " p50_us=" << (copy[n / 2] / 1000)
              << " p99_us=" << (copy[n * 99 / 100] / 1000)
              << " max_us=" << (copy.back() / 1000) << "\n";
  }

private:
  rclcpp::Publisher<std_msgs::msg::ByteMultiArray>::SharedPtr pub_;
  rclcpp::Subscription<std_msgs::msg::ByteMultiArray>::SharedPtr sub_;
  int64_t received_ = 0, rate_ = 100, count_ = 1000;
  std::vector<uint8_t> payload_;
  std::vector<uint64_t> latencies_;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<PingNode>();
  node->run();
  node->print_results();
  rclcpp::shutdown();
  return 0;
}
