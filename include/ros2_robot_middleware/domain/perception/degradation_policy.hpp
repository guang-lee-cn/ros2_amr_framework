#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_DEGRADATION_POLICY_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_DEGRADATION_POLICY_HPP_

#include <cstdint>
#include <string>

namespace amr {
namespace domain {
namespace perception {

// Sensor degradation levels — pure domain state machine.
// Maps sensor availability → fusion strategy.
enum class DegradationLevel : uint8_t {
  FULL      = 0,
  NO_LIDAR  = 1,
  NO_CAMERA = 2,
  NO_IMU    = 3,
  CRITICAL  = 4,
};

// DegradationPolicy evaluates sensor staleness and returns the
// appropriate degradation level. Pure domain logic, zero ROS2 dependency.
class DegradationPolicy {
public:
  struct SensorConfig {
    double timeout_s = 1.0;
    double last_update_s = 0.0;   // seconds since last data
    bool   initialized = false;   // has ever received data
  };

  struct Config {
    double lidar_timeout  = 1.5;
    double imu_timeout    = 0.5;
    double camera_timeout = 3.0;
  };

  DegradationPolicy() {}
  explicit DegradationPolicy(const Config &cfg) : config_(cfg) {}

  // Evaluate degradation level given current sensor staleness.
  // lidar/imu/camera: seconds since last update (negative = never received).
  DegradationLevel evaluate(
    double lidar_age_s,
    double imu_age_s,
    double camera_age_s) const
  {
    bool lidar_missing  = is_stale(lidar_age_s,  config_.lidar_timeout);
    bool imu_missing    = is_stale(imu_age_s,    config_.imu_timeout);
    bool camera_missing = is_stale(camera_age_s, config_.camera_timeout);

    int missing = (lidar_missing ? 1 : 0) +
                  (imu_missing ? 1 : 0) +
                  (camera_missing ? 1 : 0);

    if (missing >= 2) return DegradationLevel::CRITICAL;
    if (missing == 0) return DegradationLevel::FULL;

    if (lidar_missing)  return DegradationLevel::NO_LIDAR;
    if (imu_missing)    return DegradationLevel::NO_IMU;
    if (camera_missing) return DegradationLevel::NO_CAMERA;

    return DegradationLevel::FULL;
  }

  static const char *to_heartbeat_string(DegradationLevel level) {
    switch (level) {
      case DegradationLevel::FULL:      return "alive";
      case DegradationLevel::NO_LIDAR:  return "degraded_no_lidar";
      case DegradationLevel::NO_CAMERA: return "degraded_no_camera";
      case DegradationLevel::NO_IMU:    return "degraded_no_imu";
      case DegradationLevel::CRITICAL:  return "critical";
    }
    return "unknown";
  }

  /// Parse a fusion heartbeat string back to a DegradationLevel.
  /// Returns false for unknown / "inactive" strings.
  static bool from_heartbeat_string(const std::string &s, DegradationLevel &out) {
    if (s == "alive")                { out = DegradationLevel::FULL;      return true; }
    if (s == "degraded_no_lidar")    { out = DegradationLevel::NO_LIDAR;  return true; }
    if (s == "degraded_no_camera")   { out = DegradationLevel::NO_CAMERA; return true; }
    if (s == "degraded_no_imu")      { out = DegradationLevel::NO_IMU;    return true; }
    if (s == "critical")             { out = DegradationLevel::CRITICAL;  return true; }
    return false;
  }

  /// Only FULL is nominal — any degradation means the perception output is
  /// not fully trustworthy (used by the decision gate: dispatch only on alive).
  static bool is_nominal(DegradationLevel level) {
    return level == DegradationLevel::FULL;
  }

private:
  static bool is_stale(double age_s, double timeout_s) {
    return age_s < 0.0 || age_s > timeout_s;  // negative = never received
  }

  Config config_;
};

}  // namespace perception
}  // namespace domain
}  // namespace amr

#endif
