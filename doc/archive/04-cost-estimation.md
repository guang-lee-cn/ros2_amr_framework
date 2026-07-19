# 成本核算

## 1. 运行时资源

### 1.1 CPU 负载等级定义

| 等级 | 单核占用 | 典型场景 |
|------|---------|---------|
| 极低 | <1% | 纯定时器等待、少量内存拷贝、标记读写 |
| 低 | 1-5% | 简单浮点运算（sin/cos）、小消息序列化（<10KB） |
| 中 | 5-15% | 大消息序列化（>100KB）、频繁内存分配 |
| 高 | >15% | 计算密集型（图像处理、矩阵运算、大量数值迭代） |

### 1.2 消息大小精确计算

基于 CDR 序列化规则（little-endian，4 字节对齐，Header 约 28 B）：

**LidarScan @ 10Hz**

| 字段 | 计算 | 大小 |
|------|------|------|
| Header (stamp + frame_id) | 固定 | 28 B |
| ranges[360] | 360 × 4 B (float32) | 1,440 B |
| intensities[360] | 360 × 4 B (float32) | 1,440 B |
| time_increment | 1 × 4 B | 4 B |
| angle_min + max + increment | 3 × 4 B | 12 B |
| **合计** | | **2,924 B/帧 = 29.2 KB/s** |

**ImuData @ 100Hz**

| 字段 | 计算 | 大小 |
|------|------|------|
| Header | 固定 | 28 B |
| angular_velocity[3] | 3 × 8 B (float64) | 24 B |
| linear_acceleration[3] | 3 × 8 B (float64) | 24 B |
| **合计** | | **76 B/帧 = 7.6 KB/s** |

**CameraImage @ 5Hz**

| 字段 | 计算 | 大小 |
|------|------|------|
| Header | 固定 | 28 B |
| height + width + step | 3 × 4 B | 12 B |
| encoding | ~8 B | 8 B |
| is_bigendian | 1 B (+3 B padding) | 4 B |
| data[640 × 480 × 3] | 921,600 × 1 B (uint8) | 921,600 B |
| **合计** | | **921,652 B/帧 = 4.61 MB/s** |

**PerceptionObjects @ 5Hz**

| 字段 | 计算 | 大小 |
|------|------|------|
| Header | 固定 | 28 B |
| objects[] (≤5 个) | 5 × (8+12) B | ~100 B |
| **合计** | | **~128 B/帧 = 640 B/s** |

**MoveToPose Action (Goal + Feedback)**

| 消息 | 大小 |
|------|------|
| Goal (4 × float32) | 16 B |
| Feedback (4 × float32) | 16 B |
| Result (4 × float32 + bool) | 20 B |

### 1.3 单节点资源剖面

| 节点 | 频率 | 每帧大小 | 带宽 | CPU | 瓶颈分析 |
|------|------|---------|------|-----|---------|
| lidar | 10Hz | 2.9 KB | 29 KB/s | 低 (3%) | 360 次 sin/cos + 2.9KB CDR 序列化 |
| imu | 100Hz | 76 B | 8 KB/s | 极低 (<1%) | 3 次 sin/cos + 76B 序列化 |
| camera | 5Hz | 900 KB | 4.6 MB/s | 中 (10%) | 921,600 次随机数生成 + 900KB CDR 序列化 |
| fusion | 5Hz | 128 B | 0.6 KB/s | 低 (2%) | 数组下采样 + 浮点比较 + 路径点循环 |
| decision | 事件驱动 | 16 B | — | 极低 (<1%) | 仅在收到 perception 时解析 |
| motor_ctrl | 10Hz | 16 B | 160 B/s | 极低 (<1%) | 直线插值 + feedback 发布 |
| Fast-DDS | — | — | 4.64 MB/s | 中 (5%) | RTPS 发现 + QoS 匹配 + CDR 序列化/反序列化 |

注：Fast-DDS 带宽为全部 Topic 流量之和（29.2K + 7.6K + 4.61M + 0.64K ≈ 4.64 MB/s），都是 localhost 通信。

### 1.4 汇总

| 资源 | 最低 | 推荐 | 约束分析 |
|------|------|------|---------|
| CPU | 2 核 | 4 核 | 6 节点峰值叠加约 21% 单核，2 核有 10× 余量。但 camera 每 200ms 的 900KB 序列化 + CDR 拷贝是单线程 burst，4 核更平滑 |
| 内存 | 800 MB | 1.5 GB | rclcpp 进程 baseline ~60MB/个 (6×60=360MB)；Fast-DDS 预分配 socket buffer ~128MB；Docker daemon ~300MB。camera 的 900KB 帧缓存每 200ms 复用，不额外占驻留内存 |
| 磁盘 | 3 GB | 5 GB | ROS2 Jazzy base image ~1.2GB；colcon build 产物 ~300MB；Docker image layers ~500MB |
| 网络 | — | — | 全 localhost loopback，不走物理网卡，无带宽成本 |

### 1.5 Docker Compose 资源分配

```yaml
# cpus = CPU 核心数上限, mem_limit = 物理内存上限 (含 swap 禁用)
# camera 内存上限含 900KB × 3 帧缓冲 (publisher queue + subscriber queue + CDR buffer)

lidar_container:
  cpus: 0.3, mem_limit: 120M     # 2.9KB 帧 + 360 元素数组

imu_container:
  cpus: 0.2, mem_limit: 100M     # 76B 帧，极轻量

camera_container:
  cpus: 0.8, mem_limit: 256M     # 900KB 帧 × 3 缓冲 ≈ 2.7MB + 随机数状态

fusion_container:
  cpus: 0.4, mem_limit: 160M     # 缓存 3 路传感器最新帧 (900KB + 2.9KB + 76B)

decision_container:
  cpus: 0.2, mem_limit: 100M     # 事件驱动，无定时器

motor_ctrl_container:
  cpus: 0.3, mem_limit: 100M     # 浮点插值 + Action Server 状态
─────────────────────────────────────────
合计 (hard limit):  cpus: 2.2,  mem: 836 MB
合计 (soft usage):  cpus: ~0.5, mem: ~400 MB  (实际常态负载)
```

`cpus` 设置上限而非预留 — 6 个容器共享宿主机 CPU，软限制下实际常态总负载 <0.5 核。burst 时 camera 可短暂用满 0.8 核，其余容器不受影响。

---

## 2. 开发时间线

### 2.1 假设

- 每 session = 2-3 小时（工作日晚上或周末）
- 每周可投入 2-3 sessions
- 代码由你手写，我 review

### 2.2 逐 Phase 估算

| Phase | 内容 | 估算 | 风险 buffer |
|-------|------|------|------------|
| 5 | 环境搭建 + CI | 1 session | 首次配置 Docker 可能踩环境坑，+1 |
| 6.1 | 传感器层 3 节点 | 2 sessions | lidar/imu/camera 结构相似，第二个起加速 |
| 6.2 | 融合层 | 1 session | 多传感器缓存逻辑 |
| 6.3 | 决策层 | 1 session | Action Client，首次接触 rclcpp_action |
| 6.4 | 执行层 | 1 session | Action Server + Service，含取消处理 |
| 6.5 | RMW 横向对比 | 1 session | Fast-DDS vs Cyclone DDS 性能基准 |
| 6.6 | 实时性扩展 | 1 session | ros2_realtime 确定性通信实验 |
| 7 | 可观测性 | 1 session | 结构化日志 + rqt 监控 |
| 8 | 集成测试 | 2 sessions | 端到端测试 + gtest 覆盖率 |
| 9 | Docker 交付 | 1 session | Docker Compose + 健康检查 |
| 10 | 文档交付 | 1 session | Design Doc 终稿 + CHANGELOG + Release |

### 2.3 里程碑

```
Week 1 (7/16-)  Phase 5:  Docker 环境 + CI 绿钩
Week 2          Phase 6.1: 传感器层 3 节点 + 单测
Week 3          Phase 6.2-6.3: 融合层 + 决策层
Week 4          Phase 6.4-6.6: 执行层 + RMW 对比 + 实时扩展
Week 5          Phase 7-8:   可观测性 + 集成测试
Week 6          Phase 9-10:  Docker 交付 + 文档 + Release Tag
```

| 里程碑 | 可验证产出 |
|--------|-----------|
| Week 2 末 | `./test/test.sh` 跑通传感器层 3 个测试 |
| Week 4 末 | 全链路 6 节点在 Docker 中通信正常 |
| Week 6 末 | `git clone → setup_deps.sh → test.sh` 三步走通，CI 全绿 |

### 2.4 成本总结

| 维度 | 数值 |
|------|------|
| 开发环境 | 笔记本 Ubuntu 24.04 (WSL2)，4 核 8GB+ |
| 部署环境 | Docker (localhost)，2 核 2GB 可跑 |
| 总开发时间 | 12-14 sessions，约 6 周 |
| 金钱成本 | 0（全部开源/免费） |
