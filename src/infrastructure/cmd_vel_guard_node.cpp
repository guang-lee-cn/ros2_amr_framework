/// @file cmd_vel_guard_node.cpp — NAV2 输出的最后一道安全闸（分层共存 L3）。
///
/// 背景：A/B 对照实证自研栈规划层落后（rack_3c 0/4 vs NAV2 4/4），但三轮
/// 审计打磨的 CollisionGuard 安全域（fail-safe 矩阵、全盲硬停、stale 超时）
/// 是 NAV2 没有的增量价值。本节点把它作为独立安全层接在 NAV2 控制器之后：
///
///   NAV2 controller ──/cmd_vel_raw──▶ [本节点 + CollisionGuard] ──/cmd_vel──▶ 底盘
///                                      ▲ /scan_raw（同一激光）
///
/// 职责边界（与 motor_ctrl 的 abort-重规划不同）：
///   - 只钳制速度，不否决目标——目标生命周期归 NAV2 行为树（被拦超时由
///     NAV2 自身的 progress checker 处理，避免重现 Side B 的双头死循环）
///   - 线速度经 guard.clamp()（近障线性减速→硬停）；stopped 时角速度一并
///     归零（原地转向对贴面障碍无意义且加剧擦碰）
///   - 全盲/雷达停更（stale>500ms）→ 硬停，fail-safe 与真机一致
///
/// 无状态滤波器，走普通 Node（同 scene_simulator 形态，非生命周期）。
#include "ros2_robot_middleware/domain/execution/collision_guard.hpp"
#include "ros2_robot_middleware/infrastructure/qos_profiles.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

using amr::domain::execution::CollisionGuard;
using amr::domain::execution::ScanData;

namespace ros2_robot_middleware {

class CmdVelGuardNode : public rclcpp::Node {
public:
  CmdVelGuardNode() : rclcpp::Node("cmd_vel_guard") {
    CollisionGuard::Params p{};
    p.stop_dist = static_cast<float>(
        declare_parameter("guard_stop_dist", 0.30));
    p.safe_dist = static_cast<float>(
        declare_parameter("guard_safe_dist", 0.80));
    p.min_valid_echoes = static_cast<int>(
        declare_parameter("guard_min_valid_echoes",
                          CollisionGuard::kDefaultMinValidEchoes));
    guard_.set_params(p);

    scan_sub_ = create_subscription<sensor_msgs::msg::LaserScan>(
        "/scan_raw", amr::qos::sensor_stream(),
        [this](sensor_msgs::msg::LaserScan::SharedPtr m) {
          ScanData scan;
          scan.ranges = m->ranges;
          scan.angle_min = static_cast<float>(m->angle_min);
          scan.angle_increment = static_cast<float>(m->angle_increment);
          guard_.set_scan(std::move(scan), std::chrono::steady_clock::now());
        });
    cmd_sub_ = create_subscription<geometry_msgs::msg::Twist>(
        "/cmd_vel_raw", amr::qos::reliable_stream(),
        [this](geometry_msgs::msg::Twist::SharedPtr m) { on_cmd(*m); });
    cmd_pub_ = create_publisher<geometry_msgs::msg::Twist>(
        "/cmd_vel", amr::qos::reliable_stream());
    RCLCPP_INFO(get_logger(),
                "guard armed: stop=%.2f safe=%.2f min_echoes=%d",
                p.stop_dist, p.safe_dist, p.min_valid_echoes);
  }

  std::uint64_t interventions() const { return interventions_; }
  const CollisionGuard &guard() const { return guard_; }

private:
  void on_cmd(const geometry_msgs::msg::Twist &in) {
    const auto now = std::chrono::steady_clock::now();
    geometry_msgs::msg::Twist out = in;
    // 倒车指令直接放行：guard 的 clamp 对负速度同样会因前向 FOV 障碍归零
    // （域设计假设纯前进栈），而 NAV2 backup 恢复行为恰要在障碍前倒车
    // 脱困——拦它就把恢复行为废了。倒车远离前向障碍方向安全；后向感知
    // 是独立的真机课题（与域 FOV 设计一致）。
    if (in.linear.x >= 0.0) {
      out.linear.x = guard_.clamp(static_cast<float>(in.linear.x), now);
      if (guard_.stopped(now)) {
        out.angular.z = 0.0;  // 硬停时禁原地转向（贴面障碍下无意义）
      }
    }
    // 拦截提示（限频 2s）：持续拦截时 NAV2 的 progress checker 会接管，
    // 安全闸不越权 abort——Side B 双头死循环的教训
    if (out.linear.x < in.linear.x - 1e-6) {
      if (now - last_intervene_log_ > std::chrono::seconds(2)) {
        last_intervene_log_ = now;
        RCLCPP_WARN(get_logger(),
                    "guard intervene: %.2f→%.2f m/s (nearest %.2fm)",
                    in.linear.x, out.linear.x, guard_.nearest_distance());
      }
      ++interventions_;
    }
    cmd_pub_->publish(out);
  }

  CollisionGuard guard_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_sub_;
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
  std::chrono::steady_clock::time_point last_intervene_log_{};
  std::uint64_t interventions_ = 0;
};

}  // namespace ros2_robot_middleware

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(
      std::make_shared<ros2_robot_middleware::CmdVelGuardNode>());
  rclcpp::shutdown();
  return 0;
}
