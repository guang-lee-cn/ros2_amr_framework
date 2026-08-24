#pragma once
/// @file   isensor.hpp
/// @brief  Runtime-polymorphic sensor abstraction for dependency injection.
///
/// ISensor<DataType>  — abstract interface (one virtual call per read())
/// SensorBase<Derived, DataType> — CRTP helper for concrete implementations
///
/// Dependency injection pattern:
///   PerceptionService(ISensor<LidarScan>&, ISensor<ImuData>&, ISensor<CameraFrame>&)
///   FusionNode creates concrete sensors, injects them — no templates needed.
///   Tests inject mock sensors — no ROS2 dependency.
///
/// Overhead: one virtual call per sensor per tick (~5-10ns). At 100Hz IMU →
///           500ns/s. Negligible vs 10ms callback budget.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace amr::hal::sensor {

/// ── Data types (ROS2-free) ──────────────────────────────────────────

struct LidarScan {
    static constexpr int kMaxRanges = 2048;
    float ranges[kMaxRanges]        = {};
    size_t range_count              = 0;
    float angle_min                 = 0.0F;
    float angle_increment           = 0.0F;
    /// 测量时刻（纳秒，节点时钟域）。打戳规则见 docs/design/20260824-timestamp-policy-adr.md：
    /// 最早可得点打戳；0 = 未盖章（内部合成路径由 infra 在读取边界补）。
    int64_t stamp_ns                = 0;
};

struct ImuData {
    float linear_accel_x = 0.0F;
    float linear_accel_y = 0.0F;
    float angular_vel_z  = 0.0F;
    /// 测量时刻（纳秒，同上）
    int64_t stamp_ns     = 0;
};

struct CameraFrame {
    static constexpr int kMaxWidth   = 640;
    static constexpr int kMaxHeight  = 480;
    static constexpr size_t kMaxSize = kMaxWidth * kMaxHeight * 3;
    uint8_t *data                    = nullptr;
    size_t capacity                  = 0;
    size_t size                      = 0;
    uint16_t width                   = 0;
    uint16_t height                  = 0;
    /// 1D 水平扫描线深度（毫米），对应前方 FOV 的一组射线；0 = 无返回。
    /// 值小（≤ 数百元素）按值拷贝；RGB 大缓冲仍走上方指针视图。
    std::vector<uint16_t> depth;
#ifndef NDEBUG
    uint64_t generation = 0;
#endif
};

/// ── Abstract interface (runtime polymorphism) ────────────────────────

template <typename DataType> class ISensor {
public:
    virtual ~ISensor()               = default;
    virtual bool read(DataType &out) = 0;
    virtual bool init() {
        return true;
    }
    virtual void shutdown() {
    }
    virtual int health() const { return health_; }

protected:
    int health_ = 0;
};

/// ── CRTP helper (compile-time dispatch for concrete implementations) ─

template <typename Derived, typename DataType> class SensorBase : public ISensor<DataType> {
public:
    bool read(DataType &out) final {
        return static_cast<Derived *>(this)->read_impl(out);
    }
    bool init() final {
        return static_cast<Derived *>(this)->init_impl();
    }
    void shutdown() final {
        static_cast<Derived *>(this)->shutdown_impl();
    }

    // Default no-ops — override in derived
    bool init_impl() {
        return true;
    }
    void shutdown_impl() {
    }
};

} // namespace amr::hal::sensor
