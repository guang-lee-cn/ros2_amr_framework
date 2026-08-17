# 集成测试计划 — 仿真单场景闭环

> 阶段：ISO integration（对应 iso-output G3 单场景闭环）
> 日期：2026-08-08
> 模式：已有项目收尾（重构模式产出 G3 验收）
> 上游依据：[iso-output](../../doc/iso-output.md) G3 · [interfaces](../../doc/interfaces.md) · [requirements](../../doc/requirements.md)
> 上游走查：计算容器契约走查 + SceneSimulator 闭环走查（2026-08-08）

---

## 0. 验收基准（ISO G3 成功标准）

车在 SceneSim 巡一圈，**感知→决策→执行全程可观察、可复现**，面试场景第三方可复现。可量化：

- `/odom.x` 从 0 单调增长到 goal(7.0) ±0.2 后收敛
- `/cmd_vel` 到达后归零（车真停）
- `/perception/objects` 稳定 5Hz（200ms tick）
- Foxglove 3D：车(URDF) + 点云 + 路径全程可见，无"缺变换"报错
- ASan 下全程不崩（UB 零容忍，§0 铁律5）

---

## 1. 走查断点清单（闭环数据层已通，以下为已知风险）

**闭环契约核对结论**：topic/类型/QoS/frame_id(主体)/heartbeat 字符串全对齐；TF 树 `map→amr/odom→amr/chassis→amr/chassis/lidar` 完整；`/cmd_vel` unicycle 积分正确车会真动；lifecycle 自动激活。**契约层面能跑通。**

### P0 — 演示阻断 + 安全雷（必修）

| # | 问题 | 证据 file:line | 类别 |
|---|------|---------------|------|
| B1 | scene_demo 无 respawn，scene 是 map→odom 唯一发布者，误杀即整树坍塌 | scene_demo.launch.py:28；scene_simulator_node.cpp:72 | 演示阻断 |
| B2 | fusion TF 帧名硬编码 `lidar_frame`/`base_link`，与 TF 树不符，transform_scan 永失败 + tf2 报错本体 | tf2_transform_provider.hpp:35；perception_service.hpp:57 | 演示阻断 |
| B3 | scan 180°错位：ranges[0] 对车头但 header 声明 angle_min=-π，collision_guard 扫后半球，换场景必撞 | simulated_scene.hpp:63；scene_simulator_node.cpp:53；collision_guard.hpp:130 | 安全雷 |
| B4 | decision `active_goal_`/`retry_count_` 跨 callback group 无锁，UB data race | decision_node.hpp:86；decision_node.cpp:65,231 | UB(铁律5) |
| B5 | motor cancel 分支不发零速，SceneSim 用 cmd_ 缓存积分 → cancel 后滑行 | motor_ctrl_node.cpp:227 vs 250,283 | 停车安全 |

### P1 — 稳定性/可观测（建议修）

| # | 问题 | 证据 |
|---|------|------|
| B6 | decision dispatch 强依赖 amr/odom←map TF，不可用静默 return，/cmd_vel 永不发 | decision_node.cpp:243-260 |
| B7 | fusion_ready 门控冷启动失效（current_level_ 默认 FULL=0，heartbeat 首发即 alive）→ 穿墙防护形同虚设 | fusion_node.hpp:72；fusion_node.cpp:297 |
| B8 | TF 双重权威冲突（SceneSim 动态 + RSP 静态同发 odom→chassis / chassis→lidar） | scene_simulator_node.cpp:77 vs amr_visual.urdf:8 |
| B9 | decision demo_grid_ 在 Reentrant 组无锁，自并发 | decision_node.cpp:44,161 |
| B10 | fusion current_level_ 跨 timer 读写依赖隐式互斥（非 atomic） | fusion_node.hpp:72 |

### P2 — 死代码/契约漂移（清理项）

| # | 问题 | 证据 |
|---|------|------|
| B11 | motor path_sub/on_path/latest_path_ 死代码，A* 全局路径没进控制环 | motor_ctrl_node.cpp:75,210 |
| B12 | Object.msg 注释 base_link 漂移（实际 amr/chassis） | msg/Object.msg:2 |
| B13 | /odom.twist 全零（EKF/状态估计消费者拿到 0 速度） | scene_simulator_node.cpp:34-46 |
| B14 | scan.angle_max 多算一个增量 | scene_simulator_node.cpp:54 |

---

## 2. 测试场景清单

### P0 — 核心闭环（失败 = 不可演示）

| ID | 业务流程 | 涉及 | 输入 | 预期输出 |
|----|---------|------|------|---------|
| IT-01 | 数据源存活 | SceneSim | scene 起来 | /scan /odom /tf Publisher count ≥1；tf2_echo map→amr/chassis 通 |
| IT-02 | 感知出物体 | SceneSim→fusion | lowstep 场景 | /perception/objects 出障碍，含 category="low"（深度补盲） |
| IT-03 | 决策派发 | fusion→decision | heartbeat=alive + amcl_pose(0,0) + goal(7,0) | /planning/path 出；action goal 发出 |
| IT-04 | 执行出速 | decision→motor | goal 到达 motor | /cmd_vel 非 zero |
| IT-05 | 积分移动 | motor→SceneSim | /cmd_vel 持续 | /odom.x 单调增长（车真动） |
| IT-06 | 收敛停车 | 全链 | odom.x→7.0 | /cmd_vel 归零；odom 稳定 |
| IT-07 | 容错 respawn | SceneSim | kill scene | <2s 重启（B1 修后）；TF 恢复 |

### P1 — 异常/降级/安全

| ID | 业务流程 | 输入 | 预期 |
|----|---------|------|------|
| IT-08 | 避障真实生效 | goal(9,0) box 之后 | collision_guard 扫到真前方 → 减速/绕（B3 修后） |
| IT-09 | 取消停车 | cancel move_to_pose | /cmd_vel 归零（B5 修后） |
| IT-10 | 穿墙防护 | kill fusion | decision 不派发（B7 修后真生效） |
| IT-11 | TF 不可用兜底 | 断 amr/odom←map | decision 有 WARN 日志不静默（B6 修后） |
| IT-12 | UB 不崩 | 全程 + ASan | 无 data race 报错（B4 修后） |

### P2 — 契约/性能

| ID | 检查 | 预期 |
|----|------|------|
| IT-13 | heartbeat 频率 | /sensor/fusion/heartbeat 1Hz |
| IT-14 | 感知频率 | /perception/objects 5Hz ±10% |
| IT-15 | 内存 | 单 AMR <500MB |

---

## 3. 测试数据准备

- 场景：fusion `scenario` 参数 `lowstep`（演示深度补盲）/ `obstacle`（单障碍）/ `empty`
- 目标：(7,0) 不撞演示路径；(9,0) box(8,0) 之后验证避障真实生效
- 初始位姿：amcl_pose (0,0,0)，odom (0,0,0)
- 隔离：每场景独立 launch，不复用残留进程

---

## 4. 建议执行路径

走查已暴露 P0 断点，**先修 P0 再跑集成测试**（否则 IT-06/07/08/12 必失败，浪费一轮）。P0 修复属"测试环境就绪"前置（integration.md Step 2）。

```
门禁1：确认本计划 + 修复范围
  ↓
修 P0（B1 respawn / B2 帧名 / B3 scan约定 / B4 加锁 / B5 cancel归零）
  ↓
门禁2：测试环境就绪（干净重启 + TF 通 + ASan 编译）
  ↓
跑 P0 场景 IT-01..07 → 门禁3
  ↓
跑 P1 场景 IT-08..12 → 门禁4（E2E）
  ↓
上线清单 integration-report.md → 门禁5
```
