# 方案评审 — 巡逻回程机台死锁（path_pts=0）修复

> 日期：2026-08-17
> 上游：docs/change_journal.md [2026-08-16] 巡逻回程死锁条目（根因初判 + 3 修复候选）
> 性质：大脑层改动准入评审（按纪律：改大脑须先评审）
> 结论先行：**推荐 C1**——A* 双端点有界吸附（起点+目标），域层可单测，一处机制同时治
> D1（起点自锁）与 D3（目标落膨胀圈，潜在）。C4（object 层重建）列 P2 跟进，
> C2/C3 否决（理由 §3）。

---

## 1. 症状回顾

完整巡逻：起点→机台1(30s)→机台2(60s) 后静止于 (17.04,-3.91)，decision 每 2s 刷
`plan: start=(17.0,-3.9) goal=(1.0,0.0) inscribed=0.55 path_pts=0`，8 分钟无恢复。
【铁证，journal 原文】

## 2. 根因再分析（对 journal 原假设的修正）

### 2.1 谁能写 INSCRIBED（253）——写入方全集

| 写入方 | 写什么 | 依据 |
|---|---|---|
| `ScanToGrid::raytrace` | 命中点 LETHAL + 射线路径 FREE，**无 inflation** | scan_to_grid.hpp:65-68 |
| `GridUpdater::inflate` | 质心 LETHAL + inscribed 内 INSCRIBED + 外指数衰减 | grid_updater.hpp:48-67 |

→ 起点格被标 INSCRIBED **只可能来自 fusion object 的 inflate**。raytrace 标的
机台面（x≈17.5）只有单格 LETHAL，不产生 0.55m 不可走区。【铁证，代码全集】

### 2.2 journal 原假设的漏洞：skip filter 挡住了"当帧自标"

原假设："机台面在 x=17.5，车停 x=17.04，距面 0.46 < 0.55 → 起点格不可通行 → 必然自锁"。

但 decision_node.cpp:222（本次 08-12 批次已生效，死锁局日志含 `inscribed=0.55`
即为新代码）：

```cpp
if (std::hypot(mx - rx, my - ry) < inscribed) continue;  // 跳过 robot 0.55m 内 object
```

skip 半径 == inscribed 半径（0.55）。**凡与车中心距离 ≤0.55m 的 object（即能把
起点格标 INSCRIBED 的 object），恰是每帧被跳过的**——车停稳后，当帧不可能再
把起点格标进 INSCRIBED。原假设的"必然"不成立。【铁证，几何+代码】

### 2.3 修正后的完整机制链（stale marks + 清障失效）

1. **接近段铺盘**：车距机台 0.55m~fusion 聚类范围内时，机台 face 聚类质心
   ≈(17.5,-4)（lidar 只见面）未被 skip → inflate 铺下 0.55m INSCRIBED 圆盘，
   西缘 x=16.95，含未来停靠格 (17.04,-3.91)。【推断，质心位置由可见面几何推出】
2. **停靠**：车停 (17.04,-3.91)，object 距 0.47m 进入 skip 区，不再重标；
   但**旧盘无删除机制**——唯一清除是"健康射线穿过该格"（raytrace FREE）。
3. **清障失效**：死锁局传感器中途劣化（**扇区失明 2/8，scan_filter WARN 已触发**，
   journal 同条目记录）。西向射线（起点格在 lidar 后 0.25m，仅正西 ±11° 内
   约 22 条射线穿过该格）若落在盲扇区 → 无效射线直接跳过（scan_to_grid.hpp:40，
   `!isfinite || r>max_range → continue`，不清不标）→ 起点格 INSCRIBED 永存。
   【推断，与"该局扇区失明"强相关；健康传感器局西向射线每帧清掉该格，死锁应不复现】
4. **A* 无恢复**：astar_planner.hpp:73 `!is_traversable(start) → return {}`，
   decision 空路径仅 return 下周期重试（decision_node.cpp:259-263）→ 死循环。

### 2.4 缺陷定级（独立缺陷，仿真伪影只是触发器）

| # | 缺陷 | 真机同构 | 定级 |
|---|---|---|---|
| D1 | A* 端点脆断：start/goal 不可走 → 永空路径，无降级无诊断 | 定位漂移进膨胀圈、靠泊位姿即触发 | **必修** |
| D2 | 代价场无衰减：inflate 盘永久累积，唯一删除是射线清除；传感器劣化/遮挡即留死区 | 动态障碍（托盘/他车）路过留永久禁行区 | P2 |
| D3 | patrol 目标 (17,±4) 距机台面 0.5m < 0.55——**目标格本身在膨胀盘内**。现布局幸存仅因派发点距机台 >6.5m（raytrace/fusion 标记上限，scan_to_grid.hpp:26），机台未进 grid | 换布局/近距离派发即目标侧 path_pts=0 | 必修（随 C1 一并治） |

## 3. 候选方案评估

| 候选 | 机制 | 评估 | 结论 |
|---|---|---|---|
| **C1 A* 端点有界吸附** | start/goal 落不可走格时，环形搜索最近可走格为虚拟端点（半径上限=0.75m inflation） | 治 D1+D3 于同一机制；域层纯函数可单测；不动 ROS 层；与 motor 闭环兼容（§4.4） | **推荐 P1** |
| C2 decision 恢复行为（微退/重试） | plan 空且起点被膨胀覆盖 → 触发后退动作 | 本案车未被物理卡住（guard 未拦），是 A* 拒发路径；motor 已有 guard>3s abort→重规划（motor_ctrl_node.cpp:313-325）语义重叠；引入状态机复杂度 | 否决（证据不足前不加状态） |
| C3 patrol 目标外移（治标） | 目标点离机台面 ≥0.55+裕量 | 只治 D3 不治 D1：停靠点仍在旧盘内，起点侧死锁照旧；且送料停靠距离拉远一倍 | 否决（C1 落地后无必要） |
| C4 object 层每帧重建 | fusion inflate 改为独立层：每帧清层重铺，合成进主 grid（对齐 NAV2 rolling window 思路） | 治 D2 根（动态障碍留死区），真机价值高；但改 grid 数据流，涉及并发与 raytrace 合成顺序，工作量独立成项 | **P2 跟进**（另立设计） |

## 4. 推荐方案 C1 详细设计

### 4.1 域层（astar_planner.hpp）

```cpp
struct Params {
  int max_iterations = 50000;
  float heuristic_weight = 1.0F;
  float endpoint_snap_radius = 0.0F;  // 新增，米；0=关闭（现行为，测试零扰动）
};
```

`plan()` 内：world_to_grid 后，若端点格 `!is_traversable`，环形扩张搜索
（r=1..R_cells，R=snap_radius/resolution=15）首个可走格作虚拟端点；超半径仍空 →
返回 `{}`（保留"真被堵"语义，目标在墙内等场景不吞错）。默认 0 关闭 →
现有 6 个 A* 测试与所有调用方行为不变。

搜索代价上界 O((2R)²)=900 格/端点，对比 A* 主体可忽略。

### 4.2 决策层（decision_node）

`AStarPlanner::Params{200000, 1.0F, 0.75F}`。可观测性（防 C1 掩盖传感器问题）：

- path 非空时比较 `path.front()` vs start、`path.back()` vs goal，偏差 >0.1m 打
  `WARN_THROTTLE "endpoint snapped: start+%dm goal+%dm"`（持续吸附=传感器劣化/
  陈旧标记的显性信号，呼应遗留问题"数据质量看门狗"）
- `plan:` 日志行增补 snap 距离

### 4.3 吸附半径取 0.75（inflation_radius）的依据

膨胀场影响域边界即 inflation 外缘：盘内(≤0.75)吸附=“目标贴障碍/起点在影响区”，
语义成立；>0.85（0.75+一格裕量）视为真实堵塞。不取 0.55：D3 目标距面 0.5m，
吸附到 0.55 环可能仍落在衰减区高代价格，0.75 给 A* 留选路余地。

### 4.4 执行链兼容性论证

- 虚拟起点 → motor 每周期 `path[0] = current`（motor_ctrl_node.cpp:354），缝隙自闭合
- 车头朝机台、路径向西：PurePursuit 大 α 原地转向（pure_pursuit.hpp:92-99
  "rotate in place"），转向后机台出 guard ±45° FOV，无钳速；guard 只压
  `twist.linear` 不碰角速度（motor_ctrl_node.cpp:303-309）
- 目标吸附 → motor 以吸附点收尾，最远差 0.75m：见风险 R3

## 5. 单测与集成验收

单测（quality/src/test_astar_planner.cpp 增补，纯域层）：

| 用例 | 断言 |
|---|---|
| 起点在 INSCRIBED 盘内（0.5m） | path 非空，首点距 start ≤0.75 且可走 |
| 目标在 INSCRIBED 盘内（0.5m） | path 非空，末点距 goal ≤0.75 且可走 |
| 目标在 LETHAL 墙芯（>0.75） | path 为空（堵塞语义保留） |
| snap_radius=0（默认） | 端点不可走 → 空（现行为回归锁） |
| 端点可走 | 首末点=端点格（零吸附回归锁） |

集成验收（行为级，对齐 journal 验证格式）：

- run_sim.sh 抽签健康启动后完整巡逻 ≥2 圈：机台2 停靠 60s 后**正常离站**，
  无 path_pts=0 死循环；日志可见（如传感器劣化）endpoint snapped WARN
- 健康局全程无 snap WARN（吸附不误触发）

## 6. 风险与交互

| # | 风险 | 缓解 |
|---|---|---|
| R1 | 吸附掩盖传感器劣化（每帧 snap 照常跑，盲行） | §4.2 WARN + 后续看门狗项（journal 遗留 1）合流 |
| R2 | 虚拟起点路径引导车穿越"理论不可走"区 | 仅 0.09-0.75m 盘内段，方向背离障碍（§4.4）；guard 物理兜底不变 |
| R3 | 目标被吸附时 motor 停在距原目标 ≤0.75m，patrol ARRIVE_DIST=0.5 可能不满足 → patrol 停等 | patrol ARRIVE_DIST 提至 1.0（仅 patrol_3c.py 一行）；decision 到达判定 kArrivalTolerance 维持原值（现布局机台派发距离 >6.5m 不触发目标吸附，无交互） |
| R4 | 吸附选格方向任意（理论上可贴向障碍侧） | 环形最近优先 + 盘内均不可走、盘外均安全，方向由几何保证；单测覆盖 |

## 7. 评审门禁

- [ ] C1 机制与 §4.1 签名无异议
- [ ] R3 处置（ARRIVE_DIST 1.0）认可
- [ ] C4 立项认可（P2，另出设计）
- [ ] 通过后按 §5 实施：先单测后集成，journal 记录六元组
