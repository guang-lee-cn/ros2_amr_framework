#ifndef ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_OTA_AGENT_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_OTA_AGENT_NODE_HPP_

/// @file   ota_agent_node.hpp
/// @brief  OTA 一键代理——参数注入形态：
///
///   ros2 param set /ota_agent ota.target_version 12   → 一键升级到 HEALTH_GATE
///   ros2 param set /ota_agent ota.health_report "ok"  → 重启后健康上报 → 提交/回滚
///
/// 内部：OtaCoordinator::run_update()（porcelain 门面）+ 文件槽位 SlotOps
/// （目录结构同 scripts/ota_sim.sh：slots/slotX/version.txt + boot_target）。

#include "ros2_robot_middleware/domain/ota/ota_coordinator.hpp"
#include "ros2_robot_middleware/infrastructure/amr_node.hpp"

#include <rclcpp/rclcpp.hpp>

#include <fstream>
#include <optional>
#include <string>

namespace amr {
namespace infrastructure {

class OtaAgentNode : public AmrNode
{
public:
  explicit OtaAgentNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  CallbackReturn on_configure_params(const rclcpp_lifecycle::State &);
  void on_param_change(int64_t target);
  void on_health_report(const std::string & report);

  /// 文件槽位操作：读当前引导标记/写版本文件/原子切标记/回滚
  std::optional<char> read_boot_target() const;
  static std::optional<int64_t> read_version(const std::string & path);
  bool write_file(const std::string & path, const std::string & content) const;

  std::string slot_dir_;
  int64_t security_counter_ = 0;
  int64_t last_target_ = 0;
  std::string last_report_;
  rclcpp::TimerBase::SharedPtr poll_timer_;
  std::optional<domain::ota::OtaCoordinator> ota_;
  // 三审 P0-E/D：构造期钉住的公钥（root 属主只读文件）+ 镜像目录——
  // 运行时参数替换对二者均无效，fetch 对实际镜像字节做 sha256 复验
  std::string pinned_public_key_;
  std::string image_dir_;
};

}  // namespace infrastructure
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_OTA_AGENT_NODE_HPP_
