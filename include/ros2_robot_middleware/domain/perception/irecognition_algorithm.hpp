#pragma once
/// @file   irecognition_algorithm.hpp
/// @brief  Semantic recognition strategy interface — RESERVED, not wired in yet.
///
/// Future expansion: a camera recognition backend (e.g. YOLO / AprilTag)
/// registers under RecognitionRegistry and tags perceived objects with a
/// semantic category ("pallet", "person", ...). This mirrors the sensor
/// registry pattern so FusionNode stays open for extension.
///
/// Currently only the interface + registry skeleton exist — no pipeline wiring
/// and no business implementation (per 2026-08-08 review scope decision).

#include "ros2_robot_middleware/hal/sensor/isensor.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace amr {
namespace domain {
namespace perception {

/// Semantic label attached to a perceived object.
struct RecognitionResult {
  std::string category;
  float confidence = 0.0F;
};

/// Backend contract: turn a camera frame into semantic detections.
class IRecognitionAlgorithm {
public:
  virtual ~IRecognitionAlgorithm() = default;
  virtual std::vector<RecognitionResult> recognize(
      const amr::hal::sensor::CameraFrame &frame) = 0;
};

/// Registry — mirrors SensorRegistry; backends self-register by type string.
class RecognitionRegistry {
public:
  using Factory = std::function<std::unique_ptr<IRecognitionAlgorithm>()>;

  static RecognitionRegistry &instance() {
    static RecognitionRegistry reg;
    return reg;
  }

  void register_algorithm(const std::string &type, Factory factory) {
    factories_[type] = std::move(factory);
  }

  std::unique_ptr<IRecognitionAlgorithm> create(const std::string &type) {
    auto it = factories_.find(type);
    if (it == factories_.end()) return nullptr;
    return it->second();
  }

  bool contains(const std::string &type) const {
    return factories_.count(type) > 0;
  }

  std::vector<std::string> types() const {
    std::vector<std::string> result;
    result.reserve(factories_.size());
    for (const auto &[k, _] : factories_) result.push_back(k);
    return result;
  }

private:
  RecognitionRegistry() = default;
  std::unordered_map<std::string, Factory> factories_;
};

}  // namespace perception
}  // namespace domain
}  // namespace amr
