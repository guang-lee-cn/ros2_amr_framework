#pragma once
/// @file   sick_tim781_adapter.hpp
/// @brief  Real LiDAR adapter — bridges ROS2 sensor_msgs/LaserScan to HAL.
///
/// Sick TiM781 is a 270° safety LiDAR commonly used in warehouse AMRs.
/// The sick_scan2 driver publishes sensor_msgs/LaserScan on /scan.
/// This adapter subscribes to that topic and presents data via the
/// SensorBase<Self, LidarScan> CRTP interface.
///
/// Usage (replace SimulatedLidar in FusionNode):
///   auto lidar = std::make_shared<SickTiM781Adapter>(node, "/scan");
///   PerceptionService<SickTiM781Adapter, ...> ps(*lidar, ...);
///
/// Thread safety:
///   - sick_scan2 driver publishes on a DDS callback thread
///   - FusionNode calls read() on its timer thread
///   - Internal mutex ensures consistent snapshot
///
/// Contrast with SimulatedLidar:
///   - Simulated: timer-generated sine wave, no ROS2 dependency
///   - Adapter:   subscribes to real sensor topic, bridges to HAL

#include "ros2_robot_middleware/hal/sensor/isensor.hpp"

#include <chrono>
#include <mutex>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

namespace amr::hal::sensor {

class SickTiM781Adapter
    : public amr::hal::sensor::SensorBase<SickTiM781Adapter,
                                             amr::hal::sensor::LidarScan> {
public:
    /// @param topic  LiDAR topic (default: /scan for sick_scan2)
    explicit SickTiM781Adapter(const std::string &topic = "/scan")
        : topic_(topic) {}

    /// Called by FusionNode after ROS2 node is available. Creates the subscription.
    /// 注意：宿主节点若是 LifecycleNode（组合 rclcpp::Node，非继承），
    /// 不能传 rclcpp::Node&，改用 FusionNode 直接订阅 + feed_scan()。
    void connect(rclcpp::Node &node) {
        sub_ = node.create_subscription<sensor_msgs::msg::LaserScan>(
            topic_, rclcpp::QoS(10).best_effort(),
            [this](sensor_msgs::msg::LaserScan::SharedPtr msg) {
                on_scan(msg);
            });
    }

    /// 由宿主节点（LifecycleNode）直接订阅话题后喂入扫描数据。
    void feed_scan(sensor_msgs::msg::LaserScan::SharedPtr msg) {
        on_scan(std::move(msg));
    }

    /// 读侧断源判定窗口：缓存帧到达超过此时长 = 数据源死亡（e2e 断源
    /// 场景 2026-08-26 实证：此前缓存帧永远 read=true，降级永不触发）。
    static constexpr auto kStaleWindow = std::chrono::seconds(1);

    // ── CRTP contract ────────────────────────────────────────────────

    bool read_impl(amr::hal::sensor::LidarScan &out) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!latest_msg_) return false;
        // 到达时间判活（断源检测）：真驱动死 = 无新 DDS 消息到达。
        // 缓存帧的 header.stamp 是上游事件时刻（保留透传），不用于判活——
        // 传感器静默时戳不再前进，正是要检测的信号。
        if (std::chrono::steady_clock::now() - last_arrival_ > kStaleWindow) {
            return false;
        }

        // Copy into caller-owned buffer (value semantics → thread-safe)
        size_t n = std::min(latest_msg_->ranges.size(),
                            static_cast<size_t>(amr::hal::sensor::LidarScan::kMaxRanges));
        out.range_count     = n;
        out.angle_min       = latest_msg_->angle_min;
        out.angle_increment = latest_msg_->angle_increment;
        // 保留上游事件时刻（打戳规范：驱动不得覆盖，只透传真值）
        out.stamp_ns = static_cast<int64_t>(latest_msg_->header.stamp.sec) * 1000000000LL
                       + static_cast<int64_t>(latest_msg_->header.stamp.nanosec);

        for (size_t i = 0; i < n; ++i) {
            out.ranges[i] = latest_msg_->ranges[i];
        }
        // Clamp any remaining slots to max range
        for (size_t i = n; i < amr::hal::sensor::LidarScan::kMaxRanges; ++i) {
            out.ranges[i] = latest_msg_->range_max;
        }

        // Health: check if ranges look valid
        health_ = (n > 0 && latest_msg_->range_min > 0.0F) ? 0 : 1;

        return true;
    }

    bool init_impl() {
        // Real adapter: sensor is already running (sick_scan2 node).
        // init() just marks readiness — no I2C open needed.
        return true;
    }

private:
    void on_scan(sensor_msgs::msg::LaserScan::SharedPtr msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        latest_msg_ = msg;
        last_arrival_ = std::chrono::steady_clock::now();
    }

    rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr sub_;
    sensor_msgs::msg::LaserScan::SharedPtr latest_msg_;
    std::chrono::steady_clock::time_point last_arrival_;  // 最近一帧到达时刻（断源判活）
    std::string topic_;
    std::mutex mutex_;
};

} // namespace amr::hal::sensor
