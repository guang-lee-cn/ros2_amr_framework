#ifndef ROS2_ROBOT_MIDDLEWARE_HAL_COMMON_REGISTRY_HPP_
#define ROS2_ROBOT_MIDDLEWARE_HAL_COMMON_REGISTRY_HPP_

/// @file   registry.hpp
/// @brief  Static plugin registry — replaces if-else in SensorFactory.
///
/// 注册的两条路径（按可靠性排序，2026-08-26 D1 验收实验实证）：
///
/// 1. **幂等显式注册函数（推荐，静态库下唯一可靠）**：
///      inline void ensure_mytype_registered() {
///        static bool done = false;
///        if (done) return; done = true;
///        SensorRegistry::instance().register_type("cat", "type", []{...});
///      }
///    在消费节点 on_configure 里调用。框架内置传感器即此模式
///    （register_builtin_sensors）。「静态库中的自注册对象会被链接器丢弃」
///    ——无符号被引用的注册 TU 整个不进产物，make_sensor 返回 nullptr。
///
/// 2. **AMR_REGISTER_SENSOR 宏**：适配器 cpp 里一行声明自注册对象。
///    适用于「注册 TU 必然被链接」的场景（可执行内直接编译/共享库）。
///    静态库 + 第三方经 registry 查找的场景请用路径 1。
///
/// Factory lookup:
///   auto ptr = SensorRegistry::create("lidar", "sick_tim781");
///   if (!ptr) → fail-fast（未注册类型拒绝启动，不降级仿真传感器）。

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

  /// Whether a (category, type) factory is registered.
  bool contains(const std::string &category, const std::string &type) const {
    return factories_.count(category + "_" + type) > 0;
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
// 注：变量名不能用 ##category##type_str 拼——参数是字符串字面量，token
// 粘贴产物非法（D1 实验实证此宏曾无法编译）。用 __LINE__ 保唯一即可。
#define AMR_REG_CAT2(a, b) a##b
#define AMR_REG_CAT(a, b) AMR_REG_CAT2(a, b)
#define AMR_REGISTER_SENSOR(category, type_str, Concrete)                   \
  __attribute__((used))                                                     \
  ::amr::hal::common::Registrar<Concrete>                                   \
      AMR_REG_CAT(_amr_sensor_reg_, __LINE__)(category, type_str)

#endif
