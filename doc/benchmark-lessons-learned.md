# ROS2 DDS 性能测试：踩坑记录与方法论

> 测试日期：2026-07-27
> 环境：WSL2 Ubuntu 24.04, ROS2 Jazzy, Fast-DDS 2.14.6, CycloneDDS 0.10.5

---

## 1. 测试什么？——系统级 vs 通信级

### 1.1 两层测试的区别

| | 系统级 Benchmark | 纯净 DDS Micro-Benchmark |
|------|------|------|
| **被测对象** | 整个 AMR 管线（感知→决策→控制）+ DDS | 只有 DDS 通信层，无应用逻辑 |
| **延迟来源** | DDS 传输 + 算法计算 + 线程调度 | DDS 传输 + 序列化 + 线程调度 |
| **DDS 延迟占比** | <2%（被算法计算淹没） | ~100% |
| **价值** | 回答"DDS 选型对我的项目重要吗？" | 回答"DDS 本身快不快？怎么配置？" |
| **结论** | 不重要（差异淹没在计算中） | 重要（depth 配置是真正的瓶颈） |

### 1.2 为什么两层都要做？

**系统级告诉你"要不要切换 DDS"**——答案是不用。我们的 AMR 管线中，Fusion 延迟 16ms，DDS 差异 10μs，量级差 1000 倍。

**Micro-benchmark 告诉你"DDS 真正的瓶颈在哪"**——答案不是协议开销，是 QoS 配置。`KEEP_LAST(10)` 默认值会让你的高频数据传输丢 95%。

---

## 2. 踩坑记录与解决方案

### 坑 1：KEEP_LAST(10) 默认值导致 95% 丢帧

**现象**：
```
200 条消息发送，只收到 10 条。
无论 RELIABLE 还是 BEST_EFFORT，无论消息大小，都是精确的 10 条。
```

**根因**：
ROS2 默认 QoS 为 `rclcpp::QoS(10)`，即 `KEEP_LAST(10)`。Publisher 的历史缓冲区只保留最后 10 条消息。当 Subscriber 单线程处理速度 < Publisher 发送速度时，新消息覆盖旧消息，只有最后 10 条被投递。

```
时间线：
  t=0ms:   msg#0 发送 → pong 收到，排队等待处理
  t=10ms:  msg#1 发送 → 同上
  ...
  t=100ms: msg#10 发送 → publisher history[0..9] 满
  t=110ms: msg#11 发送 → history[0] 被覆盖 → msg#0 从未被投递！
  ...
  t=2000ms: msg#199 发送 → 最终只有 msg#190-199 被投递
```

**解决**：
```cpp
// ❌ 默认——高频数据必丢
rclcpp::QoS(10).reliable()

// ✅ 匹配你的消息速率
rclcpp::QoS(static_cast<size_t>(msg_count)).reliable()
// 或者：rclcpp::SensorDataQoS()  // 内置 BEST_EFFORT + depth=5
```

**教训**：`KEEP_LAST(N)` 的 N 必须 > 发送速率(Hz) × 消费者最大处理延迟(秒)。否则消息在应用层就已经丢了，DDS 背不了这个锅。

### 坑 2：DDS Discovery 耗时数秒

**现象**：
```
启动 ping → 发完 200 条消息 → 0 条响应
等 5 秒后手动重发 → 全部正常
```

**根因**：
DDS 节点发现（PDP + EDP）不是瞬时的。在 WSL2 虚拟网络中，Ping 和 Pong 的互相发现需要 2-5 秒。如果在发现完成前开始发送，消息无人接收。

**解决**：
```cpp
// ✅ 等待 publisher 匹配到至少一个 subscriber
auto t0 = std::chrono::steady_clock::now();
while (pub_->get_subscription_count() == 0) {
  if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds(10)) {
    RCLCPP_ERROR(get_logger(), "DDS discovery timeout");
    return;
  }
  rclcpp::spin_some(get_node_base_interface());
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
```

**教训**：ROS2 启动后不能立即认为通信就绪。生产代码中用 `wait_for_action_server()` 或检查 `get_subscription_count()`。

### 坑 3：单线程订阅者的队列延迟

**现象**：
```
RTT avg = 800ms。但消息只有 256 字节，本地回环。
理论上 DDS 传输 < 1ms。800ms 是哪来的？
```

**根因**：
Pong 节点是单线程的 `SingleThreadedExecutor`。200 条消息在 subscriber callback 队列中串行排队。第 200 条消息需要等前面 199 条全部处理完才能开始处理。每条消息处理时间看似快（~50μs publish），但累计起来：

```
msg#199 的 RTT = DDS ping→pong (1ms)
               + 排队等待 199 条消息 (199 × 5ms ≈ 1000ms)
               + DDS pong→ping (1ms)
               ≈ 1000ms
```

**解决**：
```cpp
// ✅ 使用多线程 executor 处理高频回调
auto exec = std::make_shared<rclcpp::executors::MultiThreadedExecutor>();
exec->add_node(pong_node);
exec->spin();
```

或者：降低测试消息密度（如 10Hz 而不是 100Hz）。

**教训**：**"DDS 延迟 <1ms"只对稀疏消息成立。** 密集消息场景下，RTT 由消费者处理速度和队列深度决定，DDS 传输延迟反而是最小的一项。

### 坑 4：环境因素——CPU 调频 + 非实时调度

**现象**：
```
所有测试 RTT 都在 600-1600ms，远超物理机预期（<10ms）。
但所有节点在同一 WSL2 实例内，localhost 通信不经过 Windows 网络栈。
```

**根因**：
延迟主要来源不是网络，是：
1. **单线程订阅者队列深度**（坑 3）——主导因素
2. **动态 CPU 调频**（powersave governor）——CPU 从低频切到高频需要 ~100ms
3. **非实时调度**——内核线程可随时抢占用户进程
4. **没有 CPU 隔离**——ROS2 线程可在各核心间迁移，引入 p99 抖动

**解决**：
```bash
# 固定 CPU 频率
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# CPU 隔离 + 实时调度
sudo taskset -c 2,3 chrt -f 99 ros2 run ... bench_ping ...
```

**影响**：修复后预期 RTT 从 ~800ms 降到 ~10ms（物理机）或 ~50ms（WSL2）。

**教训**：在 WSL2 上测出的绝对值不能直接引用为"ROS2 DDS 延迟"——不是网络问题，是**调度环境未受控**。但 Fast-DDS vs CycloneDDS 的相对比较（差异 <5%）不受影响。

### 坑 5：BEST_EFFORT 在虚拟网络中比 RELIABLE 更慢

**现象**：
```
Fast-DDS BEST_EFFORT avg RTT: 1623ms
Fast-DDS RELIABLE  avg RTT: 872ms
```

**根因**：
BEST_EFFORT 无 ACK/重传机制 = 无流控反压。在 WSL2 虚拟网络中，UDP 缓冲区溢出后消息被丢弃，但发送方不知道，继续高速发送。订阅者需要处理更多重传的 UDP 包，反而增加了延迟。

**解决**：在虚拟网络环境中，首选 RELIABLE。物理网络（以太网）中 BEST_EFFORT 才发挥低延迟优势。

**教训**：QoS 配置不是"RELIAvBLE = 慢但可靠，BEST_EFFORT = 快但丢帧"这么简单。在特定网络条件下，这个直觉可能完全反转。

---

## 3. Benchmark 手把手流程

### 3.1 前置条件

```bash
# 确认 DDS 实现已安装
dpkg -l | grep -E "fastrtps|cyclonedds"

# 确认 RMW 可切换
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp    # Fast-DDS（默认）
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp  # CycloneDDS
```

### 3.2 系统级 Benchmark（AMR 全链路）

```bash
# 1. 启动系统
source /opt/ros/jazzy/setup.bash
source ~/code/ros2_ws/install/setup.bash
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp  # 或 rmw_cyclonedds_cpp
ros2 launch ros2_robot_middleware system.launch.py &

# 2. 等待系统稳定（5s）
sleep 5

# 3. 采集 Prometheus 指标
while true; do
  curl -s http://localhost:9090/metrics | \
    grep -E "amr_(fusion|decision|motor|e2e)_latency|amr_sensor_rate|amr_degradation" \
    >> metrics_snapshot.txt
  sleep 2
done

# 4. 60s 后停止，汇总统计
# 使用 bench_dds.sh 脚本自动化
./quality/scripts/bench_dds.sh rmw_fastrtps_cpp 60 results/fastdds
./quality/scripts/bench_dds.sh rmw_cyclonedds_cpp 60 results/cyclonedds
```

### 3.3 纯净 DDS Micro-Benchmark（ping/pong）

```bash
# 1. 编译
colcon build --packages-select ros2_robot_middleware

# 2. 启动 pong（后台）
ros2 run ros2_robot_middleware bench_pong --ros-args -p qos:=reliable &
PONG_PID=$!
sleep 3  # 等待 DDS discovery

# 3. 运行 ping（单次）
ros2 run ros2_robot_middleware bench_ping --ros-args \
  -p rate:=100 -p count:=200 -p size:=256 -p qos:=reliable

# 4. 输出解析
# BENCH_RESULT: received=200 avg_us=872000 p50_us=864000 p99_us=1240000 max_us=1245000

# 5. 矩阵测试脚本
./quality/scripts/bench_dds_rtt.sh  # 自动遍历 RMW × QoS × Size
```

### 3.4 数据解读 Checklist

拿到 benchmark 数据后，按以下顺序检查：

1. **丢帧率 > 0？** → 检查 QoS depth 是否 >= 消息数
2. **RTT > 100ms（本地）？** → 检查是否单线程订阅者，队列深度是否过大
3. **两 DDS 差异 < 5%？** → 正常。DDS 差异在计算密集型场景中可忽略
4. **BEST_EFFORT > RELIABLE？** → 虚拟网络环境正常。标注环境
5. **传感器速率偏低？** → 检查是否是计算节点占用 CPU 过多

---

## 4. 结论：Fast-DDS vs CycloneDDS

### 4.1 我们的测试结论

| 场景 | Fast-DDS | CycloneDDS | 推荐 |
|------|:---:|:---:|------|
| 计算密集型 AMR（Fusion 16ms/帧） | 与 CycloneDDS 等效 | 与 Fast-DDS 等效 | Fast-DDS（默认） |
| 小消息 ping/pong (<1KB) | 872ms avg RTT | 806ms avg RTT | 等效 |
| 大消息 ping/pong (64KB) | 709ms avg RTT | 654ms avg RTT | CycloneDDS 略优 |
| BEST_EFFORT 稳定性 | 正常 | 超时 | Fast-DDS |
| 传感器数据可靠性 | 0% 丢帧 | 0% 丢帧 | 等效 |

**一句话结论：在我们的 WSL2 环境下，两者应用层性能等效。DDS 选型的决策因素在架构层（线程模型/SHM/大消息），不在微秒级延迟差异。**

### 4.2 开源社区测试理论

社区普遍认可的 DDS 测试方法论来自三个来源：

**ROS2 Real-Time Working Group（2024）**：
- 测试必须隔离 DDS 层：独立的 pub/sub 进程，无应用计算
- 必须测 3 个维度：延迟（RTT）、吞吐（msg/s）、发现时间（从启动到首条消息）
- QoS 矩阵：RELIABLE/BEST_EFFORT × VOLATILE/TRANSIENT_LOCAL × 消息大小（64B/1KB/1MB）
- 环境标注：OS、内核版本、网络类型（物理/虚拟）、CPU 调频状态

**eProsima Fast-DDS Performance Report（2025）**：
- SHM transport 下可实现 <10μs RTT（256B，同主机）
- UDP loopback 下 ~50μs RTT
- 跨主机 1Gbps 以太网下 ~200μs RTT
- 关键变量：CPU isolation + `chrt -f 99` + `nohz_full`

**Open Robotics DDS Tuning Guide**：
- 默认 QoS 不适合生产：`KEEP_LAST(10)` 只适合 debug
- 传感器数据必须用 `BEST_EFFORT`：丢帧好过积压
- 大消息（>64KB）必须测试分片：Fast-DDS 默认分片 64KB，CycloneDDS 可配到 1GB
- SHM transport 在 containerized 环境中容易失效（/dev/shm 权限）

### 4.3 我们与社区结论的差异

| 社区结论 | 我们的发现 | 差异原因 |
|------|------|------|
| Fast-DDS 比 CycloneDDS 慢 10-20% | 等效（<2%） | 社区用纯净 ping/pong，我们测 AMR 管线；计算延迟淹没 DDS 差异 |
| BEST_EFFORT 更快 | BEST_EFFORT 更慢（1623ms vs 872ms） | WSL2 虚拟网络，UDP 无 ACK 反压导致缓冲区溢出 |
| RTT < 1ms（同主机） | RTT ~800ms | 单线程订阅者队列深度主导 RTT，不是 DDS 传输 |

**重要提醒**：我们的测试环境是 WSL2 虚拟网络。物理机 Ubuntu 上的结果会显著不同（预期 RTT <1ms 级别）。本文档的价值在于**测试方法论和踩坑经验**，而非绝对性能数字。

---

## 5. 后续改进

- [ ] 在物理机 Ubuntu 上重新运行 benchmark
- [ ] 使用 `perf` + `bpftrace` 分析 DDS 线程模型的热点
- [ ] 测试 SHM transport 在小消息场景的收益
- [ ] 添加 `ros2_control` 的 QoS 配置作为对比基线
- [ ] 使用 `ddsperf`（CycloneDDS 自带工具）做第三方验证

---

*文档版本：v1.0 | 适用环境：WSL2 / 物理机 Ubuntu | 依赖：bench_ping, bench_pong, bench_dds_rtt.sh*
