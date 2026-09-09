# TSAN 夜跑手册（Wave 2.3，三审 2026-08-31）

> 目标：捕获 demo_grid_ 类数据竞争（三审 P0-B 已知嫌疑：MultiThreadedExecutor
> + Reentrant 组下 A* 唯一世界模型零锁）。CI nightly 跑**并发核心**
> （ci.yml tsan-nightly，2026-09-08 起）；本手册是本地深跑（三个用例全跑、
> 判读规则、抑制策略），比 CI 那份更全。

## 1. 构建（一次性，独立 build/install 目录不污染常规构建）

```bash
cd ~/code/ros2_ws && source /opt/ros/jazzy/setup.bash && source install/setup.bash
CMAKE_BUILD_PARALLEL_LEVEL=4 nice -n19 colcon build \
  --packages-select ros2_robot_middleware --build-base /tmp/tsan_build \
  --install-base /tmp/tsan_install \
  --cmake-args -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1"
```
> `--install-base` 不能省：只隔离 build-base 时，TSAN 版二进制仍会装进工作区
> `install/`，把常规构建覆盖成插桩版（后续 smoke/launch 静默变慢且行为不同）。

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
| 2026-09-08 本地首跑（WSL2, 6.6 内核） | test_decision 8 测试 | **零 ThreadSanitizer 报告**（含 P0-B 并发压力回归锁）；ASLR 需 setarch -R；BlockedGoal 一测本地 flaky（DDS 发现时序，同代码 CI 四腿绿——本地环境不可信家族，非代码回归） |
| 2026-09-08 起 CI nightly | ci.yml tsan-nightly job（cron 02:00 北京，并发核心） | 自动化防线（三审⑤关闭） |
| 2026-09-08 首跑战果补记 | test_grid_race ×3 | **揪出 P0-B 回归锁测试自身无锁**（写者裸写/读者裸拷贝，名不副实且从未被 TSAN 跑过）——测试加锁自纠后 ×3 零报告；N-R1 变体（注入×快照）同绿 |
| 2026-09-09 CI 首次真跑 | ci.yml tsan-nightly | job 自 09-08 创建起就没跑过一次：装包名笔误（`colcon-common-versions`）在 apt 阶段即红，而 push 事件跳过该 job、日常全绿掩盖三天。修笔误 + 补 `hardware_interface` 等依赖后 dispatch 34311055445 绿：8 tests + **零报告**。本地同参数复跑同结果 |
