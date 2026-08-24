#ifndef ROS2_ROBOT_MIDDLEWARE_HAL_ROS2_CONTROL_DIFF_DRIVE_SYSTEM_HPP_
#define ROS2_ROBOT_MIDDLEWARE_HAL_ROS2_CONTROL_DIFF_DRIVE_SYSTEM_HPP_

/// @file   diff_drive_system.hpp
/// @brief  ros2_control SystemInterface 插件：差速底盘（仿真硬件）。
///
/// 定位：AMR HAL 的第一个 ros2_control 落地——补齐 IActuator 缺失的
/// 固定节拍锁步、命令独占(claim)、激活接管语义（对标 diff 见 U4 对标报告）。
///
/// 对应关系：
///   read()  = 仿真编码器采样（速度一阶惯性逼近目标 + 位置积分）
///   write() = 下发速度命令到仿真目标（read→update→write 契约下，
///             write[i] 的命令在 read[i+1] 生效——真实硬件的单拍延迟）
///   on_activate() = 命令接口初值 = 当前状态（接管语义，不跳变）

#include <hardware_interface/system.hpp>

#include <array>
#include <string>
#include <vector>

namespace amr {
namespace hal {
namespace ros2_control {

/// 差速底盘仿真硬件：两轮 velocity 命令 + position/velocity 状态。
/// 关节约定：info_.joints[0]=左轮 [1]=右轮，各含 position/velocity 状态接口
/// 与 velocity 命令接口（URDF <ros2_control> 段声明，框架自动生成接口描述）。
class DiffDriveSystem : public hardware_interface::SystemInterface
{
public:
  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareComponentInterfaceParams & params) override;

  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

  hardware_interface::return_type write(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  static constexpr std::size_t kWheels = 2;

  std::array<std::string, kWheels> joint_name_{};
  std::array<double, kWheels> wheel_pos_{0.0, 0.0};   // 状态：累计转角 rad
  std::array<double, kWheels> wheel_vel_{0.0, 0.0};   // 状态：当前角速度 rad/s
  std::array<double, kWheels> wheel_target_{0.0, 0.0}; // 仿真硬件侧的命令目标

  double max_wheel_vel_{20.0};   // rad/s，命令安全钳位
  double cmd_accel_{40.0};       // rad/s^2，一阶响应的加速度限制
};

}  // namespace ros2_control
}  // namespace hal
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_HAL_ROS2_CONTROL_DIFF_DRIVE_SYSTEM_HPP_
