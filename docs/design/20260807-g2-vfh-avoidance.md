# G2-B 局部避障：VFH 绕行

> 日期：2026-08-07
> 范围：G2 局部避障完整版——在 G2-C 安全护栏（停）之上加 VFH 主动绕行
> 前置：G2-C 护栏已就位（`20260806-g2-collision-guard.md`）、坐标系修复完成

## 1. 问题

G2-C 护栏让车遇障**停**，但不会**绕**。完整局部避障需在毫秒级内主动绕开动态障碍，保持任务前进。

## 2. 目标与非目标

- **目标**：障碍进入介入距离时，VFH 计算绕行方向，修正转向（w），车保持前进绕开；障碍移除后回到 PurePursuit 全局跟踪
- **非目标**：
  - 完整 VFH+（多级直方图、坡度、代价函数）——用简化单层 VFH，够用且可测
  - 与全局路径的协调（decision 层 A* 兜底重规划）
  - 速度规划（VFH 只出转向，速度由 PurePursuit + 护栏管）

## 3. 架构位置

```
execute() 20Hz 循环
  tracker_.track() → (v, w)
  → VFH: 若目标方向有近障 → 修正 w（转向绕行），v 微降保证转向
  → CollisionGuard: clamp v（安全底线，<stop 硬停）
  → publish_twist
```

- **VFH 管"绕"**（方向修正），**护栏管"停"**（速度底线），两层解耦
- 车绕行时护栏仍生效：若绕行方向也有障碍（如窄道），护栏降 v → 停 → 超时 → decision 重规划

## 4. VfhAvoidance 算法（简化 VFH）

```
输入：scan（ranges + angle_min/increment）、goal_angle（车体帧，目标方向角）

1. 介入判定：goal_angle 方向 ±fov_half 内，最近障碍距离 < active_range？
   → 否：输出 0（PurePursuit 控制）
2. 分 bin：360° / bin_count 扇形，每 bin 记录最近障碍距离（忽略 inf/NaN）
3. 可通行 bin：最近障碍距离 > passable_threshold
4. 找 gap：连续可通行 bin 序列，长度 >= min_gap_bins
5. 选 gap：所有 gap 中心方向中，最接近 goal_angle 者（θ_avoid）
6. 输出：w = clamp(k · angle_diff(θ_avoid, 0), ±max_steering)
         且 v 侧向绕行时轻微降速（调用方处理）
```

**边界**：
- 全向被围（无 gap）→ 输出 0 + `blocked` 标志，护栏负责停
- 静止（v≈0）→ 不绕行（避免原地打转）
- inf/NaN 点忽略（无回波 ≠ 障碍）
- 绕行方向只在障碍近时介入，远障碍零扰动

## 5. 参数

| 参数 | 值 | 依据 |
|---|---|---|
| `active_range` | 1.2 m | 介入距离（> safe_dist 0.8，提前绕） |
| `bin_count` | 60 | 每 6° 一个扇区 |
| `passable_threshold` | 0.5 m | 扇区可通行需最近障碍 > 0.5m（车身半宽裕量） |
| `min_gap_bins` | 3 | 最小 gap = 18°，过窄通道判不可行 |
| `max_steering` | 1.5 rad/s | 与 PurePursuit max_angular 一致 |
| `fov_half` | 0.7854 (45°) | 介入判定只看目标方向前向 |

## 6. 类上限自检

**VfhAvoidance**（新领域类，header-only）：
- `.h` ~120 行 / 公有方法 3（构造、steer、params）/ 成员 1（params_）
- 无状态（纯函数式），每帧从 scan 算
- 达标

**PurePursuit**（既有）：加 1 个公有方法 `lookahead_bearing()`（返回车体帧目标方向角，供 VFH 用）。只加方法不加成员，不触红线。

**MotorCtrlNode**（既有）：成员已 12 个（既存红线 + G2-C 声明）。本迭代 +1（vhf_）。偏差已在 G2-C 文档记录，拆分执行层列 backlog。

## 7. GWT 场景清单（TDD）

| # | 场景 | 断言 |
|---|------|------|
| 1 | 目标方向有近障（障碍正前） | 输出非零 w（绕行转向） |
| 2 | 目标方向清晰 | 输出 0 |
| 3 | 障碍在侧向（远离目标方向） | 输出 0（不扰动 PurePursuit） |
| 4 | 障碍在前、gap 在左 | w 左转（绕向 gap） |
| 5 | 全向被围（无 gap） | blocked=true + w=0（护栏停） |
| 6 | 多 gap | 选最接近 goal_angle 的 |
| 7 | ranges 含 inf/NaN | 忽略 |
| 8 | v≈0（静止） | 不绕行（0） |

## 8. 仿真数据评测（G3 数据基线前置）

| 指标 | 定义 |
|---|---|
| 绕行成功率 | 车绕障后到达目标的比例 |
| 最近距离 | 绕行中车与障碍的最小间隙（>安全） |
| 单次耗时 | 绕行场景 vs 无障碍基准的耗时增量 |

场景：warehouse 直行路径中间放障碍（复用 gz create）→ 车自动绕行 → 记录数据。
产出数据表 + 结论（VFH 是否满足"绕行不撞、效率可接受"）。
