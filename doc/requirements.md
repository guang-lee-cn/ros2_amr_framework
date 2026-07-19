# 业务与需求背景

## 项目定位

工业 AMR 中间件系统，面向仓库物流、智能制造等场景的自主移动机器人平台软件。为上层 SLAM、导航、规划算法提供稳定、高效的感知数据接口与控制接口。

## 核心业务目标

- **交付能力**：提供从传感器采集到控制指令输出的完整中间件管线，可集成到商业 AMR 产品中
- **可移植性**：WSL2（开发）↔ ARM64 RK3588（部署）无缝切换，传感器模拟↔真实硬件通过 YAML 配置
- **可观测性**：内建 Traces/Metrics/Logs 体系，支持 Prometheus + Grafana 生产监控
- **可靠性**：传感器降级策略 + 看门狗恢复 + 进程隔离，保障 7×24 稳定运行

## 功能需求

### Must（已实现）

| 功能 | 说明 |
|------|------|
| 传感器数据采集 | LiDAR/IMU/Camera 三路传感器，模拟 + 真实硬件双模式 |
| 多传感器融合 | DBSCAN 聚类 + EKF 状态估计 + 多目标跨帧跟踪 |
| 决策 | 感知→导航目标分发，支持抢占 + 重试 |
| 执行 | 轨迹插值 + MoveToPose Action Server |
| 降级管理 | 5 级传感器降级（FULL→NO_IMU→NO_LIDAR→CRITICAL→FATAL） |
| 健康监控 | 1Hz 心跳老化检测 + 看门狗重启（最多 3 次） |
| 可观测性 | Traces (TracerContext) + Metrics (Prometheus + SHM) + Logs (spdlog async JSON) |
| DDS QoS 定制 | 按 Topic 差异化 QoS (IMU reliable / Camera best_effort) |
| SROS2 安全 | 每节点 keystore + governance.p7s + permissions.xml |
| Gazebo 仿真 | 仓库环境 + ros_gz_bridge |

### Should（方案完成，待实施）

| 功能 | 说明 |
|------|------|
| TF2 坐标变换 | ITransformProvider 接口已定义，LiDAR→base_link 刚体变换已集成 |
| 真实传感器适配 | SickTiM781Adapter 已完成，IMU/Camera 适配器待开发 |
| Grafana Dashboard | 预置面板 JSON 已完成，待接入真实 Prometheus 数据源 |

### Could（远期）

| 功能 | 说明 |
|------|------|
| ARM64 交叉编译 | 需物理 RK3588 开发板验证 |
| OTA 升级 | 容器化原子更新 + 回滚 |
| 多 AMR 集群 | FleetManager 骨架已有，待扩展调度逻辑 |

## 非功能需求

| 类别 | 指标 | 目标 |
|------|------|:---:|
| 性能 | 端到端延迟 P99 | <15ms |
| 性能 | IMU 回调延迟确定性 | 零 malloc / 零 syscall |
| 可用性 | 进程崩溃恢复时间 | <2s |
| 可观测性 | 日志热路径延迟 | <1μs |
| 安全性 | DDS 通信 | SROS2 加密 + 每节点 ACL |
| 可移植性 | 传感器切换 | 改 YAML 一行，不重新编译 |
| 资源 | 单 AMR 内存占用 | <500MB (6 进程合计) |

## 需求风险与架构应对

| 风险 | 应对 |
|------|------|
| 传感器硬件故障导致融合中断 | 5 级降级策略，LiDAR 缺失时 Tracker 仅 predict |
| 日志 I/O 阻塞实时回调 | spdlog async 线程池，热路径不调 write() |
| DDS 消息丢失 | reliable QoS (IMU/PerceptionObjects)，best_effort (Camera) |
| 单进程崩溃导致全系统挂 | 6 进程隔离，HealthMonitor 独立进程执行重启 |
| 第三方库许可证冲突 | 全部选用 Apache 2.0 / BSD-3 / MIT 许可的组件 |
