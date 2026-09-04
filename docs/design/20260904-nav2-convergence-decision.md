# 决策记录：导航栈收敛到 NAV2（规划层替换 + 安全域保留）

> 日期：2026-09-04 · 状态：已决策、仿真域已落地、真机接线为 roadmap
> 前置证据链：A/B 对照（本仓库 launch/ab_custom.launch.py 跑台）·
> L3 安全闸验证（nav2_guarded）· 定位形态验证（nav2_localized）
> 关联审计：20260830/20260901 三轮审计（穿货架 bug 家族的世界模型结论）

---

## 1. 决策

**导航的规划职责整体移交 NAV2；自研栈保留安全域，以独立安全闸形态接入。**

```
感知:  雷达/场景仿真 → /scan_raw + /odom + TF
大脑:  NAV2（map_server+AMCL 定位 / Smac 规划 / DWB 控制 / 行为树恢复）
安全:  cmd_vel_guard_node（域 CollisionGuard：减速→硬停 / 全盲 fail-safe）
执行:  /cmd_vel → 底盘
```

## 2. 理由（证据）

### 2.1 A/B 对照（rack_3c 四站巡回，同场景同目标同判定）

| | NAV2 | 自研栈（fusion→decision→motor） |
|---|---|---|
| 到达率 | **4/4** | **0/4** |
| 总耗时 | 48.8s | 287.9s（三站超时） |
| 里程效率 | 96%（51.1m/53m 直线） | 未离开起点口袋 |
| 死因 | — | 货架角冻结重规划 128 次、guard 拦停 29 次死循环 |

### 2.2 世界模型结论（三轮审计 + 会话实证）

自研栈穿货架/卡死是**扁平世界模型的系统性缺陷**，四层叠加：
感知 fail-open（没扫到=自由）、栅格 raytrace 清除吃掉标记、规划无机身
概念（无 inscribed 语义）、执行无闭环校验。逐点修复（sticky obstacle/
静态注入/快照锁）每修一层另一层换姿态复发——bug1→bug2 的结构性根源。

NAV2 分层代价地图（LETHAL/INSCRIBED/inflated 三级语义 + unknown≠free）
把这四层作为一个整体解决。同场景压力测试（6 随机箱+2 移动障碍+1.5m/s）
4/4 SUCCEEDED、零规划失败（换 SmacPlanner2D 后）。

### 2.3 安全域是自研栈不可替代的增量

三轮审计打磨的 fail-safe 链（CollisionGuard 22 测试矩阵、全盲硬停、
stale 超时、StampGate）是 NAV2 没有的。L3 验证：安全闸串接后 4/4
到达、51.1s（+2.3s 减速代价）、8 次干预全部温和减速零误拦。

## 3. 处置清单

### 保留并激活（生产链路组成部分）
- `cmd_vel_guard_node`：NAV2 输出安全闸（域 CollisionGuard 全复用）
- supervisor / health_monitor / OTA / fleet：进程监管与运维链不动
- 场景仿真器（SimulatedScene）：CI 级仿真底座 + 随机/移动障碍测试能力

### 保留但退居二线（不删）
- fusion/decision/motor_ctrl 计算管线：A/B 基线跑台（ab_custom.launch）、
  22+ 域测、e2e 测试的载体。**不再向其新增导航功能**；域算法（KF/
  CollisionGuard/StampGate）继续按测试资产维护
- VFH/A*/PurePursuit：同上，作为教学/对照实现

### 退役路径（roadmap）
- system.launch 真机形态重写为 NAV2 接线（sick_tim781 话题→/scan_raw、
  真机 URDF TF、预建地图）——需上硬件验证，本决策不宣称完成
- compose/systemd 部署面随之切换（现指向 compute_container 的部分）

## 4. Launch 形态谱系（当前）

| Launch | 形态 | 验证状态 |
|---|---|---|
| nav2_localized | **生产**：预建地图+AMCL+Smac+DWB+安全闸 | 6/6，AMCL 波动 68mm |
| nav2_scene | 开发：在线 SLAM 建图（部署期） | 4/4，长时稳定 |
| nav2_guarded | L3 演示（SLAM+闸） | 4/4 |
| ab_custom | A/B 基线：自研栈 | 0/4（基线数据源） |
| nav2_demo | Gazebo（gz-sim 雷达渲染死亡，受限） | 环境问题已归档 |

## 5. 风险与开放项

1. **安全闸单点**：guard 挂掉 = 无 /cmd_vel 输出（fail-safe 方向正确）。
   **已关闭（launch 级，2026-09-04）**：三份 nav2 launch 为 guard 与
   scene_simulator 配 respawn=True（崩溃 2s 自动拉起）。supervisor 级
   预算管理（retry→FATAL 语义）随 system.launch NAV2 接线一并做（roadmap）
2. **真机未验**：sick_tim781 真实点云特征（密度/噪声/遮挡）与场景仿真
   差异，AMCL/代价地图参数需上机重调——参数文件已按场景隔离便于替换
3. **ISO 3691-4**：软件安全闸不替代双通道安全回路（审计 P0-H 维持原判）

## 6. 验证门

- A/B 跑台可复现（同种子同目标）：`ros2 launch ... ab_custom` vs NAV2
- 定位模式：6/6 + AMCL RMS<100mm（实测 68mm）
- 安全闸：带闸导航 4/4 + 干预日志非空且零硬停误拦
- CI 四门禁全绿（提交 9e4c81f 实测）
