# AMR 商业部署方案

> 日期：2026-07-31
> 定位：从原型（Ubuntu + scp）到商业可部署（Docker + OTA + 版本锁定）
> 参考：[benchmark-lessons-learned.md](benchmark-lessons-learned.md) · [dds-selection-guide.md](dds-selection-guide.md)

---

## 一、部署架构

```
┌──────────────────────────────────────────────────┐
│  AMR 工控机（Ubuntu Server 24.04 LTS）            │
│                                                  │
│  ┌────────────────────────────────────────────┐  │
│  │  Docker container: amr_compute            │  │
│  │  ┌──────────────────────────────────────┐ │  │
│  │  │  compute_container                    │ │  │
│  │  │  fusion + decision + motor + ekf     │ │  │
│  │  │  deps: ros-jazzy-ros-base (locked)   │ │  │
│  │  └──────────────────────────────────────┘ │  │
│  │                                            │  │
│  │  Docker container: amr_drivers             │  │
│  │  ┌──────────────────────────────────────┐ │  │
│  │  │  lidar_node / imu_node / camera_node  │ │  │
│  │  │  + 底盘适配器 (IActuator)            │ │  │
│  │  │  deps: ros-jazzy-ros-base + CAN       │ │  │
│  │  └──────────────────────────────────────┘ │  │
│  └────────────────────────────────────────────┘  │
│                                                  │
│  主机层：systemd 管理容器 + 自启                  │
│  只读 rootfs + AppArmor                          │
└──────────────────────────────────────────────────┘
```

**为什么拆两个容器**：compute（业务）和 drivers（硬件）故障隔离——driver 崩了不拖累业务，与现有进程隔离设计一致。

## 二、版本锁定策略

### 2.1 依赖锁定

| 层 | 锁定方式 | 防什么 |
|------|---------|--------|
| **ROS2 发行版** | 容器内 `ros-jazzy-ros-base` 固定版本号 | `apt upgrade` 升级到新版 ROS2 |
| **DDS** | Fast-DDS 2.14.x / CycloneDDS 0.10.x 固定 | DDS 行为变化 |
| **应用代码** | Git tag + Docker image tag | 代码漂移 |
| **底层 OS** | Ubuntu 24.04 LTS + `unattended-upgrades` 只装安全补丁 | 内核/库破坏性升级 |

### 2.2 版本号规范

```
应用版本:  v1.2.3 (语义化版本)
  MAJOR: 破坏性接口变更
  MINOR: 新功能（向后兼容）
  PATCH: bug 修复

Docker tag:  amr-compute:v1.2.3
              amr-drivers:v1.2.3
```

### 2.3 依赖版本记录

```yaml
# versions.yaml — 记录每个部署版本锁定的依赖
ros_distro: jazzy
ros_base_version: 0.13.0-1noble
fastdds_version: 2.14.6
cyclonedds_version: 0.10.5
app_git_sha: 5fe2e5b
```

## 三、OTA 升级方案

### 3.1 双分区 A/B 切换

```
┌───────────────────────────────────────┐
│  分区 A: 当前版本 v1.2.3 (active)      │
│  分区 B: 待升级 v1.3.0 (standby)       │
└───────────────────────────────────────┘

升级流程:
  1. 后台下载新镜像到分区 B
  2. 校验 SHA-256 + 签名
  3. 切换到分区 B → 重启容器
  4. 健康检查通过 → 标记 B 为 active
  5. 失败 → 自动切回分区 A（回滚）
```

### 3.2 OTA 通道

| 阶段 | 方式 | 说明 |
|------|------|------|
| 开发 | 本地 `docker build` + `docker load` | 快速迭代 |
| 测试 | 私有 registry + `docker pull` | 内网 |
| 规模化 | OTA manager（如 Mender / RAUC） | 云端分发 + 批量升级 |

### 3.3 升级安全

- **签名验证**：镜像签名（cosign / Docker Content Trust）
- **失败回滚**：双分区 + 健康检查门禁
- **分批灰度**：先 10% → 观察 → 再 100%

## 四、构建与发布流水线

```yaml
# CI（GitHub Actions）
on:
  push:
    tags: ['v*']

jobs:
  build-image:
    runs-on: ubuntu-24.04
    steps:
      - build Docker image (amr-compute)
      - run tests in container
      - push to registry (ghcr.io/guang-lee-cn/amr-compute:v1.2.3)
      - sign image
      - create OTA manifest
```

**构建即部署**：每个版本 tag 产生不可变镜像 + 签名 + OTA manifest，可复现、可回滚。

## 五、系统管理

### 5.1 systemd 服务

```ini
# /etc/systemd/system/amr.service
[Unit]
Description=AMR compute container
After=docker.service

[Service]
ExecStartPre=/usr/bin/docker pull ghcr.io/guang-lee-cn/amr-compute:v1.2.3
ExecStart=/usr/bin/docker run --rm \
  --network host \
  --name amr-compute \
  ghcr.io/guang-lee-cn/amr-compute:v1.2.3
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
```

**关键配置**：
- `--network host`：DDS multicast 跨容器必需，避免 docker bridge 阻断
- `Restart=always`：进程崩溃自恢复
- `--rm`：停止即清理

### 5.2 监控与告警

| 指标 | 来源 | 告警 |
|------|------|------|
| 进程存活 | systemd + Docker healthcheck | 重启次数 > N |
| 传感器速率 | :9090/metrics | 速率 < 标称 50% |
| 降级等级 | :9090/metrics | > 0 持续 10s |
| 控制延迟 | :9091/metrics | p99 > 阈值 |
| 资源使用 | cAdvisor / nvidia-smi | CPU/内存 > 80% |

### 5.3 远程管理

```
安全基线:
  - VPN / Tailscale 远程访问（非公网暴露）
  - SSH key-only，禁用密码
  - AppArmor 限制容器权限
  - 加密分区（LUKS）
```

## 六、当前到商业的分阶段路线

```
阶段 1（当前）: 原型
  Ubuntu + colcon build + ros2 launch
  └── 验证算法和管线正确

阶段 2（近期）: Docker 化
  ├── Dockerfile（构建 compute + drivers 镜像）
  ├── docker-compose（本机编排）
  └── systemd 自启 + --network host
  └── 验证 DDS 在容器内工作（multicast）

阶段 3（规模化）: OTA + 版本管理
  ├── 私有 registry + tag 规范
  ├── 双分区 A/B + 健康检查回滚
  └── 监控告警（Prometheus + Grafana 已有）

阶段 4（量产）: 安全加固
  ├── 镜像签名 + AppArmor
  ├── LUKS 加密 + 安全启动
  └── 远程管理（VPN）
```

## 七、参考

- [ROS2 Docker 官方镜像](https://hub.docker.com/_/ros)
- [DDS multicast 在 Docker](https://docs.ros.org/en/jazzy/How-To-Guides/Run-2-nodes-in-separate-docker-containers.html)
- [Mender OTA](https://mender.io/)
- [RAUC](https://rauc.io/)
- [Docker Content Trust](https://docs.docker.com/engine/security/trust/)
