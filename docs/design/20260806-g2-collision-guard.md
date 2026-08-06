# G2-C 安全护栏（CollisionGuard）设计

> 日期：2026-08-06
> 范围：G2 局部避障第一阶段——碰撞监测速度护栏（安全底线）
> 后续：G2 完整局部避障（VFH/DWB），护栏作为常驻安全层保留

## 1. 问题

当前执行链 `execute()` 的 PurePursuit 只响应 /odom 反馈，对动态障碍（行人/叉车突然进入路径）无感知。感知→A* 重规划链路 ~5Hz 秒级，来不及急停。

## 2. 目标与非目标

- **目标**：毫秒级碰撞监测——任何速度指令发布前，经 /scan 护栏钳制；障碍进入安全距离即减速，进入停车距离即急停。
- **非目标**（G2 后续）：
  - 主动绕行（VFH/势场方向修正）
  - 与全局路径的协调重规划（decision 层已有障碍栅格 → A* 重规划兜底）
  - 可通行性判定（护栏只关心"前方有没有东西"，不判定绕行方向）

## 3. 方案：执行层速度护栏

```
execute() 20Hz 循环
  tracker_.track() → twist
  ────────────────────────────── 插入点（G2-C）
  twist.linear = guard_.clamp(twist.linear, now)   # /scan 护栏
  ──────────────────────────────
  publish_twist(twist)
```

- `/scan` 订阅（独立 callback group，与 odom 一致避免被 execute 饿死）
- 领域层新增 `CollisionGuard` 纯算法类（无 ROS 依赖，可 GWT 测试）
- **只钳线速度，不动角速度**：差速车原地转向可绕开静态障碍

## 4. CollisionGuard 算法

```
输入：ScanData{ ranges[], angle_min, angle_increment }，cmd_v，now
FOV 内最近距离 d = min{ r : angle(r) ∈ [-fov_half, +fov_half] }

  d <= stop_dist     → v = 0              # 急停
  d <= safe_dist     → v *= (d-stop)/(safe-stop)   # 线性减速
  否则               → v 原样

角速度永远不变。
```

**边界处理**：
- `r == inf` / `r > range_max` / `NaN` → 忽略（无回波不是障碍）
- `angle_min + i*angle_increment` 落在 FOV 外的点 → 忽略
- **scan 超时陈旧**（距上次收到 > 500ms）→ 保守停（无感知 = 不安全）
- 无效/空 scan → 保守停

**阻塞状态（防死锁，G2-C 必须闭环）**：
- `is_blocked(now)`：当前 v 被障碍钳制为 0（d<=stop 或 stale/empty）
- 护栏连续阻塞 > 3s → execute() 急停并返回失败（reached=false）
- decision 收到失败 → 释放 active_goal_ → 下周期 A* 重规划
- 保证：**不撞（护栏）+ 不死锁（超时放弃）**

**已知边界**：A* 栅格障碍（离散感知点）与激光扫描（连续）不一致，窄道场景可能"试→超时→重派"反复循环。不致死锁但低效，属 G2 完整避障（路径协调）范畴，G2-C 不处理。

## 5. 参数

| 参数 | 值 | 依据 |
|------|-----|------|
| `stop_dist` | 0.3 m | 车身 0.45×0.35m，前向裕量 |
| `safe_dist` | 0.8 m | > stop，线性减速区间 |
| `fov_half` | 45° | 车头 ±45°，LiDAR 360° 只取前进方向 |
| `scan_stale_timeout` | 500 ms | /scan 10~25Hz，> 2 周期即判陈旧 |
| `range_max` | 20.0 m | gz sick_tim781 量程 |

## 6. 类上限自检

**CollisionGuard**（新领域类）：
- `.h` ~50 行 / `.cc` ~90 行 / 公有方法 5 / 成员变量 3（params_, scan_, recv_time_）
- 全部达标。

**MotorCtrlNode**（既有）：
- 成员变量 10 个，已超 §6 红线（≤5）。**既存违规**，G1 之前已存在。
- G2-C 增量 +2（scan_sub_、guard_）。
- **偏差声明**：按 §8 输出——完整拆分执行层（抽取 PoseFeed/Publish 等）涉及重构 motor 核心路径，收益 vs 回归风险不匹配当前迭代。列入 backlog：G3 前做 `motor` 执行层拆分（拆出 VelocityPublisher / ScanFeed，成员减至红线内）。

## 7. GWT 场景清单（TDD）

| # | 场景 | 断言 |
|---|------|------|
| 1 | FOV 内无点 | v 原样 |
| 2 | 障碍在 safe_dist 外 | v 原样 |
| 3 | 障碍在 (stop, safe) 内 | v 按 (d-stop)/(safe-stop) 线性缩放 |
| 4 | 障碍 <= stop_dist | v = 0 |
| 5 | 障碍在 FOV 外（侧/后） | 不影响 v |
| 6 | ranges 含 inf / NaN | 忽略，不影响 |
| 7 | scan 超时陈旧 | 保守停 v = 0 |
| 8 | 空 scan | 保守停 v = 0 |
| 9 | clamp 只改 v | w 不变 |
| 10 | 障碍持续阻塞超 3s | is_blocked() = true |
| 11 | 障碍在超时前移开 | is_blocked() = false |

## 8. 仿真验证场景

1. 无障碍直行：护栏不触发，速度原样
2. 路径中放障碍物（spawn box）：车接近 → 减速 → 停，不撞上
3. 障碍移除：恢复原速
