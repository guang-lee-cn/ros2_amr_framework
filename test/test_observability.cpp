/// @file test_observability.cpp — ring_buffer + metrics_registry unit tests (no ROS2)
#include "ros2_robot_middleware/observability/metrics_registry.hpp"
#include "ros2_robot_middleware/observability/ring_buffer.hpp"

#include <gtest/gtest.h>

#include <thread>

// ── RingBuffer ───────────────────────────────────────────────────────

TEST(RingBufferTest, PushPop_SingleElement) {
  amr::observability::RingBuffer<int, 8> rb;
  EXPECT_TRUE(rb.empty());

  ASSERT_TRUE(rb.try_push(42));
  EXPECT_FALSE(rb.empty());
  EXPECT_EQ(rb.size(), 1u);

  auto val = rb.pop();
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, 42);
  EXPECT_TRUE(rb.empty());
}

TEST(RingBufferTest, Pop_EmptyReturnsNullopt) {
  amr::observability::RingBuffer<int, 8> rb;
  EXPECT_EQ(rb.pop(), std::nullopt);
}

TEST(RingBufferTest, Push_FullReturnsFalse) {
  // Capacity N stores at most N-1 (head==tail distinguishes empty/full)
  amr::observability::RingBuffer<int, 4> rb;
  for (int i = 0; i < 3; ++i) ASSERT_TRUE(rb.try_push(i));
  EXPECT_FALSE(rb.try_push(99));  // full → rejected, 4th slot reserved
  EXPECT_EQ(rb.size(), 3u);
}

TEST(RingBufferTest, WrapAround_NoDataLoss) {
  amr::observability::RingBuffer<int, 8> rb;
  // Fill → drain → refill to exercise head/tail wrap
  for (int i = 0; i < 7; ++i) ASSERT_TRUE(rb.try_push(i));
  for (int i = 0; i < 7; ++i) { auto v = rb.pop(); ASSERT_TRUE(v); EXPECT_EQ(*v, i); }
  EXPECT_TRUE(rb.empty());

  for (int i = 10; i < 17; ++i) ASSERT_TRUE(rb.try_push(i));
  for (int i = 10; i < 17; ++i) { auto v = rb.pop(); ASSERT_TRUE(v); EXPECT_EQ(*v, i); }
}

TEST(RingBufferTest, ThreadSafety_ProducerConsumer) {
  amr::observability::RingBuffer<int, 256> rb;
  std::atomic<bool> done{false};
  std::atomic<int> sum{0};

  // Producer thread
  std::thread producer([&]() {
    for (int i = 1; i <= 1000; ++i) {
      while (!rb.try_push(i)) { /* spin — buffer full, consumer is slow */ }
    }
    done.store(true, std::memory_order_release);
  });

  // Consumer thread
  std::thread consumer([&]() {
    int last = 0;
    while (!done.load(std::memory_order_acquire) || !rb.empty()) {
      auto v = rb.pop();
      if (v) {
        EXPECT_GT(*v, last);  // monotonic
        last = *v;
        sum.fetch_add(*v, std::memory_order_relaxed);
      }
    }
  });

  producer.join();
  consumer.join();
  EXPECT_EQ(sum.load(), 500500);  // sum(1..1000)
}

// ── MetricsRegistry ──────────────────────────────────────────────────

TEST(MetricsRegistryTest, Histogram_RecordsLatency) {
  auto &m = amr::observability::shared_metrics();
  auto before = m.fusion_latency.total_count.load(std::memory_order_relaxed);

  m.fusion_latency.record(150);  // 150μs
  m.fusion_latency.record(320);

  EXPECT_EQ(m.fusion_latency.total_count.load(std::memory_order_relaxed), before + 2);
  EXPECT_GT(m.fusion_latency.total_sum_us.load(std::memory_order_relaxed), 0);
}

TEST(MetricsRegistryTest, Counter_Increments) {
  auto &m = amr::observability::shared_metrics();
  auto before = m.fusion_cycle_count.load(std::memory_order_relaxed);

  m.fusion_cycle_count.fetch_add(1, std::memory_order_relaxed);
  m.fusion_cycle_count.fetch_add(1, std::memory_order_relaxed);

  EXPECT_EQ(m.fusion_cycle_count.load(std::memory_order_relaxed), before + 2);
}

TEST(MetricsRegistryTest, Gauge_StoresValue) {
  auto &m = amr::observability::shared_metrics();
  m.degradation_level.store(3, std::memory_order_relaxed);
  EXPECT_EQ(m.degradation_level.load(std::memory_order_relaxed), 3);
  m.degradation_level.store(0, std::memory_order_relaxed);
}
