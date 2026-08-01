# 子系统设计 · sim_bridge

> L3/L4 + ADR + GWT，strategy.md §6.5 子系统清单中 sim_bridge 的独立设计。
> 契约：[contracts/bridge-contract.md](../../contracts/bridge-contract.md)

## 定位

可信可视化仿真底座：场景 + 车模型 + gz↔ros2 桥接，产出标准话题契约，供感知→决策→执行管线验证。

## 组件（L4）

| 组件 | 说明 | 状态 |
|------|------|------|
| `worlds/amr.sdf` | 车模型：ray lidar（CPU）+ 点云 + IMU + Camera + DiffDrive | 本期改 gpu_lidar→ray |
| `worlds/warehouse.sdf` | 最小场景 | 保留 |
| `launch/simulation.launch.py` | 启动编排：gz + 桥接 + compute_container + health_monitor | 本期加 /cmd_vel、/points 桥 |
| 桥接契约 | contracts/bridge-contract.md | 本期更新 /scan 目标 |

## ADR

| # | 决策 | 理由 | 代价 |
|---|------|------|------|
| ADR-1 | **保持 `gpu_lidar`**（gz-sim Harmonic 仅支持该类型） | 曾误判 gpu_lidar 不产数据而改 ray，实测 **`ray` 类型不被支持**（SdfEntityCreator 报错）；真正根因是 world 缺 Sensors 系统插件 | 需 GPU 渲染支撑，WSL2 用 d3d12 硬件路径已打通 |
| ADR-2 | 桥接直喂 `/scan`（fusion 经 `sensors.lidar.topic` 参数指向） | 符合契约，零 fusion 代码改动；SICK 适配器按参数读话题 | 真实模式仍用 /sensor/lidar，模式间话题名不一致（I4 归一） |
| ADR-3 | 最小 world 优先 | 先单场景跑通再叠加 | 场景扩展留 I5 |
| ADR-4 | **相机禁用**（amr.sdf 注释保留配置） | WSL2 d3d12 下相机 640x480 图像读回（Ogre2RenderTarget::Copy）触发 gz SIGSEGV，实测移除相机后 gz 稳定 | 视觉暂靠 /scan + /points；恢复需换渲染后端（待评估 ogre v1 / 其他 GPU 路径） |
| ADR-5 | **IMU 用 simulated 兜底** | gz IMU 传感器在此环境不产数据（已最小化配置仍无效，疑似 gz-sensors IMU 兼容问题）；fusion 本就配置 simulated IMU | gz /imu 对 robot_localization 不可用，EKF 靠 odom 降级 |

## GWT 清单

| 场景 | Given | When | Then |
|------|-------|------|------|
| G1 | 仿真启动 | 3s 后 | `/scan` 以 ~10Hz 发布 LaserScan |
| G2 | 仿真启动 | 3s 后 | `/points` 发布 PointCloud2 |
| G3 | 仿真运行 | 发布 `/cmd_vel`(x=0.2) | odom 变化、车在 Gazebo 移动 |
| G4 | 仿真启动 | 3s 后 | `/imu` 与 `/image` 发布 |
| G5 | d3d12 渲染 | gz 启动 | 20s 内不卡死 |

**验证结果（2026-08-02）**：G1/G2/G3/G5 ✅；G4 ⚠️ 受限（imu 见 ADR-5，image 见 ADR-4）。

测试命名（集成级）：
```
TEST_F(SimBridgeIT, Given_Launch_Then_ScanPublishesAt10Hz)
TEST_F(SimBridgeIT, Given_Launch_Then_PointCloudPublishes)
TEST_F(SimBridgeIT, Given_CmdVel_Then_RobotMoves)
TEST_F(SimBridgeIT, Given_Launch_Then_ImuCameraPublish)
TEST_F(SimBridgeIT, Given_D3D12Render_Then_NoFreeze)
```

## 已校准（实测）

- gz 传感器数据话题是**根话题**（`/lidar`、`/lidar/points`、`/imu`、`/camera`），**非帧路径**（帧路径话题为空）。桥接必须用根话题。
- `/cmd_vel` 桥为 ros→gz 单向（`]` 操作符），无 remap。
- 桥接 `sensors.lidar.topic` 指向 `/scan`（fusion 经 SICK 适配器读话题）。

## 已知限制（ADR-4/ADR-5）

- 相机禁用（渲染读回崩溃），恢复需换渲染后端。
- gz IMU 不产数据，fusion 用 simulated IMU 兜底。
