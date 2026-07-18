cmake_minimum_required(VERSION 3.8)
project(ros2_robot_middleware)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(action_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)

# Custom interfaces
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/Object.msg"
  "msg/PerceptionObjects.msg"
  "msg/HealthStatus.msg"
  "msg/HealthReport.msg"
  "srv/SetParam.srv"
  "action/MoveToPose.action"
  DEPENDENCIES std_msgs action_msgs
)

# Static library — all business logic, linked by both executables and tests
add_library(robot_middleware_lib STATIC
  src/nodes/lidar_node.cpp
  src/nodes/imu_node.cpp
  src/nodes/camera_node.cpp
  src/nodes/fusion_node.cpp
  src/nodes/decision_node.cpp
  src/nodes/motor_ctrl_node.cpp
  src/nodes/health_monitor_node.cpp
  src/nodes/fleet_manager_node.cpp
)

target_include_directories(robot_middleware_lib PUBLIC
  include
  ${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_cpp
)

ament_target_dependencies(robot_middleware_lib
  rclcpp rclcpp_action rclcpp_lifecycle rclcpp_components lifecycle_msgs sensor_msgs geometry_msgs std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target
  ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(robot_middleware_lib "${cpp_typesupport_target}")

# Composable compute nodes — shared library loaded into a container process
# fusion, decision, motor_ctrl share a process (zero-copy, low latency)
# sensor drivers (lidar/imu/camera) remain independent processes (fault isolation)
add_library(compute_component SHARED
  src/nodes/fusion_node.cpp
  src/nodes/decision_node.cpp
  src/nodes/motor_ctrl_node.cpp
)
target_include_directories(compute_component PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_cpp)
ament_target_dependencies(compute_component
  rclcpp rclcpp_action rclcpp_lifecycle rclcpp_components lifecycle_msgs sensor_msgs geometry_msgs std_msgs
)
target_link_libraries(compute_component "${cpp_typesupport_target}")

# Component container — loads compute nodes (fusion + decision + motor_ctrl)
# into a single process. Sensor drivers remain independent processes.
add_executable(compute_container src/compute_container.cpp)
target_link_libraries(compute_container compute_component)
ament_target_dependencies(compute_container rclcpp rclcpp_components)
install(TARGETS compute_component DESTINATION lib)
install(TARGETS compute_container DESTINATION lib/${PROJECT_NAME})

# Thin executables — independent processes for sensor drivers + monitoring
set(INDEPENDENT_NODES lidar imu camera health_monitor fleet_manager)
foreach(node ${INDEPENDENT_NODES})
  add_executable(${node}_node src/nodes/${node}_main.cpp)
  target_link_libraries(${node}_node robot_middleware_lib)
  install(TARGETS ${node}_node DESTINATION lib/${PROJECT_NAME})
endforeach()

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_robot_middleware
    test/test_robot_middleware.cpp
  )
  target_link_libraries(test_robot_middleware robot_middleware_lib)
  target_include_directories(test_robot_middleware PRIVATE include)
  ament_target_dependencies(test_robot_middleware
    rclcpp rclcpp_action rclcpp_lifecycle sensor_msgs geometry_msgs std_msgs
  )
  rosidl_get_typesupport_target(cpp_typesupport_target
    ${PROJECT_NAME} "rosidl_typesupport_cpp")
  target_link_libraries(test_robot_middleware "${cpp_typesupport_target}")
endif()

ament_package()
