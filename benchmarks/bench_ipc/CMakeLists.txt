cmake_minimum_required(VERSION 3.8)
project(bench_ipc)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(ping src/ping.cpp)
target_compile_features(ping PUBLIC cxx_std_17)
ament_target_dependencies(ping rclcpp std_msgs)

add_executable(pong src/pong.cpp)
target_compile_features(pong PUBLIC cxx_std_17)
ament_target_dependencies(pong rclcpp std_msgs)

add_executable(bench_intra src/bench_intra.cpp)

add_executable(bench_durability src/bench_durability.cpp)
target_compile_features(bench_durability PUBLIC cxx_std_17)
ament_target_dependencies(bench_durability rclcpp std_msgs)
target_compile_features(bench_intra PUBLIC cxx_std_17)
ament_target_dependencies(bench_intra rclcpp std_msgs)

install(TARGETS ping pong bench_intra bench_durability
  DESTINATION lib/${PROJECT_NAME})

ament_package()
