/// @file   metrics_registry.cpp
/// @brief  Shared-memory metrics singleton implementation.

#include "ros2_robot_middleware/observability/metrics_registry.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace amr::observability {

void Histogram::record(int64_t latency_us) noexcept {
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

MetricsRegistry &shared_metrics() {
    static MetricsRegistry *ptr = []() -> MetricsRegistry * {
        constexpr const char *kShmName = "/amr_metrics_registry";
        const size_t sz = sizeof(MetricsRegistry);
        int fd = shm_open(kShmName, O_CREAT | O_RDWR, 0644);
        if (fd < 0) {
            static MetricsRegistry local;  // fallback: no /dev/shm
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
