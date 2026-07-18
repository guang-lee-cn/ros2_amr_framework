// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from ros2_robot_middleware:msg/PerceptionObjects.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/perception_objects.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__PERCEPTION_OBJECTS__STRUCT_HPP_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__PERCEPTION_OBJECTS__STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rosidl_runtime_cpp/bounded_vector.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.hpp"
// Member 'objects'
#include "ros2_robot_middleware/msg/detail/object__struct.hpp"

#ifndef _WIN32
# define DEPRECATED__ros2_robot_middleware__msg__PerceptionObjects __attribute__((deprecated))
#else
# define DEPRECATED__ros2_robot_middleware__msg__PerceptionObjects __declspec(deprecated)
#endif

namespace ros2_robot_middleware
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct PerceptionObjects_
{
  using Type = PerceptionObjects_<ContainerAllocator>;

  explicit PerceptionObjects_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init)
  {
    (void)_init;
  }

  explicit PerceptionObjects_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init)
  {
    (void)_init;
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _objects_type =
    std::vector<ros2_robot_middleware::msg::Object_<ContainerAllocator>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<ros2_robot_middleware::msg::Object_<ContainerAllocator>>>;
  _objects_type objects;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__objects(
    const std::vector<ros2_robot_middleware::msg::Object_<ContainerAllocator>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<ros2_robot_middleware::msg::Object_<ContainerAllocator>>> & _arg)
  {
    this->objects = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    ros2_robot_middleware::msg::PerceptionObjects_<ContainerAllocator> *;
  using ConstRawPtr =
    const ros2_robot_middleware::msg::PerceptionObjects_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<ros2_robot_middleware::msg::PerceptionObjects_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<ros2_robot_middleware::msg::PerceptionObjects_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      ros2_robot_middleware::msg::PerceptionObjects_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<ros2_robot_middleware::msg::PerceptionObjects_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      ros2_robot_middleware::msg::PerceptionObjects_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<ros2_robot_middleware::msg::PerceptionObjects_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<ros2_robot_middleware::msg::PerceptionObjects_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<ros2_robot_middleware::msg::PerceptionObjects_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__ros2_robot_middleware__msg__PerceptionObjects
    std::shared_ptr<ros2_robot_middleware::msg::PerceptionObjects_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__ros2_robot_middleware__msg__PerceptionObjects
    std::shared_ptr<ros2_robot_middleware::msg::PerceptionObjects_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const PerceptionObjects_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->objects != other.objects) {
      return false;
    }
    return true;
  }
  bool operator!=(const PerceptionObjects_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct PerceptionObjects_

// alias to use template instance with default allocator
using PerceptionObjects =
  ros2_robot_middleware::msg::PerceptionObjects_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace ros2_robot_middleware

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__PERCEPTION_OBJECTS__STRUCT_HPP_
