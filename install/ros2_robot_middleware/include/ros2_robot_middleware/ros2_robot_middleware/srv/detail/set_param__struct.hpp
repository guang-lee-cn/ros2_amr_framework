// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from ros2_robot_middleware:srv/SetParam.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/srv/set_param.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__STRUCT_HPP_
#define ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rosidl_runtime_cpp/bounded_vector.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


#ifndef _WIN32
# define DEPRECATED__ros2_robot_middleware__srv__SetParam_Request __attribute__((deprecated))
#else
# define DEPRECATED__ros2_robot_middleware__srv__SetParam_Request __declspec(deprecated)
#endif

namespace ros2_robot_middleware
{

namespace srv
{

// message struct
template<class ContainerAllocator>
struct SetParam_Request_
{
  using Type = SetParam_Request_<ContainerAllocator>;

  explicit SetParam_Request_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->param_name = "";
      this->value = 0.0;
    }
  }

  explicit SetParam_Request_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : param_name(_alloc)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->param_name = "";
      this->value = 0.0;
    }
  }

  // field types and members
  using _param_name_type =
    std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>>;
  _param_name_type param_name;
  using _value_type =
    double;
  _value_type value;

  // setters for named parameter idiom
  Type & set__param_name(
    const std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>> & _arg)
  {
    this->param_name = _arg;
    return *this;
  }
  Type & set__value(
    const double & _arg)
  {
    this->value = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator> *;
  using ConstRawPtr =
    const ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__ros2_robot_middleware__srv__SetParam_Request
    std::shared_ptr<ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__ros2_robot_middleware__srv__SetParam_Request
    std::shared_ptr<ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const SetParam_Request_ & other) const
  {
    if (this->param_name != other.param_name) {
      return false;
    }
    if (this->value != other.value) {
      return false;
    }
    return true;
  }
  bool operator!=(const SetParam_Request_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct SetParam_Request_

// alias to use template instance with default allocator
using SetParam_Request =
  ros2_robot_middleware::srv::SetParam_Request_<std::allocator<void>>;

// constant definitions

}  // namespace srv

}  // namespace ros2_robot_middleware


#ifndef _WIN32
# define DEPRECATED__ros2_robot_middleware__srv__SetParam_Response __attribute__((deprecated))
#else
# define DEPRECATED__ros2_robot_middleware__srv__SetParam_Response __declspec(deprecated)
#endif

namespace ros2_robot_middleware
{

namespace srv
{

// message struct
template<class ContainerAllocator>
struct SetParam_Response_
{
  using Type = SetParam_Response_<ContainerAllocator>;

  explicit SetParam_Response_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->success = false;
      this->message = "";
    }
  }

  explicit SetParam_Response_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : message(_alloc)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->success = false;
      this->message = "";
    }
  }

  // field types and members
  using _success_type =
    bool;
  _success_type success;
  using _message_type =
    std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>>;
  _message_type message;

  // setters for named parameter idiom
  Type & set__success(
    const bool & _arg)
  {
    this->success = _arg;
    return *this;
  }
  Type & set__message(
    const std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>> & _arg)
  {
    this->message = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator> *;
  using ConstRawPtr =
    const ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__ros2_robot_middleware__srv__SetParam_Response
    std::shared_ptr<ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__ros2_robot_middleware__srv__SetParam_Response
    std::shared_ptr<ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const SetParam_Response_ & other) const
  {
    if (this->success != other.success) {
      return false;
    }
    if (this->message != other.message) {
      return false;
    }
    return true;
  }
  bool operator!=(const SetParam_Response_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct SetParam_Response_

// alias to use template instance with default allocator
using SetParam_Response =
  ros2_robot_middleware::srv::SetParam_Response_<std::allocator<void>>;

// constant definitions

}  // namespace srv

}  // namespace ros2_robot_middleware


// Include directives for member types
// Member 'info'
#include "service_msgs/msg/detail/service_event_info__struct.hpp"

#ifndef _WIN32
# define DEPRECATED__ros2_robot_middleware__srv__SetParam_Event __attribute__((deprecated))
#else
# define DEPRECATED__ros2_robot_middleware__srv__SetParam_Event __declspec(deprecated)
#endif

namespace ros2_robot_middleware
{

namespace srv
{

// message struct
template<class ContainerAllocator>
struct SetParam_Event_
{
  using Type = SetParam_Event_<ContainerAllocator>;

  explicit SetParam_Event_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : info(_init)
  {
    (void)_init;
  }

  explicit SetParam_Event_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : info(_alloc, _init)
  {
    (void)_init;
  }

  // field types and members
  using _info_type =
    service_msgs::msg::ServiceEventInfo_<ContainerAllocator>;
  _info_type info;
  using _request_type =
    rosidl_runtime_cpp::BoundedVector<ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator>, 1, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator>>>;
  _request_type request;
  using _response_type =
    rosidl_runtime_cpp::BoundedVector<ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator>, 1, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator>>>;
  _response_type response;

  // setters for named parameter idiom
  Type & set__info(
    const service_msgs::msg::ServiceEventInfo_<ContainerAllocator> & _arg)
  {
    this->info = _arg;
    return *this;
  }
  Type & set__request(
    const rosidl_runtime_cpp::BoundedVector<ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator>, 1, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<ros2_robot_middleware::srv::SetParam_Request_<ContainerAllocator>>> & _arg)
  {
    this->request = _arg;
    return *this;
  }
  Type & set__response(
    const rosidl_runtime_cpp::BoundedVector<ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator>, 1, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<ros2_robot_middleware::srv::SetParam_Response_<ContainerAllocator>>> & _arg)
  {
    this->response = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    ros2_robot_middleware::srv::SetParam_Event_<ContainerAllocator> *;
  using ConstRawPtr =
    const ros2_robot_middleware::srv::SetParam_Event_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<ros2_robot_middleware::srv::SetParam_Event_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<ros2_robot_middleware::srv::SetParam_Event_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      ros2_robot_middleware::srv::SetParam_Event_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<ros2_robot_middleware::srv::SetParam_Event_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      ros2_robot_middleware::srv::SetParam_Event_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<ros2_robot_middleware::srv::SetParam_Event_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<ros2_robot_middleware::srv::SetParam_Event_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<ros2_robot_middleware::srv::SetParam_Event_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__ros2_robot_middleware__srv__SetParam_Event
    std::shared_ptr<ros2_robot_middleware::srv::SetParam_Event_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__ros2_robot_middleware__srv__SetParam_Event
    std::shared_ptr<ros2_robot_middleware::srv::SetParam_Event_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const SetParam_Event_ & other) const
  {
    if (this->info != other.info) {
      return false;
    }
    if (this->request != other.request) {
      return false;
    }
    if (this->response != other.response) {
      return false;
    }
    return true;
  }
  bool operator!=(const SetParam_Event_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct SetParam_Event_

// alias to use template instance with default allocator
using SetParam_Event =
  ros2_robot_middleware::srv::SetParam_Event_<std::allocator<void>>;

// constant definitions

}  // namespace srv

}  // namespace ros2_robot_middleware

namespace ros2_robot_middleware
{

namespace srv
{

struct SetParam
{
  using Request = ros2_robot_middleware::srv::SetParam_Request;
  using Response = ros2_robot_middleware::srv::SetParam_Response;
  using Event = ros2_robot_middleware::srv::SetParam_Event;
};

}  // namespace srv

}  // namespace ros2_robot_middleware

#endif  // ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__STRUCT_HPP_
