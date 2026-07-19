# DDS 层分析 — Fast-DDS vs Cyclone DDS 对比 & XML Profile 定制计划

## 1. RMW 切换方式

一行环境变量即可更换底层 DDS 实现，无需重新编译：

```bash
# 默认：Fast-DDS (eProsima)
ros2 launch ros2_robot_middleware system.launch.py

# 切换：Cyclone DDS (Eclipse)
RMW_IMPLEMENTATION=rmw_cyclonedds_cpp ros2 launch ros2_robot_middleware system.launch.py
```

ROS2 启动时读取 `RMW_IMPLEMENTATION`，动态加载对应的 `.so`：

```
rmw_fastrtps_cpp.so    → Fast-DDS
rmw_cyclonedds_cpp.so  → Cyclone DDS
```

> 你的 iTOP-4412 嵌入式板只有 ARM Cortex-A9 + 1GB RAM，Cyclone DDS 更合适（内存占用更低）。

---

## 2. 公开性能基准 (Electronics 2025, Patsnap 2025, UvA Thesis 2025)

### 2.1 延迟 (越小越好)

| Payload | Fast-DDS | Cyclone DDS | 差异 |
|---------|----------|-------------|------|
| 128 B | 0.17 ms | 0.17 ms | 持平 |
| 512 B | 0.18 ms | 0.18 ms | 持平 |
| 2 KB | 0.19 ms | 0.20 ms | Fast-DDS 微优 (~10 μs) |
| 32 KB (1:1) | 1.04 ms | 1.05 ms | 持平 |
| 32 KB (1:8 subs) | 1.21 ms | **1.14 ms** | Cyclone 优 ~70 μs |

**结论：小消息延迟几乎相同。** 本项目 LidarScan (~3KB) 和 ImuData (76B) 在此范围内，差异可忽略。

### 2.2 吞吐量 (越大越好)

| Payload | Fast-DDS | Cyclone DDS | Cyclone 倍数 |
|---------|----------|-------------|:---:|
| 128 B | 7.25 Mb/s | 14.45 Mb/s | **2×** |
| 512 B | 28.7 Mb/s | 56.5 Mb/s | **2×** |
| 2 KB | 47.6 Mb/s | 219.3 Mb/s | **4.6×** |
| 32 KB | 146.4 Mb/s | 290.8 Mb/s | **2×** |

**结论：Cyclone DDS 吞吐量碾压。** 本项目 CameraImage 每帧 900KB、5Hz = 4.6MB/s，实际不触碰到瓶颈，但如果你扩展为多相机（4路×4.6=18.4MB/s），Cyclone DDS 的余量更大。

### 2.3 CPU 占用

> Cyclone DDS 比 Fast-DDS 低 **15-20%** CPU (Patsnap 2025)。

原因：Cyclone 单线程全局服务核 vs Fast-DDS 每 Participant 独立线程池。

### 2.4 Wi-Fi / 嵌入式

| 场景 | Fast-DDS | Cyclone DDS |
|------|:---:|:---:|
| Wi-Fi 稳定性 | 较差 | **更好** (UvA 2025) |
| 嵌入式 ARM | 较重 | **更轻** (更低 RAM 占用) |
| ROS2 默认 | 从 Humble 起被替换 | **Humble+ 默认** |

---

## 3. 本项目本地测试数据

测试环境：WSL2 Ubuntu 24.04, 6 个节点全链路运行, 30s 采样窗口。

| 指标 | Fast-DDS | Cyclone DDS |
|------|----------|-------------|
| 发现时间 | 1020 ms | 1063 ms |
| best_effort 延迟 | ~1 ms | ~1 ms |
| reliable 延迟 | ~1 ms | ~1 ms |

本地测试的 1ms 延迟包含了 WSL2 虚拟化层 + `ros2 topic delay` 自身开销（实际 DDS 层延迟在 0.2ms 量级，见 2.1 公开数据）。

---

## 4. 选型决策矩阵

| 维度 | Fast-DDS | Cyclone DDS | 本项目选择 |
|------|:---:|:---:|:---:|
| 小消息延迟 (< 3KB) | ★★★★★ | ★★★★★ | 持平 |
| 大消息吞吐量 | ★★★ | ★★★★★ | 当前够用 |
| CPU 占用 | ★★★ | ★★★★★ | Cyclone 优 |
| XML Profile 可定制 | ★★★★★ | ★★★ | Fast-DDS 优 |
| 全 QoS 22 项 | ★★★★★ | ★★★★ | — |
| Wi-Fi 稳定性 | ★★★ | ★★★★★ | 若上真机 |
| 嵌入式适配 | ★★★ | ★★★★★ | iTOP-4412 |

**本项目选择 Fast-DDS (默认) 的理由：**
- 需求是"DDS 层深度定制能力"，Fast-DDS XML Profile 自定义空间最大
- 全 localhost 通信，不碰网络瓶颈
- 6 节点不超过 5MB/s 带宽，远低于二者上限

---

## 5. Fast-DDS XML Profile 定制 (R0 核心技术)

XML Profile 是 Fast-DDS 独有功能，允许在不改代码的情况下调底层参数：

```xml
<profiles>
    <!-- Participant 级别：加速启动发现 -->
    <participant profile_name="robot_participant">
        <rtps>
            <builtin>
                <initialAnnouncements>
                    <count>5</count>
                </initialAnnouncements>
            </builtin>
        </rtps>
    </participant>

    <!-- Writer 级别：IMU 通道预分配内存 -->
    <data_writer profile_name="imu_writer">
        <qos>
            <reliability>
                <kind>RELIABLE</kind>
            </reliability>
        </qos>
        <historyMemoryPolicy>PREALLOCATED</historyMemoryPolicy>
    </data_writer>

    <!-- Reader 级别：Camera 通道动态内存 -->
    <data_reader profile_name="camera_reader">
        <qos>
            <reliability>
                <kind>BEST_EFFORT</kind>
            </reliability>
        </qos>
        <historyMemoryPolicy>DYNAMIC</historyMemoryPolicy>
    </data_reader>
</profiles>
```

### 可调参数及影响

| 参数 | 默认值 | 自定义值 | 目的 |
|------|--------|---------|------|
| `initialAnnouncements.count` | 3 | 5 | 加速冷启动 RTPS 发现（多 2 次握手帧） |
| `historyMemoryPolicy` (IMU) | DYNAMIC | PREALLOCATED | 消除高频 reliable 通道的动态内存抖动 |
| `historyMemoryPolicy` (Camera) | DYNAMIC | DYNAMIC | 大帧 (900KB) 保持按需分配 |
| `socketBufferSize` | 系统默认 | 待测 | best_effort 通道高负载防丢帧 |

### 如何使用

```bash
# 1. 创建 XML 文件
# 2. 设置环境变量
export FASTRTPS_DEFAULT_PROFILES_FILE=/path/to/profiles.xml

# 3. 正常启动，Fast-DDS 自动解析 XML 并应用
ros2 launch ros2_robot_middleware system.launch.py
```

---

## 6. Summary

> "RMW 切换就一行环境变量。Fast-DDS 和 Cyclone DDS 在小消息延迟上几乎相同（~0.2ms），但 Cyclone 吞吐量高 2-4 倍、CPU 低 15%。我选 Fast-DDS 是因为它能用 XML Profile 做颗粒级调参 — initialAnnouncements、historyMemoryPolicy、socketBufferSize — 这些 Cyclone DDS 调不了。ROS2 Jazzy 默认是 Fast-DDS，也是因为定制性。"

---

## 参考来源

- [ROS2 Networking: Cyclone DDS vs Fast DDS — Throughput, Latency And CPU (Patsnap 2025)](https://eureka.patsnap.com/report-ros-2-networking-cyclonedds-vs-fast-dds-throughput-latency-and-cpu)
- [Fast-DDS vs Cyclone DDS in Jazzy: 6 Performance Benchmarks (CSDN)](https://wenku.csdn.net/column/2k4nxhhteo)
- [Advancing user-space networking for DDS (ScienceDirect 2025)](https://www.sciencedirect.com/science/article/pii/S1574119225000021)
- [eProsima Performance (Official)](https://www.eprosima.com/developer-resources/performance)
- [Electronics 2025, 14, 2377 — DDS Performance Journal Paper](https://mdpi-res.com/d_attachment/electronics/electronics-14-02377/article_deploy/electronics-14-02377.pdf)
- [UvA Thesis: Cyclone DDS Wi-Fi Stability (2025)](https://scripties.uba.uva.nl/search?id=record_56186)
