cmake_minimum_required(VERSION 3.8)
project(ros2_robot_middleware)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(PCL QUIET)  # optional — for PCL cluster backend + benchmark
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(spdlog REQUIRED)
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
  src/infrastructure/lidar_node.cpp
  src/infrastructure/imu_node.cpp
  src/infrastructure/camera_node.cpp
  src/infrastructure/fusion_node.cpp
  src/infrastructure/decision_node.cpp
  src/infrastructure/motor_ctrl_node.cpp
  src/infrastructure/health_monitor_node.cpp
  src/infrastructure/prometheus_http_server.cpp
  src/infrastructure/fleet_manager_node.cpp
  src/observability/metrics_registry.cpp
)

target_include_directories(robot_middleware_lib PUBLIC
  include
  ${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_cpp
)

ament_target_dependencies(robot_middleware_lib
  rclcpp rclcpp_action rclcpp_lifecycle rclcpp_components lifecycle_msgs sensor_msgs geometry_msgs nav_msgs tf2_ros diagnostic_msgs spdlog std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target
  ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(robot_middleware_lib "${cpp_typesupport_target}")

# Composable compute nodes — shared library loaded into a container process
# fusion, decision, motor_ctrl share a process (zero-copy, low latency)
# sensor drivers (lidar/imu/camera) remain independent processes (fault isolation)
add_library(compute_component SHARED
  src/infrastructure/fusion_node.cpp
  src/infrastructure/decision_node.cpp
  src/infrastructure/motor_ctrl_node.cpp
  src/observability/metrics_registry.cpp
)
target_include_directories(compute_component PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_cpp)
ament_target_dependencies(compute_component
  rclcpp rclcpp_action rclcpp_lifecycle rclcpp_components lifecycle_msgs sensor_msgs geometry_msgs nav_msgs tf2_ros tf2_geometry_msgs diagnostic_msgs spdlog std_msgs
)
target_link_libraries(compute_component "${cpp_typesupport_target}")

# Component container — loads compute nodes (fusion + decision + motor_ctrl)
# into a single process. Sensor drivers remain independent processes.
add_executable(compute_container src/infrastructure/compute_container.cpp)
# compute_component has fusion/decision/motor; robot_middleware_lib provides
# PrometheusHttpServer + metrics_registry (used by perf endpoint).
target_link_libraries(compute_container compute_component robot_middleware_lib)
ament_target_dependencies(compute_container rclcpp rclcpp_components spdlog)
target_link_libraries(compute_container fmt::fmt)
install(TARGETS compute_component DESTINATION lib)
install(TARGETS compute_container DESTINATION lib/${PROJECT_NAME})

# Thin executables — independent processes for sensor drivers + monitoring
set(INDEPENDENT_NODES lidar imu camera health_monitor fleet_manager)
foreach(node ${INDEPENDENT_NODES})
  add_executable(${node}_node src/infrastructure/${node}_main.cpp)
  target_link_libraries(${node}_node robot_middleware_lib)
  install(TARGETS ${node}_node DESTINATION lib/${PROJECT_NAME})
endforeach()

# Scene simulator — synthetic sensor driver for the interview demo
# (replaces Gazebo; gz-sim gpu_lidar has engine bugs on this platform).
add_executable(scene_simulator
  src/infrastructure/scene_simulator_main.cpp
  src/infrastructure/scene_simulator_node.cpp)
target_link_libraries(scene_simulator robot_middleware_lib)
ament_target_dependencies(scene_simulator visualization_msgs)
install(TARGETS scene_simulator DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY config launch worlds
  DESTINATION share/${PROJECT_NAME})

# ── Performance instrumentation option ──────────────────────────────
# cmake -DAMR_PERF_INSTRUMENTATION=ON .. → AMR_PERF_PHASE() real timers
# cmake -DAMR_PERF_INSTRUMENTATION=OFF .. (default) → 3 no-op macros, zero overhead
#
# Uses configure_file() to generate the header:
#   ON:  #include <atomic> + PerfRegistry class + ScopedPerfTimer class
#   OFF: #define AMR_PERF_PHASE(name) ((void)0)  (3 lines, no includes)
option(AMR_PERF_INSTRUMENTATION "Enable compile-time performance instrumentation" OFF)

if(AMR_PERF_INSTRUMENTATION)
  # Compile-time macro so #ifdef AMR_PERF_INSTRUMENTATION in .cpp works
  # (e.g. compute_container's perf endpoint guard).
  add_compile_definitions(AMR_PERF_INSTRUMENTATION)

  set(AMR_PERF_MACROS
"#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#define AMR_PERF_CONCAT2(a, b) a##b
#define AMR_PERF_CONCAT(a, b)  AMR_PERF_CONCAT2(a, b)

#define AMR_PERF_PHASE(name) \\
  ::amr::observability::ScopedPerfTimer AMR_PERF_CONCAT(_amr_perf_, __LINE__)(name)

#define AMR_PERF_POINT(name) \\
  ::amr::observability::PerfRegistry::instance().count(name)

#define AMR_PERF_RECORD(name, elapsed_us) \\
  ::amr::observability::PerfRegistry::instance().record(name, elapsed_us)

// ── Generated implementation ─────────────────────────────────────
// See observability/perf_instrumentation_impl.hpp for full class definitions.
#include \"ros2_robot_middleware/observability/perf_instrumentation_impl.hpp\"")
else()
  set(AMR_PERF_MACROS
"#define AMR_PERF_PHASE(name)              ((void)0)
#define AMR_PERF_POINT(name)              ((void)0)
#define AMR_PERF_RECORD(name, elapsed_us) ((void)0)")
endif()

# ── Performance instrumentation header ─────────────────────────────
# Generates include/generated/perf_instrumentation.hpp:
#   AMR_PERF_INSTRUMENTATION=OFF → 3 no-op macros (4 lines, zero overhead)
#   AMR_PERF_INSTRUMENTATION=ON  → PerfRegistry + ScopedPerfTimer (self-contained)
#
# Product code: #include "generated/perf_instrumentation.hpp"
set(PERF_TEMPLATE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/include/ros2_robot_middleware/generated")
set(PERF_OUTPUT
  "${CMAKE_CURRENT_SOURCE_DIR}/include/generated/perf_instrumentation.hpp")

if(AMR_PERF_INSTRUMENTATION)
  configure_file("${PERF_TEMPLATE_DIR}/perf_on.hpp" "${PERF_OUTPUT}" COPYONLY)
else()
  configure_file("${PERF_TEMPLATE_DIR}/perf_noop.hpp" "${PERF_OUTPUT}" COPYONLY)
endif()


# ── DDS micro-benchmark tools (always built, instrumented when AMR_PERF_INSTRUMENTATION=ON) ──
add_executable(bench_ping quality/src/bench_ping.cpp)
target_link_libraries(bench_ping robot_middleware_lib)
install(TARGETS bench_ping DESTINATION lib/${PROJECT_NAME})

add_executable(bench_pong quality/src/bench_pong.cpp)
target_link_libraries(bench_pong robot_middleware_lib)
install(TARGETS bench_pong DESTINATION lib/${PROJECT_NAME})

if(PCL_FOUND)
  add_executable(bench_cluster quality/src/bench_cluster.cpp)
  target_link_libraries(bench_cluster robot_middleware_lib ${PCL_LIBRARIES})
  install(TARGETS bench_cluster DESTINATION lib/${PROJECT_NAME})
endif()

if(BUILD_TESTING)
  # Enable AMR_TEST_ONLY() macro in library code for test-only method implementations
  target_compile_definitions(robot_middleware_lib PUBLIC AMR_TEST_HOOKS)
  target_compile_definitions(compute_component PUBLIC AMR_TEST_HOOKS)

  find_package(ament_cmake_gtest REQUIRED)

  # Common configuration for all test targets
  function(add_amr_test TEST_NAME TEST_SOURCES)
    ament_add_gtest(${TEST_NAME} ${TEST_SOURCES})
    target_link_libraries(${TEST_NAME} robot_middleware_lib)
    target_include_directories(${TEST_NAME} PRIVATE include quality/src)
    target_compile_definitions(${TEST_NAME} PRIVATE AMR_TEST_HOOKS)  # enables AMR_TEST_ONLY() macro
    ament_target_dependencies(${TEST_NAME}
      rclcpp rclcpp_action rclcpp_lifecycle sensor_msgs geometry_msgs std_msgs
      tf2_ros tf2_geometry_msgs
    )
    rosidl_get_typesupport_target(cpp_typesupport_target
      ${PROJECT_NAME} "rosidl_typesupport_cpp")
    target_link_libraries(${TEST_NAME} "${cpp_typesupport_target}")
  endfunction()

  add_amr_test(test_sensor_nodes   quality/src/test_sensor_nodes.cpp)
  add_amr_test(test_motor_ctrl     quality/src/test_motor_ctrl.cpp)
  add_amr_test(test_decision       quality/src/test_decision.cpp)
  add_amr_test(test_fusion         quality/src/test_fusion.cpp)
  add_amr_test(test_observability  quality/src/test_observability.cpp)
  add_amr_test(test_kalman_filter quality/src/test_kalman_filter.cpp)
  add_amr_test(test_monitoring    quality/src/test_monitoring.cpp)
  add_amr_test(test_sensor_hal    quality/src/test_sensor_hal.cpp)
  add_amr_test(test_dbscan        quality/src/test_dbscan.cpp)
  add_amr_test(test_tracker       quality/src/test_tracker.cpp)
  add_amr_test(test_astar         quality/src/test_astar.cpp)
  add_amr_test(test_pure_pursuit  quality/src/test_pure_pursuit.cpp)
  add_amr_test(test_path_smoother quality/src/test_path_smoother.cpp)
  add_amr_test(test_track_error  quality/src/test_track_error.cpp)
  add_amr_test(test_grid_updater quality/src/test_grid_updater.cpp)
  add_amr_test(test_collision_guard quality/src/test_collision_guard.cpp)
  add_amr_test(test_vfh_avoidance  quality/src/test_vfh_avoidance.cpp)
  add_amr_test(test_velocity_smoother quality/src/test_velocity_smoother.cpp)
  add_amr_test(test_goal_dispatch_gate quality/src/test_goal_dispatch_gate.cpp)
  add_amr_test(test_simulated_scene quality/src/test_simulated_scene.cpp)
  add_amr_test(test_integration_scenario quality/src/test_integration_scenario.cpp)
  add_amr_test(test_depth_obstacle_detector quality/src/test_depth_obstacle_detector.cpp)
  add_amr_test(test_degradation_policy quality/src/test_degradation_policy.cpp)
endif()

ament_package()
