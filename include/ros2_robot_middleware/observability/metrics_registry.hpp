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
#include <cstring>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace amr::observability {

// ── Histogram (same as before, but must be POD for shm) ──────────────
struct alignas(64) Histogram {
    static constexpr int kBucketCount = 64;
    static constexpr int64_t kBaseUs = 2;
    static constexpr int64_t kMaxUs = 1LL << 34;

    std::atomic<int64_t> buckets[kBucketCount]{};
    std::atomic<int64_t> total_count{0};
    std::atomic<int64_t> total_sum_us{0};

    void record(int64_t latency_us) noexcept {
        total_count.fetch_add(1, std::memory_order_relaxed);
        total_sum_us.fetch_add(latency_us, std::memory_order_relaxed);
        int idx = 0;
        int64_t bound = kBaseUs;
        while (idx < kBucketCount - 1 && latency_us >= bound) {
            ++idx;
            bound *= kBaseUs;
        }
        buckets[idx].fetch_add(1, std::memory_order_relaxed);
    }
};

// ── Shared memory registry ───────────────────────────────────────────
class MetricsRegistry {
public:
    static MetricsRegistry &instance() {
        static MetricsRegistry reg;
        return reg;
    }

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

    MetricsRegistry() = default; // public — constructed by shared_metrics()
};


// ── Shared memory allocator for cross-process singleton ──────────────
// Replaces process-local static with mmap'd shared memory so that
// lidar/imu/camera/compute/health_monitor all share the same counters.

constexpr const char *kShmName = "/amr_metrics_registry";

inline MetricsRegistry &shared_metrics() {
    static MetricsRegistry *ptr = []() -> MetricsRegistry * {
        const size_t sz = sizeof(MetricsRegistry);
        int fd = shm_open(kShmName, O_CREAT | O_RDWR, 0644);
        if (fd < 0) {
            // Fallback: process-local if shm fails (e.g. no /dev/shm)
            static MetricsRegistry local;
            return &local;
        }
        if (ftruncate(fd, static_cast<off_t>(sz)) < 0) {
            close(fd);
            static MetricsRegistry local;
            return &local;
        }
        void *addr = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
        if (addr == MAP_FAILED) {
            static MetricsRegistry local;
            return &local;
        }
        return reinterpret_cast<MetricsRegistry *>(addr);
    }();
    return *ptr;
}

} // namespace amr::observability
