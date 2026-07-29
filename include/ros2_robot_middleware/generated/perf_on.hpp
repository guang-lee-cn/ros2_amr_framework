// Generated — CI/test mode: real instrumentation timers.
// Self-contained: all class definitions inline, no external includes needed.
#ifndef ROS2_ROBOT_MIDDLEWARE_GENERATED_PERF_INSTRUMENTATION_HPP_
#define ROS2_ROBOT_MIDDLEWARE_GENERATED_PERF_INSTRUMENTATION_HPP_

#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace amr {
namespace observability {

struct PerfSnapshot {
  std::string name;
  int64_t count = 0;
  int64_t total_us = 0;
  int64_t p50_us = 0;
  int64_t p99_us = 0;
  double avg_us() const { return count > 0 ? static_cast<double>(total_us) / count : 0.0; }
};

class PerfRegistry {
public:
  static PerfRegistry &instance() {
    static PerfRegistry reg;
    return reg;
  }

  void record(const std::string &name, int64_t elapsed_us) {
    auto &slot = slots_[name];
    slot.count.fetch_add(1, std::memory_order_relaxed);
    slot.total_us.fetch_add(elapsed_us, std::memory_order_relaxed);
    int bucket = 0;
    int64_t v = elapsed_us;
    while (v > 1) { v >>= 1; ++bucket; }
    if (bucket >= 0 && bucket < kBuckets) {
      slot.buckets[bucket].fetch_add(1, std::memory_order_relaxed);
    }
  }

  void count(const std::string &name) {
    slots_[name].count.fetch_add(1, std::memory_order_relaxed);
  }

  std::vector<PerfSnapshot> snapshots() const {
    std::vector<PerfSnapshot> result;
    for (const auto &[name, slot] : slots_) {
      PerfSnapshot s;
      s.name = name;
      s.count = slot.count.load(std::memory_order_relaxed);
      s.total_us = slot.total_us.load(std::memory_order_relaxed);
      if (s.count > 0) {
        int64_t cumulative = 0;
        int64_t target_50 = s.count * 50 / 100;
        int64_t target_99 = s.count * 99 / 100;
        bool found_p50 = false;
        for (int b = 0; b < kBuckets; ++b) {
          cumulative += slot.buckets[b].load(std::memory_order_relaxed);
          if (!found_p50 && cumulative >= target_50) {
            s.p50_us = (static_cast<int64_t>(1) << b);
            found_p50 = true;
          }
          if (cumulative >= target_99) {
            s.p99_us = (static_cast<int64_t>(1) << b);
            break;
          }
        }
      }
      result.push_back(s);
    }
    return result;
  }

  void clear() { slots_.clear(); }

private:
  static constexpr int kBuckets = 64;
  struct Slot {
    std::atomic<int64_t> count{0};
    std::atomic<int64_t> total_us{0};
    std::atomic<int64_t> buckets[kBuckets]{};
  };
  std::unordered_map<std::string, Slot> slots_;
};

class ScopedPerfTimer {
public:
  explicit ScopedPerfTimer(std::string name)
    : name_(std::move(name)), start_(std::chrono::steady_clock::now()) {}

  ~ScopedPerfTimer() {
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
    PerfRegistry::instance().record(name_, elapsed);
  }

  ScopedPerfTimer(const ScopedPerfTimer &) = delete;
  ScopedPerfTimer &operator=(const ScopedPerfTimer &) = delete;

private:
  std::string name_;
  std::chrono::steady_clock::time_point start_;
};

}  // namespace observability
}  // namespace amr

#define AMR_PERF_CONCAT2(a, b) a##b
#define AMR_PERF_CONCAT(a, b)  AMR_PERF_CONCAT2(a, b)
#define AMR_PERF_PHASE(name) \
  ::amr::observability::ScopedPerfTimer AMR_PERF_CONCAT(_amr_perf_, __LINE__)(name)
#define AMR_PERF_POINT(name) \
  ::amr::observability::PerfRegistry::instance().count(name)
#define AMR_PERF_RECORD(name, elapsed_us) \
  ::amr::observability::PerfRegistry::instance().record(name, elapsed_us)

#endif
