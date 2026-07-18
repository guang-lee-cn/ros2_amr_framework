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

#include "ros2_robot_middleware/domain/perception/sensor_interface.hpp"

#include <mutex>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

namespace amr::infrastructure::sensors {

class SickTiM781Adapter
    : public amr::domain::sensor::SensorBase<SickTiM781Adapter,
                                             amr::domain::sensor::LidarScan> {
public:
    /// @param topic  LiDAR topic (default: /scan for sick_scan2)
    explicit SickTiM781Adapter(const std::string &topic = "/scan")
        : topic_(topic) {}

    /// Called by FusionNode after ROS2 node is available. Creates the subscription.
    void connect(rclcpp::Node &node) {
        sub_ = node.create_subscription<sensor_msgs::msg::LaserScan>(
            topic_, rclcpp::QoS(10).best_effort(),
            [this](sensor_msgs::msg::LaserScan::SharedPtr msg) {
                on_scan(msg);
            });
    }

    // ── CRTP contract ────────────────────────────────────────────────

    bool read_impl(amr::domain::sensor::LidarScan &out) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!latest_msg_) return false;

        // Copy into caller-owned buffer (value semantics → thread-safe)
        size_t n = std::min(latest_msg_->ranges.size(),
                            static_cast<size_t>(amr::domain::sensor::LidarScan::kMaxRanges));
        out.range_count     = n;
        out.angle_min       = latest_msg_->angle_min;
        out.angle_increment = latest_msg_->angle_increment;

        for (size_t i = 0; i < n; ++i) {
            out.ranges[i] = latest_msg_->ranges[i];
        }
        // Clamp any remaining slots to max range
        for (size_t i = n; i < amr::domain::sensor::LidarScan::kMaxRanges; ++i) {
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
    }

    rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr sub_;
    sensor_msgs::msg::LaserScan::SharedPtr latest_msg_;
    std::string topic_;
    std::mutex mutex_;
};

} // namespace amr::infrastructure::sensors
