#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_DEVICE_DEVICE_REGISTRY_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_DEVICE_DEVICE_REGISTRY_HPP_

/// @file   device_registry.hpp
/// @brief  设备运行时清单（纯 Domain）——inventory + 健康聚合。
///
/// 定位：职责4「设备管理」的运行时层。SensorFactory 管「怎么造」，本表管
/// 「现在有什么、健不健康」——上线注册/下线注销/健康打点/健康清单查询。
/// 产品化方向（需要时再加）：远程配置下发/拓扑关系/持久化。

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace amr {
namespace device {

struct DeviceInfo
{
  std::string id;        // 如 "lidar_front"
  std::string type;      // 如 "sick_tim781"
  int64_t registered_ns = 0;
  uint8_t health = 100;  // 0=失效 100=满血
};

class DeviceRegistry
{
public:
  /// 上线注册；重复 id 返回 false
  bool add(const std::string & id, const std::string & type, int64_t now_ns)
  {
    if (devices_.count(id)) return false;
    devices_[id] = DeviceInfo{id, type, now_ns};
    return true;
  }

  /// 下线注销
  bool remove(const std::string & id) { return devices_.erase(id) > 0; }

  /// 健康打点（驱动层 health() 的汇聚口）
  bool set_health(const std::string & id, uint8_t health)
  {
    auto it = devices_.find(id);
    if (it == devices_.end()) return false;
    it->second.health = health;
    return true;
  }

  const DeviceInfo * find(const std::string & id) const
  {
    auto it = devices_.find(id);
    return it == devices_.end() ? nullptr : &it->second;
  }

  /// 健康清单（阈值过滤；诊断监控的设备侧入口）
  std::vector<DeviceInfo> list(uint8_t min_health = 1) const
  {
    std::vector<DeviceInfo> out;
    for (const auto & [id, d] : devices_) {
      (void)id;
      if (d.health >= min_health) out.push_back(d);
    }
    std::sort(out.begin(), out.end(),
              [](const DeviceInfo & a, const DeviceInfo & b) { return a.id < b.id; });
    return out;
  }

  size_t size() const { return devices_.size(); }

private:
  std::unordered_map<std::string, DeviceInfo> devices_;
};

}  // namespace device
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_DOMAIN_DEVICE_DEVICE_REGISTRY_HPP_
