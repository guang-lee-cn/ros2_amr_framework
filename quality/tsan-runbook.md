# TSAN 夜跑手册（Wave 2.3，三审 2026-08-31）

> 目标：捕获 demo_grid_ 类数据竞争（三审 P0-B 已知嫌疑：MultiThreadedExecutor
> + Reentrant 组下 A* 唯一世界模型零锁）。TSAN 不进 CI（ROS2 内部噪声大、
> 跑一次 20min+），按本手册夜间手动跑。

## 1. 构建（一次性，独立 build-base 不污染常规构建）

```bash
cd ~/code/ros2_ws && source /opt/ros/jazzy/setup.bash && source install/setup.bash
CMAKE_BUILD_PARALLEL_LEVEL=4 nice -n19 colcon build \
  --packages-select ros2_robot_middleware --build-base /tmp/tsan_build \
  --cmake-args -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1"
```

## 2. 抑制 ROS2 内部已知噪声（否则几千条淹没信号）

```bash
export TSAN_OPTIONS="suppress_warnings=1 history_size=7 halt_on_error=0"
# 噪声主源：rmw/fastrtps 内部缓存的 benign race。初筛只看自家帧：
#   grep -A5 'WARNING: ThreadSanitizer' <log> | grep -E 'amr::|ros2_robot_middleware'
# 若需白名单文件（TSAN 无官方 suppression 文件支持 race 之外类别），
# 按函数黑名单：TSAN_OPTIONS 追加 "ignore_non_joined_threads=1"
```

## 3. 跑什么（竞争窗口最大的三个用例）

```bash
cd /tmp/tsan_build/ros2_robot_middleware
./test_decision          # demo_grid_ 主嫌疑：Reentrant×plan loop
./test_e2e_behavior      # 全链 MultiThreaded + 后台线程
./test_motor_ctrl        # execute 20Hz vs 订阅并发
```

## 4. 判读

- **必红（要修）**：调用栈同时出现两个自家帧且至少一个写操作
- **可忽略**：单侧为 `rmw_fastrtps`/`rclcpp` 内部、无自家写
- demo_grid_ 若报：修法优先级 = 专用 `std::mutex`（最小侵入）> 双缓冲
  （读侧零锁，改动大）。参照 decision_node.hpp `goal_mutex_` 的既有模式。

## 5. 历史记录

| 日期 | 结果 | 备注 |
|------|------|------|
| （待首次夜跑回填） | | |
