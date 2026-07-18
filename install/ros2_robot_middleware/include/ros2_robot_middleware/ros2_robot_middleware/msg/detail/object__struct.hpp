// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from ros2_robot_middleware:msg/Object.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/object.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__OBJECT__STRUCT_HPP_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__OBJECT__STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rosidl_runtime_cpp/bounded_vector.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


#ifndef _WIN32
# define DEPRECATED__ros2_robot_middleware__msg__Object __attribute__((deprecated))
#else
# define DEPRECATED__ros2_robot_middleware__msg__Object __declspec(deprecated)
#endif

namespace ros2_robot_middleware
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct Object_
{
  using Type = Object_<ContainerAllocator>;

  explicit Object_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->id = "";
      this->x = 0.0f;
      this->y = 0.0f;
      this->z = 0.0f;
    }
  }

  explicit Object_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : id(_alloc)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->id = "";
      this->x = 0.0f;
      this->y = 0.0f;
      this->z = 0.0f;
    }
  }

  // field types and members
  using _id_type =
    std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>>;
  _id_type id;
  using _x_type =
    float;
  _x_type x;
  using _y_type =
    float;
  _y_type y;
  using _z_type =
    float;
  _z_type z;

  // setters for named parameter idiom
  Type & set__id(
    const std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>> & _arg)
  {
    this->id = _arg;
    return *this;
  }
  Type & set__x(
    const float & _arg)
  {
    this->x = _arg;
    return *this;
  }
  Type & set__y(
    const float & _arg)
  {
    this->y = _arg;
    return *this;
  }
  Type & set__z(
    const float & _arg)
  {
    this->z = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    ros2_robot_middleware::msg::Object_<ContainerAllocator> *;
  using ConstRawPtr =
    const ros2_robot_middleware::msg::Object_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<ros2_robot_middleware::msg::Object_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<ros2_robot_middleware::msg::Object_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      ros2_robot_middleware::msg::Object_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<ros2_robot_middleware::msg::Object_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      ros2_robot_middleware::msg::Object_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<ros2_robot_middleware::msg::Object_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<ros2_robot_middleware::msg::Object_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<ros2_robot_middleware::msg::Object_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__ros2_robot_middleware__msg__Object
    std::shared_ptr<ros2_robot_middleware::msg::Object_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__ros2_robot_middleware__msg__Object
    std::shared_ptr<ros2_robot_middleware::msg::Object_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const Object_ & other) const
  {
    if (this->id != other.id) {
      return false;
    }
    if (this->x != other.x) {
      return false;
    }
    if (this->y != other.y) {
      return false;
    }
    if (this->z != other.z) {
      return false;
    }
    return true;
  }
  bool operator!=(const Object_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct Object_

// alias to use template instance with default allocator
using Object =
  ros2_robot_middleware::msg::Object_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace ros2_robot_middleware

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__OBJECT__STRUCT_HPP_
