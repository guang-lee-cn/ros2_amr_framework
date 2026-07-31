#ifndef ROS2_ROBOT_MIDDLEWARE_HAL_COMMON_REGISTRY_HPP_
#define ROS2_ROBOT_MIDDLEWARE_HAL_COMMON_REGISTRY_HPP_

/// @file   registry.hpp
/// @brief  Static plugin registry — replaces if-else in SensorFactory.
///
/// Adapters self-register via AMR_REGISTER_SENSOR macro; factory looks up
/// by category + type string. No framework code changes when adding a new
/// adapter.
///
/// Usage (in adapter cpp):
///   AMR_REGISTER_SENSOR("lidar", "simulated", SimulatedLidar);
///   AMR_REGISTER_SENSOR("lidar", "sick_tim781", SickTiM781Adapter);
///
/// Factory lookup:
///   auto ptr = SensorRegistry::create("lidar", "sick_tim781");
///   if (!ptr) → fallback / error.

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace amr {
namespace hal {
namespace common {

using SensorFactoryFn = std::function<std::unique_ptr<void, void (*)(void *)>()>;
using TypedSensorFactory = std::function<void *(void *)>;

/// Registry maps "category_type" → factory returning a generic pointer.
/// Caller casts to the concrete ISensor<DataType>.
class SensorRegistry {
public:
  using Factory = std::function<void *()>;

  static SensorRegistry &instance() {
    static SensorRegistry reg;
    return reg;
  }

  /// Register a factory under "category_type".
  void register_type(const std::string &category, const std::string &type,
                     Factory factory) {
    factories_[category + "_" + type] = std::move(factory);
  }

  /// Create a sensor instance by category+type. Returns nullptr if unknown.
  void *create(const std::string &category, const std::string &type) const {
    auto it = factories_.find(category + "_" + type);
    if (it == factories_.end()) return nullptr;
    return it->second();
  }

  /// List all registered "category_type" keys.
  std::vector<std::string> types() const {
    std::vector<std::string> result;
    result.reserve(factories_.size());
    for (const auto &[k, _] : factories_) result.push_back(k);
    return result;
  }

private:
  std::unordered_map<std::string, Factory> factories_;
};

/// Static registration helper — registers at static-init time.
template <typename Concrete>
struct Registrar {
  Registrar(const std::string &category, const std::string &type) {
    SensorRegistry::instance().register_type(category, type,
        []() { return static_cast<void *>(new Concrete()); });
  }
};

}  // namespace common
}  // namespace hal
}  // namespace amr

// Register a concrete sensor under category + type.
#define AMR_REGISTER_SENSOR(category, type_str, Concrete)                  \
  static ::amr::hal::common::Registrar<Concrete>                           \
      _amr_reg_##category##_##type_str(category, type_str)

#endif
