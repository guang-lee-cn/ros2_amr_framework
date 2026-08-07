# 架构总览（工作文档，持续更新）

> 用途：**后续所有架构梳理流程的分析都追加到本文档**，标记日期保留历史。
> 图表规范：`/home/guang/code/prompt/references/diagram-spec.md` §6（Mermaid 渲染规避规则）。
> 日期：2026-08-07 建立 / 2026-08-07 按"边界 + 两种视角 + 输入输出表 + 故障定位"重构

---

## 1. 小车边界（谁是小车，谁不是）

```mermaid
flowchart TB
    subgraph CAR["小车整车边界"]
        CONT["compute_container 自动驾驶大脑（自研）<br>fusion 感知 / decision 决策 / motor 执行"]
        LOC["定位与建图（开源 NAV2）<br>AMCL / map_server / slam_toolbox"]
        PHYS["传感器与执行器（真车）<br>LiDAR / IMU / 差速电机"]
        CONT --> PHYS
        PHYS --> CONT
        LOC --> CONT
    end
    ENV["环境<br>货架 / 障碍 / 道路 / 目标点"]
    PHYS --> ENV
    ENV --> PHYS
    OPS["用户 / 调度系统"]
    OPS -->|"任务目标"| CONT
```

- **小车 = 物理体（传感器/执行器）+ 自动驾驶大脑（compute_container）+ 车载定位（AMCL）**
- **compute_container（自研）**：融合感知 + 决策 + 执行，单进程（intra-process 零拷贝）——小车的"大脑"
- **AMCL / map_server / slam_toolbox（开源）**：独立进程，不在计算容器内——小车的"地图/定位外设"
- **仿真器（当前）/ 真车硬件（生产）**：替代 `PHYS`，为大脑提供传感器数据、接收执行指令

## 2. 两种视角

### 2.1 用户视角（使用产品的人）

用户看到的是**两个黑盒**：环境（货架/障碍/道路）和**小车（整车）**。用户不关心内部，只关心**行为是否符合预期**：

```mermaid
flowchart LR
    ENV["环境黑盒<br>货架 / 障碍 / 平坦路"]
    CAR["小车黑盒<br>自主导航"]
    ENV --> CAR
    CAR -->|"遇障停 → 绕行 → 到达目标"| EXP["预期行为<br>取货 → 前往最新规划点"]
```

| 用户看到的情况 | 预期行为 |
|---|---|
| 前方有障碍 | 减速 / 停车 / 绕行，不撞 |
| 平坦路 | 持续前进 |
| 到达目标点 | 停车，等待下一个任务 |
| 任务下发 | 规划新路线前往 |

### 2.2 开发者视角（调试/排障的人）

开发者看到的**环境黑盒 = 仿真器**。关心：仿真器代替什么真实设备、输出哪些数据、数据流到哪、计算容器内有哪些模块及其输入输出。

```mermaid
flowchart LR
    SIM["仿真器黑盒<br>代替：真车传感器 + 执行器<br>输入 /cmd_vel<br>输出 /scan /odom /tf /clock"]
    CONT["compute_container（自研大脑）<br>fusion / decision / motor"]
    LOC["开源组件<br>AMCL / map_server"]
    SIM --> CONT
    CONT --> SIM
    LOC --> CONT
```

## 3. 全系统输入输出表（一张表看全）

| 模块 | 所属 | 输入 | 输出 | 业务含义 |
|---|---|---|---|---|
| fusion_node | 容器内·自研 | `/scan` | `/perception/objects` | 感知：识别障碍物列表 |
| decision_node | 容器内·自研 | `/perception/objects`、`/amcl_pose`、`goal_x/goal_y` | `/cmd/move_to_pose`、`/planning/path` | 决策：绕障规划、派发目标 |
| motor_ctrl_node | 容器内·自研 | `/cmd/move_to_pose`、`/odom`、`/scan` | `/cmd_vel` | 执行：算速度指令（跟踪+绕行+护栏） |
| AMCL | 容器外·开源 NAV2 | `/scan`、`/odom`、`/map` | `/amcl_pose`、`map→odom` TF | 定位：车在地图中的位置 |
| map_server | 容器外·开源 NAV2 | 地图文件 | `/map` | 地图加载 |
| slam_toolbox | 容器外·开源 | `/scan`、`/odom`、`/tf` | 地图（建图） | 建图 |
| Gazebo 仿真 | 容器外·开源 | `/cmd_vel` | `/scan`、`/odom`、`/tf`、`/clock` | 代替真车硬件，物理仿真 |

**关键语义**：
- **`/scan` 一个数据、两个消费者**：fusion（障碍感知）+ motor（护栏/VFH 安全），语义都是"看环境"
- **`/odom` 一个数据、两个消费者**：AMCL（运动模型定位）+ motor（执行闭环），语义都是"看自身"
- **`/clock` 隐式时间源**：全节点 `use_sim_time` 共享
- **决策不消费 bridge 原始数据**：只拿感知的产物（障碍列表）和定位的产物（amcl_pose + TF）

## 4. 坐标系与数据流（定位故障用）

```mermaid
flowchart LR
    GZ["Gazebo 仿真"]
    BR["ros_gz_bridge"]
    FUS["感知 fusion"]
    DEC["决策 decision"]
    MOT["执行 motor"]
    POS["定位 AMCL"]
    GZ -->|"/scan"| BR
    GZ -->|"/odom"| BR
    GZ -->|"/tf"| BR
    BR -->|"/scan"| FUS
    BR -->|"/odom /tf"| POS
    POS -->|"map-odom TF"| DEC
    FUS -->|"障碍列表"| DEC
    DEC -->|"move_to_pose"| MOT
    MOT -->|"/cmd_vel"| BR
    BR -->|"/cmd_vel 控制"| GZ
```

- 坐标系：`map = world + origin(8.162, 9.852)`；decision 在 map 帧规划，派发时经 TF 转 odom 帧给 motor（坐标系 bug 修复点）

## 5. 故障定位流程（出问题怎么办）

**现象 → 数据流检查 → 定位模块 → 判定自研/开源 → 修复**

```mermaid
flowchart TB
    P["故障现象<br>车不动 / 乱走 / 撞障 / 定位跳"]
    D["查数据流（ros2 topic echo）<br>/scan /perception/objects /amcl_pose /cmd_vel"]
    M["定位到模块"]
    S["自研：代码在 ros2_robot_middleware<br>→ 查该模块源码 + 单测"]
    O["开源：NAV2 / gz / bridge<br>→ 查配置 + 上游 issue"]
    F["修复 + 单元测试 + 仿真验证"]
    P --> D --> M --> S
    M --> O
    S --> F
    O --> F
```

| 现象 | 先查 | 定位模块 | 自研/开源 | 常见原因 |
|---|---|---|---|---|
| 车不动 | `/cmd_vel` 有无速度 | motor → decision | 自研 | 无目标派发 / 护栏停 / 速度 0 |
| 车乱走出界 | `/amcl_pose` 是否可信 | AMCL → decision | 开源+自研 | 定位漂移 / 地图错 / 坐标系 |
| 车撞障碍 | 撞前 `/cmd_vel` 是否降速 | 护栏/VFH → motor | 自研 | 护栏未触发 / scan 脏（如车体回波） |
| 感知不到障碍 | `/perception/objects` 有无 | fusion | 自研 | `/scan` 断 / 聚类参数 |
| 定位跳变 | `/amcl_pose` 连续性 | AMCL | 开源 | 地图稀疏 / scan 噪声 |
| 仿真器异常 | gz 日志 | 仿真 | 开源 | 物理 / 模型 / gz 服务 |

## 6. 完成度 / 开源 / 闭源

| 模块 | 来源 | 版本 | 完成度 | 商用程度 |
|---|---|---|---|---|
| 感知融合 fusion | 自研 | - | 高（测试绿） | 需数据评测 |
| A* 决策 decision | 自研 | - | 高（G1 全链验证） | 基础可用，需任务集成 |
| PurePursuit 执行 | 自研 | - | 高（闭环） | 需调优 |
| 碰撞护栏 | 自研 | - | 完成（11 GWT） | 安全底线，可用 |
| VFH 绕行 | 自研 | - | 完成（8 GWT） | 待仿真验证 |
| 定位/建图 | 开源 | NAV2 (jazzy) | 集成 | 地图精度受限 |
| 仿真 | 开源 | Gazebo Harmonic 8.14 | 集成 | 开发/评测用 |
| 可观测 | 自研+开源 | Prometheus/Grafana | 高 | 可用 |

## 7. 环境问题 vs 产品问题

**产品问题（已解决/需解决）**：
- ✅ 坐标系错位（decision map goal vs motor odom）— 已修，AMCL 精度 0.15m
- ✅ 护栏数据竞争（跨线程 scan）— 已修
- ✅ decision 死循环（AMCL 漂移）— 随坐标系修复
- ⚠️ VFH 仿真验证（当前卡点）

**环境问题（仿真，非产品缺陷）**：
- ❌ 车 spawn 后异常滑动 0.94m（无 cmd_vel 却移动）
- ❌ LiDAR 检测车体自身（最小 world 证实 0.68m 车体弧线；warehouse 里 0.2m）
- ❌ gz service 大面积不可用（查询超时）

---

## 附录：后续架构梳理流程记录

<!-- 每次架构梳理追加一节，格式：
### YYYY-MM-DD <阶段> — <主题>
（L1/L2/L3/L4 图 + 模块分析 + 完成度/开源/闭源 + 环境vs产品）
-->
