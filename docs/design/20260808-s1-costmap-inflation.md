# S1 子系统设计 — Costmap 代价场 + 指数 Inflation（NAV2 P0 对标）

> 范围：解决"A* 撞 box + 贴边振荡"两个症状的真根因
> 对标：NAV2 costmap_2d（ObstacleLayer + InflationLayer + SmacPlanner2D cost-aware）
> 路线：两边都留（fusion objects 保留 + decision /scan raytrace 建栅格）
> 上游依据：[NAV2 对标报告](../design/)（agent 2026-08-08）+ [integration-plan](integration-plan.md)

---

## 1. 根因（agent 已代码定位）

| 症状 | 根因 | 代码 |
|------|------|------|
| A* 撞 box | inflate 在质心打 **0.30m 实心圆盘**，box 对角 0.71m > 圆盘 0.60m，**角是空的**；Cluster 只有质心无尺寸 | grid_updater.hpp:46 |
| A* 贴边振荡 | `vector<bool>` **二值**无梯度，A* 步代价纯欧氏，数学上必走最短=贴 inflation 边；`heuristic_weight` 死代码；对角只查目标格（穿角） | astar_planner.hpp:40,148,64,145 |
| 丢帧障碍消失 | decision 每帧 `std::fill(false)` 全清 | decision_node.cpp:161 |

---

## 2. 方案（NAV2 costmap 核心复刻）

### 2.1 OccupancyGrid 代价场（vector<bool> → vector<uint8_t>）

```
0       = FREE
1..252  = inflate 梯度（越近障碍越高）
253     = INSCRIBED（机器人内切圆内，必碰撞）
254     = LETHAL（障碍本体）
255     = NO_INFO（未知）
```

### 2.2 指数 Inflation（NAV2 InflationLayer 公式）

对每个 LETHAL 格，在 `inflation_radius` 内按距离指数衰减：

```
d <= inscribed_radius  →  cost = INSCRIBED (253)
d >  inscribed_radius  →  cost = 253 * exp(-cost_scaling_factor * (d - inscribed_radius))
d >  inflation_radius  →  cost = 0
```

参数（对标 NAV2 默认 + 车体）：
- `inscribed_radius` = 0.22m（0.6×0.4 车内切圆 = 0.2，+裕度）
- `inflation_radius` = 0.55m（inscribed + 0.33 膨胀带）
- `cost_scaling_factor` = 3.0（衰减陡度；越大 A* 越敢贴，越小越保守）

> NAV2 公式来源：[Inflation Layer 配置](https://docs.nav2.org/configuration/packages/costmap-plugins/inflation.html)

### 2.3 A* cost-aware（步代价含 cost + 对角防穿角 + 启用启发）

- 步代价：`step = base(diag/cardinal) + cost(next_cell)/253`（高 cost 格代价高，A* 自动绕开但不必远离）
- is_traversable：`cost < INSCRIBED`（>253 才不通，253 以内高代价但可走）
- 对角防穿角：对角扩展时，两个正交邻居都得 traversable
- `heuristic_weight` 启用：`f = g + heuristic_weight * h`

### 2.4 Cluster 加 w/h（bbox）+ GridUpdater bbox 栅格化

- `Cluster`/`PerceivedObject`/`Object.msg` 加 `float w, h`
- `cluster_detector` 聚类时算 min/max x/y → w/h
- `GridUpdater` 先把 bbox 栅格标 LETHAL，再指数 inflate（替代质心圆盘）→ box 0.5×0.5 角不再空

### 2.5 decision /scan raytrace（两边都留）

- decision 订阅 `/scan`，Bresenham raytrace（传感器原点→每个 hit 射线上的格 free，hit 格 LETHAL）
- 替代"每帧全清"：raytrace 天然 clearing（动态清障）
- objects 保留（tracker + KF 动态预测），叠加到 costmap

---

## 3. ADR

**决策**：选 NAV2 uint8 代价场 + 指数 inflation，弃 vector<bool> 二值。
**理由**：二值网格 A* 数学上必走最短=贴 inflation 边（贴边振荡）；代价场让 A* "绕开但不必远离"（自然居中）。
**代价**：网格内存 ×1（bool→uint8），inflation 计算 O(radius²·obstacles)，400×400@5cm 单障碍 <1ms，5Hz 够。

**决策**：两边都留（/scan raytrace + objects）。
**理由**：/scan raytrace 保形状 + clearing（静态仓库）；objects tracker 保动态预测（动态障碍）。NAV2 纯栅格丢对象级，我们保留优势。

---

## 4. GWT 测试清单（TDD 先行）

### GridUpdater（代价场 + inflation）
- `Given_LethalPoint_WhenInflate_CenterIs254`
- `Given_CellAtInscribed_WhenInflate_Is253`
- `Given_CellOutsideInflation_WhenInflate_Is0`
- `Given_CellBetweenInscribedAndRadius_WhenInflate_ExponentialDecay`（验证 cost=253·exp(-k·(d-insc))）
- `Given_BboxObstacle_WhenInflate_CornersLethal`（0.5×0.5 box 角不空，治撞 box）

### A*（cost-aware + 对角 + 启发）
- `Given_CostGradient_WhenPlan_PrefersLowerCostPath`（绕高 cost）
- `Given_DiagonalWithBlockedCorner_WhenPlan_NoCornerCut`（防穿角）
- `Given_HeuristicWeight2_WhenPlan_AppliesWeight`

### Cluster（bbox）
- `Given_ClusterPoints_WhenDetect_ComputesWidthHeight`

### 集成（/scan raytrace）
- `Given_ScanWithObstacle_WhenRaytrace_MarksHitLethalAndRayFree`

---

## 5. 落地（文件 + 顺序）

| 步 | 改动 | 文件 |
|----|------|------|
| 1 | OccupancyGrid uint8 + cost 常量 | astar_planner.hpp |
| 2 | Inflation 指数衰减 | grid_updater.hpp |
| 3 | A* cost-aware + 对角 + 启发 | astar_planner.hpp |
| 4 | Cluster w/h | icluster_algorithm.hpp, cluster_detector.hpp, msg/Object.msg, target_selector.hpp |
| 5 | GridUpdater bbox 栅格化 | grid_updater.hpp |
| 6 | decision /scan raytrace（ScanToGrid） | decision_node.cpp + 新 scan_to_grid.hpp |

S1 = 步 1-5（costmap 代价场闭环，治撞 box + 贴边）。步 6（/scan raytrace）属 S2，S1 先用 bbox + decay。

---

## 6. 不在本期（标记后续）

- DWB/MPPI 局部控制器替代 PP+VFH（S3，P1）
- Recovery Spin/BackUp（S4，P2）
- 行为树编排（S6，P3）
- 在线重规划（S5，需配合 PurePursuit reset）
