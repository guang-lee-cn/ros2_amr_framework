# 商用就绪度差距清单（Commercial Readiness Gap）

> 日期：2026-08-06
> 依据：商用 AMR 经典分层范式（MiR/OTTO/Nav2 一致）+ 本项目现状审计
> 结论：单机车载层基础已通（任务点导航 + 感知避障 + odom 闭环），距商用地基仍有 7 项差距

---

## 一、商用经典架构（对照基准）

```
WMS/MES/ERP（任务来源）
  ↓ 任务
Fleet Manager（调度：任务分配、路径预留、交通管制、充电、死锁检测）
  ↓ mission（目标点）
车载层
  ├─ 建图/定位：SLAM 建图 → 静态地图 + AMCL/robot_localization
  ├─ 全局规划：A*/Dijkstra on global costmap（static + inflation）
  ├─ 局部避障：DWA/TEB/MPPI on local costmap（毫秒级）
  ├─ 速度执行：velocity smoother + collision monitor
  ├─ 底盘控制：独立进程 + 编码器反馈
  └─ 恢复行为：卡死 → 后退 → 重规划 → 绕行
```

## 二、差距清单与优先级

| # | 差距 | 性质 | 优先级 | 补强方式 | 工作量 |
|---|------|------|:---:|---------|:---:|
| G1 | **静态地图 + 全局定位**：当前无 SLAM 建图、无 AMCL，A* 起点用 /odom 相对位姿，无 map 坐标系 | 架构地基 | **P0** | slam_toolbox 建图 → 保存 map → AMCL/robot_localization 定位 | 1-2d |
| G2 | **局部避障毫秒级**：当前全局 A* 重规划（秒级），动态障碍（人/叉车）响应不够 | 安全 | **P0** | NAV2 DWB/MPPI 组件嵌入 motor（方案 C），或自研简版 DWA | 2-3d |
| G3 | **导航数据基线**：无成功率/平均速度/避障失败率数据，验收无据 | 验证 | **P1** | 仿真场景脚本采集导航指标基线 | 1d |
| G4 | **恢复行为**：卡死/误差过大只会停止，无后退-重规划-绕行 | 算法 | P1 | 行为状态机（卡死检测 → 恢复） | 1d |
| G5 | **任务/调度/交通管制**：fleet_manager 是雏形，无路径预留、死锁检测 | 架构 | P1 | fleet_manager 扩展（对齐 Open-RMF 思想） | 多周 |
| G6 | **标准接口**：无 VDA 5050/MQTT/OPC UA，无法对接 MES/客户系统 | 接口 | P2 | 适配层（Transport Order API） | 2-3d |
| G7 | **控制隔离 + RT + 功能安全**：控制层在 compute_container 内、非实时、无激光安全区/ISO 13849 | 部署/认证 | P2 | 控制独立进程 + PREEMPT_RT + 安全硬件 | 大 |

## 三、落地顺序建议

**P0（本迭代，地基优先）**：
1. **G1 静态地图 + 全局定位** —— 无地图，导航就是"相对运动"而非"已知地图导航"，商用地基必须先立
2. **G2 局部避障** —— 安全红线，人机混行必须毫秒级

**P1（下迭代）**：G3 数据基线（量化验收）→ G4 恢复行为 → G5 调度

**P2**：G6 标准接口 → G7 安全认证

## 四、与方案 C（自研 + 开源组件借力）的关系

- **自研保留**：决策任务语义、控制闭环、observability、调度层
- **开源借力**：G1 用 slam_toolbox/AMCL（已验证）、G2 用 NAV2 DWB/MPPI 组件（已验证算法）
- **架构不动**：compute_container、生命周期、motor 控制链

## 五、已修复的链路（本次迭代）

| 修复 | 提交 | 效果 |
|------|------|------|
| motor 发布 /cmd_vel | 7476a9e | 车能物理动（闭环最后一跳） |
| decision 任务点导航 + 感知避障 | ebe7c95 | 车到任务点停，不追墙 |
| motor odom 回调饿死（callback group） | ebe7c95 | 真实位姿闭环，不冲出目标 |
