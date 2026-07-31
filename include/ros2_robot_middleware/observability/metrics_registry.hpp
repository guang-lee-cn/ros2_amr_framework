#pragma once
/// @file   metrics_registry.hpp
/// @brief  Cross-process atomic metrics via POSIX shared memory.
///
/// All processes map the same shm segment → one MetricsRegistry shared
/// across lidar/imu/camera/compute/health_monitor processes.
///
/// Hot path: atomic loads/stores on mmap'd memory — no syscall, no IPC overhead.

#include <atomic>
#include <cstdint>

namespace amr::observability {

// ── Histogram (same as before, but must be POD for shm) ──────────────
struct alignas(64) Histogram {
    static constexpr int kBucketCount = 64;
    static constexpr int64_t kBaseUs = 2;
    static constexpr int64_t kMaxUs = 1LL << 34;

    std::atomic<int64_t> buckets[kBucketCount]{};
    std::atomic<int64_t> total_count{0};
    std::atomic<int64_t> total_sum_us{0};

    void record(int64_t latency_us) noexcept;
};

// ── Shared memory registry ───────────────────────────────────────────
class MetricsRegistry {
public:
    // ── Sensor Rates (Gauge, deci-Hz) ────────────────────────────────
    std::atomic<int32_t> lidar_rate_ds{0};
    std::atomic<int32_t> imu_rate_ds{0};
    std::atomic<int32_t> camera_rate_ds{0};

    // ── Latency Histograms (μs) ──────────────────────────────────────
    Histogram fusion_latency;
    Histogram decision_latency;
    Histogram motor_latency;
    Histogram e2e_latency;

    // ── State Gauges ─────────────────────────────────────────────────
    std::atomic<int32_t> degradation_level{0};
    std::atomic<int64_t> degradation_events{0};
    std::atomic<int64_t> recovery_events{0};
    std::atomic<int32_t> object_count{0};
    std::atomic<int64_t> fusion_cycle_count{0};

    // ── End-to-end timestamp ─────────────────────────────────────────
    std::atomic<int64_t> last_sensor_timestamp_ns{0};

    MetricsRegistry() = default;
};

/// Cross-process singleton — definition in metrics_registry.cpp.
/// Shared via mmap'd /dev/shm so all processes see the same counters.
MetricsRegistry &shared_metrics();

} // namespace amr::observability
