#include "ros2_robot_middleware/hal/ros2_control/diff_drive_system.hpp"

#include <hardware_interface/types/hardware_interface_return_values.hpp>
#include <pluginlib/class_list_macros.hpp>

#include <cmath>
#include <string>

namespace amr {
namespace hal {
namespace ros2_control {

namespace {
/// URDF 接口声明里必须有这些名字，缺一个直接 ERROR（fail-fast，不带病运行）
bool has_interface(
  const std::vector<hardware_interface::InterfaceInfo> & ifaces, const std::string & name)
{
  for (const auto & i : ifaces) {
    if (i.name == name) return true;
  }
  return false;
}
}  // namespace

hardware_interface::CallbackReturn DiffDriveSystem::on_init(
  const hardware_interface::HardwareComponentInterfaceParams & params)
{
  if (hardware_interface::SystemInterface::on_init(params) !=
      hardware_interface::CallbackReturn::SUCCESS)
  {
    return hardware_interface::CallbackReturn::ERROR;
  }

  if (info_.joints.size() != kWheels) {
    RCLCPP_ERROR(
      get_logger(), "DiffDriveSystem 期望 %zu 个关节（左/右轮），URDF 给了 %zu 个",
      kWheels, info_.joints.size());
    return hardware_interface::CallbackReturn::ERROR;
  }

  for (std::size_t i = 0; i < kWheels; ++i) {
    const auto & j = info_.joints[i];
    joint_name_[i] = j.name;
    if (!has_interface(j.state_interfaces, "position") ||
        !has_interface(j.state_interfaces, "velocity") ||
        !has_interface(j.command_interfaces, "velocity"))
    {
      RCLCPP_ERROR(
        get_logger(),
        "关节 '%s' 接口声明不完整：需要 position/velocity 状态 + velocity 命令",
        j.name.c_str());
      return hardware_interface::CallbackReturn::ERROR;
    }
  }

  // 硬件参数（URDF <param>）——非数字值 fail-fast 拒绝 on_init
  // （2026-08-25 审计 P1-c：裸 stod 抛异常会击穿 controller_manager）
  const auto & hp = info_.hardware_parameters;
  auto parse_double = [this](const char *key, const std::string &value,
                             double &out) -> bool {
    try {
      out = std::stod(value);
      return true;
    } catch (const std::exception &) {
      RCLCPP_ERROR(get_logger(), "硬件参数 %s='%s' 不是数字", key, value.c_str());
      return false;
    }
  };
  if (auto it = hp.find("max_wheel_vel"); it != hp.end()) {
    if (!parse_double("max_wheel_vel", it->second, max_wheel_vel_)) {
      return hardware_interface::CallbackReturn::ERROR;
    }
  }
  if (auto it = hp.find("cmd_accel"); it != hp.end()) {
    if (!parse_double("cmd_accel", it->second, cmd_accel_)) {
      return hardware_interface::CallbackReturn::ERROR;
    }
  }
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn DiffDriveSystem::on_activate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // 接管语义：命令初值 = 当前实际状态（停着的轮子保持停，绝不预设 0 之外的值）。
  // 注意：框架持有的接口内存初始为 NaN 而非 0——激活时必须显式写入全部状态，
  // 否则 NaN 流入控制器会触发 cm 自动失活（实测教训，2026-08-24）。
  for (std::size_t i = 0; i < kWheels; ++i) {
    const std::string prefix = joint_name_[i] + "/";
    set_state(prefix + "position", wheel_pos_[i]);
    set_state(prefix + "velocity", wheel_vel_[i]);
    set_command(prefix + "velocity", wheel_vel_[i]);
    wheel_target_[i] = wheel_vel_[i];
  }
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn DiffDriveSystem::on_deactivate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // 卸力语义：目标清零，仿真轮自然减速（真实硬件此处下使能/进阻尼模式）
  for (std::size_t i = 0; i < kWheels; ++i) {
    wheel_target_[i] = 0.0;
  }
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::return_type DiffDriveSystem::read(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & period)
{
  const double dt = period.seconds();
  for (std::size_t i = 0; i < kWheels; ++i) {
    // 一阶响应 + 加速度限制逼近目标：模拟电机拖动特性
    const double dv = wheel_target_[i] - wheel_vel_[i];
    const double dv_clamped = std::clamp(dv, -cmd_accel_ * dt, cmd_accel_ * dt);
    wheel_vel_[i] += dv_clamped;
    wheel_pos_[i] += wheel_vel_[i] * dt;

    const std::string prefix = joint_name_[i] + "/";
    set_state(prefix + "position", wheel_pos_[i]);
    set_state(prefix + "velocity", wheel_vel_[i]);
  }
  return hardware_interface::return_type::OK;
}

hardware_interface::return_type DiffDriveSystem::write(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  // 命令下发：钳位后送仿真硬件目标；下一拍 read() 才反映到状态（真实单拍延迟）
  for (std::size_t i = 0; i < kWheels; ++i) {
    const double cmd = get_command<double>(joint_name_[i] + "/velocity");
    wheel_target_[i] = std::clamp(cmd, -max_wheel_vel_, max_wheel_vel_);
  }
  return hardware_interface::return_type::OK;
}

}  // namespace ros2_control
}  // namespace hal
}  // namespace amr

PLUGINLIB_EXPORT_CLASS(
  amr::hal::ros2_control::DiffDriveSystem, hardware_interface::SystemInterface)
