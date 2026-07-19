# 部署与运维架构

## 目标硬件环境

| 环境 | 平台 | 用途 |
|------|------|------|
| 开发 | WSL2 Ubuntu 24.04, x86_64 | 编码 + 单元测试 + Gazebo 仿真 |
| CI | GitHub Actions, `ubuntu-24.04` + `ros:jazzy-ros-base` 容器 | 自动构建 + 测试 + 覆盖率门禁 |
| 部署 | RK3588 ARM64, Ubuntu 24.04 | 真实 AMR 车载计算平台 |
| 仿真 | Gazebo Harmonic + ros_gz_bridge | 传感器+物理仿真，无头模式 |

## 容器部署

### 镜像分层（multi-stage build）

```
Stage 1: builder    — gcc/cmake/lcov, 编译 ros2_robot_middleware
Stage 2: runtime    — ros-core only, 可执行文件 + 配置文件
Stage 3: dev        — 含 rqt/调试工具, 本地开发用
```

### docker-compose 服务

```yaml
services:
  lidar:       {image: amr-runtime, command: lidar_node}
  imu:         {image: amr-runtime, command: imu_node}
  camera:      {image: amr-runtime, command: camera_node}
  compute:     {image: amr-runtime, command: compute_container, ipc: host}
  health:      {image: amr-runtime, command: health_monitor_node}
  fleet:       {image: amr-runtime, command: fleet_manager_node}
  prometheus:  {image: prom/prometheus, ports: [9090:9090]}
  grafana:     {image: grafana/grafana, ports: [3000:3000]}
```

`compute` 服务使用 `ipc: host` 以支持 SHM 传输。

## 启动流程

```
系统上电 → containerd 启动 → docker-compose up
  ├── 1. clean_shm: rm -f /dev/shm/amr_metrics_registry
  ├── 2. sensors: lidar → imu → camera (并行 activate)
  ├── 3. compute: compute_container (fusion + decision + motor_ctrl)
  ├── 4. health: health_monitor (最后启动，开始监控)
  └── 5. fleet: fleet_manager (可选，多 AMR 模式)
```

## 运维操作

### 日志查看

```bash
# 实时日志 (Docker logging driver)
docker logs -f amr-compute

# 结构化日志过滤
docker logs amr-compute 2>&1 | grep '"mod":"fusion"' | jq .

# ROS2 日志
ros2 topic echo /rosout
```

### 指标查询

```bash
# Prometheus 指标
curl http://localhost:9090/metrics

# Grafana Dashboard
# Import config/grafana_dashboard.json → 数据源: Prometheus (http://prometheus:9090)
```

### 健康检查

```bash
ros2 topic echo /health/report    # 集群健康报告
ros2 lifecycle get /fusion        # 节点生命周期状态
```

### 故障定位

```
1. Grafana 面板查看延迟尖刺 → 确定时间窗口
2. spdlog 日志 grep 对应时间段 → 提取 trace_id
3. 根据 trace_id 串联事件 → 定位故障根因
```

## 版本升级

| 方式 | 适用场景 | 步骤 |
|------|---------|------|
| 镜像升级 | 容器化部署 | `docker pull amr-runtime:v2.1` → `docker-compose down && up -d` |
| 包升级 | 非容器化 | `apt install ros-jazzy-ros2-robot-middleware` → `systemctl restart amr` |
| 配置热更新 | 运行时参数 | `ros2 service call /cmd/set_param "{param_name: step_size, value: 0.08}"` |

## CI/CD 流水线

```
Git Push → GitHub Actions
  ├── [1] Build (colcon --cmake-clean-first --coverage)
  ├── [2] Test (colcon test, 65 cases)
  ├── [3] Coverage (lcov, 门禁: <80% 不能降)
  └── [4] (future) Docker Build + Push → ghcr.io
```
