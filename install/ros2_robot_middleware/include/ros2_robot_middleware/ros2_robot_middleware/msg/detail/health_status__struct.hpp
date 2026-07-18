// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from ros2_robot_middleware:msg/HealthStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/health_status.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_STATUS__STRUCT_HPP_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_STATUS__STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rosidl_runtime_cpp/bounded_vector.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


#ifndef _WIN32
# define DEPRECATED__ros2_robot_middleware__msg__HealthStatus __attribute__((deprecated))
#else
# define DEPRECATED__ros2_robot_middleware__msg__HealthStatus __declspec(deprecated)
#endif

namespace ros2_robot_middleware
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct HealthStatus_
{
  using Type = HealthStatus_<ContainerAllocator>;

  explicit HealthStatus_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->node_name = "";
      this->status = "";
      this->last_seen_s = 0.0;
      this->timeout_s = 0.0;
    }
  }

  explicit HealthStatus_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : node_name(_alloc),
    status(_alloc)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->node_name = "";
      this->status = "";
      this->last_seen_s = 0.0;
      this->timeout_s = 0.0;
    }
  }

  // field types and members
  using _node_name_type =
    std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>>;
  _node_name_type node_name;
  using _status_type =
    std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>>;
  _status_type status;
  using _last_seen_s_type =
    double;
  _last_seen_s_type last_seen_s;
  using _timeout_s_type =
    double;
  _timeout_s_type timeout_s;

  // setters for named parameter idiom
  Type & set__node_name(
    const std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>> & _arg)
  {
    this->node_name = _arg;
    return *this;
  }
  Type & set__status(
    const std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>> & _arg)
  {
    this->status = _arg;
    return *this;
  }
  Type & set__last_seen_s(
    const double & _arg)
  {
    this->last_seen_s = _arg;
    return *this;
  }
  Type & set__timeout_s(
    const double & _arg)
  {
    this->timeout_s = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    ros2_robot_middleware::msg::HealthStatus_<ContainerAllocator> *;
  using ConstRawPtr =
    const ros2_robot_middleware::msg::HealthStatus_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<ros2_robot_middleware::msg::HealthStatus_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<ros2_robot_middleware::msg::HealthStatus_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      ros2_robot_middleware::msg::HealthStatus_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<ros2_robot_middleware::msg::HealthStatus_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      ros2_robot_middleware::msg::HealthStatus_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<ros2_robot_middleware::msg::HealthStatus_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<ros2_robot_middleware::msg::HealthStatus_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<ros2_robot_middleware::msg::HealthStatus_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__ros2_robot_middleware__msg__HealthStatus
    std::shared_ptr<ros2_robot_middleware::msg::HealthStatus_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__ros2_robot_middleware__msg__HealthStatus
    std::shared_ptr<ros2_robot_middleware::msg::HealthStatus_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const HealthStatus_ & other) const
  {
    if (this->node_name != other.node_name) {
      return false;
    }
    if (this->status != other.status) {
      return false;
    }
    if (this->last_seen_s != other.last_seen_s) {
      return false;
    }
    if (this->timeout_s != other.timeout_s) {
      return false;
    }
    return true;
  }
  bool operator!=(const HealthStatus_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct HealthStatus_

// alias to use template instance with default allocator
using HealthStatus =
  ros2_robot_middleware::msg::HealthStatus_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace ros2_robot_middleware

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_STATUS__STRUCT_HPP_
