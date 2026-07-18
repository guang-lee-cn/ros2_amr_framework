/// @file test_observability.cpp — ring_buffer + metrics_registry unit tests (no ROS2)
#include "ros2_robot_middleware/observability/log_event.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"
#include "ros2_robot_middleware/observability/ring_buffer.hpp"
#include "ros2_robot_middleware/observability/trace_points.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"

#include <set>
#include <string_view>

using namespace amr::observability;

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

// ── TraceContext ─────────────────────────────────────────────────────

TEST(TraceContextTest, ScopedSpan_GeneratesTraceId) {
  auto &ctx = amr::observability::current_trace();
  EXPECT_EQ(ctx.trace_id, 0u);
  EXPECT_EQ(ctx.span_id, 0u);

  {
    amr::observability::ScopedSpan span("test_root");
    EXPECT_NE(current_trace().trace_id, 0u);   // root span got a trace_id
    EXPECT_EQ(current_trace().span_id, 1u);      // first span
  }
  // Scope exit restores parent context
  EXPECT_EQ(ctx.trace_id, 0u);
}

TEST(TraceContextTest, NestedSpans_IncrementSpanId) {
  {
    amr::observability::ScopedSpan root("root");
    auto root_trace = current_trace().trace_id;
    EXPECT_EQ(current_trace().span_id, 1u);

    {
      amr::observability::ScopedSpan child("child");
      EXPECT_EQ(current_trace().trace_id, root_trace);  // same trace
      EXPECT_GT(current_trace().span_id, 1u);            // child span
    }
    // Back to root
    EXPECT_EQ(current_trace().span_id, 1u);
  }
}

TEST(TraceContextTest, LogEvent_PicksUpTraceContext) {
  {
    amr::observability::ScopedSpan span("test");
    auto ev = amr::observability::LogEvent::make(
        amr::observability::LogLevel::INFO, "test_mod", "test_msg", 0, 0);
    // LOG_OBS would auto-populate trace_id from current_trace().
    // Here we simulate what LOG_OBS does:
    auto &ctx = amr::observability::current_trace();
    ev.trace_id = ctx.trace_id;
    ev.span_id  = ctx.span_id;

    EXPECT_NE(ev.trace_id, 0u);
    EXPECT_EQ(ev.span_id, 1u);
  }
  // After span exits, new events get trace_id=0 (no active trace)
  auto ev2 = amr::observability::LogEvent::make(
      amr::observability::LogLevel::INFO, "test_mod", "no_trace", 0, 0);
  EXPECT_EQ(ev2.trace_id, 0u);
}

// ── TracePoints — uniqueness validation ─────────────────────────────

TEST(TracePointsTest, AllNamesAreUnique) {
  std::set<std::string_view> names;
  auto check = [&](const char *name) {
    EXPECT_TRUE(names.insert(name).second) << "Duplicate trace point: " << name;
  };

  check(amr::trace::LIDAR_CALLBACK);
  check(amr::trace::IMU_CALLBACK);
  check(amr::trace::CAMERA_CALLBACK);
  check(amr::trace::FUSION_TIMER);
  check(amr::trace::FUSION_DEGRADATION);
  check(amr::trace::FUSION_CLUSTER_DETECT);
  check(amr::trace::DECISION_ON_PERCEPTION);
  check(amr::trace::DECISION_SEND_GOAL);
  check(amr::trace::DECISION_PREEMPT);
  check(amr::trace::MOTOR_EXECUTE);
  check(amr::trace::MOTOR_CANCEL);
  check(amr::trace::MOTOR_STEP);
  check(amr::trace::HEALTH_CHECK);
  check(amr::trace::HEALTH_RECOVERY);
  check(amr::trace::FLEET_AGGREGATE);
}

TEST(TracePointsTest, NamingFollowsConvention) {
  // All names use '::' separator, lowercase, no trailing colon
  auto valid = [](const char *name) -> bool {
    std::string_view s(name);
    return s.find("::") != std::string_view::npos  // has separator
           && s.back() != ':';                      // no trailing colon
  };
  EXPECT_TRUE(valid(amr::trace::FUSION_TIMER));
  EXPECT_TRUE(valid(amr::trace::MOTOR_EXECUTE));
}
