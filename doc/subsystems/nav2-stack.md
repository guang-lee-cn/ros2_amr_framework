# NAV2 导航栈集成

> 状态：v2.4.0 生产形态（2026-09-04 收敛，ADR 见
> docs/design/20260904-nav2-convergence-decision.md）
> 本文是 NAV2 集成的运维手册：launch 矩阵、话题契约、安全闸、建图流程、
> 调参速查。

## 一、架构位置

```
感知   /scan_raw + /odom + TF(amr/odom→amr/chassis→lidar)
        │
定位   map_server(预建图) + AMCL(扫描匹配 → map→odom TF)
        │
规划   SmacPlanner2D(全局) ── 全局代价地图: static+obstacle+inflation
控制   DWB(局部 20Hz)     ── 局部代价地图: obstacle+inflation(滚动5×5)
        │
安全   cmd_vel_guard_node(自研域 CollisionGuard)
        │                        正向速度: safe_dist线性减速→stop_dist硬停
        │                        全盲/stale: fail-safe 硬停
        │                        负向(backup恢复): 放行
        ▼
执行   /cmd_vel → 底盘(DiffDrive/场景仿真器)
```

**职责边界**：NAV2 管目标生命周期与「怎么走」；安全闸只钳速度不否决
目标（拦截超时交 NAV2 progress checker——双头死循环的教训，见 ADR §2）。

## 二、Launch 矩阵

| Launch | 用途 | 传感器源 | 定位 |
|---|---|---|---|
| `nav2_localized` | **生产运行** | 真机雷达/场景仿真 | map_server+AMCL |
| `nav2_scene` | **部署期建图**/开发 | 场景仿真器 | slam_toolbox 在线 |
| `nav2_guarded` | L3 演示（SLAM+闸） | 场景仿真器 | slam_toolbox |
| `ab_custom` | A/B 基线（自研栈） | 场景仿真器 | 里程计直通 |
| `nav2_demo` | Gazebo（受限†） | gz bridge | slam_toolbox |

† gz-sim gpu_lidar 渲染线程静默死亡，WSL2 GPU/llvmpipe/云端三环境实证
（1-8 分钟随机）——只适合短演示，长时一律用场景仿真器。

### 场景仿真器参数（nav2_scene）

```bash
scene:=rack_3c          # 4排料架+2机台（默认）
scene:=rack_4box        # 4 个孤立箱
scene:=warehouse_open   # 空场
random_boxes:=6         # 随机静态箱（种子固定=可复现）
movers:=2 mover_speed:=0.6   # 移动障碍（避让机器人，碰墙反弹）
```

## 三、话题契约

| 话题 | 方向 | 类型 | QoS | 说明 |
|---|---|---|---|---|
| `/scan_raw` | 雷达→全部 | LaserScan | best_effort | 场景仿真 /scan 重映射 |
| `/odom` | 底盘→NAV2/AMCL | Odometry | reliable | TF amr/odom→amr/chassis 同源 |
| `/cmd_vel_raw` | NAV2→闸 | Twist | reliable | controller/behavior 重映射 |
| `/cmd_vel` | 闸→底盘 | Twist | reliable | 钳制后输出 |
| `/map` | map_server→static_layer | OccupancyGrid | transient_local | latched，晚加入可取 |

## 四、安全闸（cmd_vel_guard_node）

参数（launch 内置默认）：

| 参数 | 默认 | 语义 |
|---|---|---|
| `guard_stop_dist` | 0.30m | 前向 FOV 内最近障碍低于此 → 硬停 |
| `guard_safe_dist` | 0.80m | 低于此线性减速（0.30→0.80 插值） |
| `guard_min_valid_echoes` | 50 | 全向有效回波低于此 = 传感器失明 → 硬停 |

验证基准：带闸四站巡回 4/4、+2.3s 减速代价、干预全部温和减速零误拦。
**已知开放项**：闸进程未入 supervisor 监管清单（ADR §5.1）。

## 五、建图与地图管理（部署期）

```bash
# 1. SLAM 模式巡航建图（覆盖全部工作区）
ros2 launch ros2_robot_middleware nav2_scene.launch.py scene:=rack_3c
ros2 action send_goal /navigate_to_pose ...   # 逐站覆盖

# 2. 存图（源码树 + install 均需）
ros2 run nav2_map_server map_saver_cli -f maps/<scene名> --occ 0.65 --free 0.25

# 3. 重建（maps/ 随 CMake install 入 share）
colcon build --packages-select ros2_robot_middleware

# 4. 生产 launch 引用 maps/<scene名>.yaml（nav2_localized 内 map_yaml）
```

地图与场景必须同名同几何（AMCL 匹配的是建图时的环境）。

## 六、调参速查（踩坑实录）

| 症状 | 根因 | 参数 |
|---|---|---|
| 窄通道内重规划必败（"from potential"） | 膨胀半径 ≥ 通道半宽，起点必带代价 | `inflation_radius` ≤ 通道半宽−0.1 |
| 高速贴障后卡死 | 贴障无惩罚，冲入 inscribed 环 | `BaseObstacle.scale` ≥ 0.15 |
| NavFn 间歇规划失败刷屏 | NavFn 已知缺陷 | 换 `nav2_smac_planner::SmacPlanner2D` |
| 目标在障碍物内 → 原地打转 | goal 落进膨胀区 | 目标点离障碍 ≥ robot_radius+0.1 |
| 连发目标假 SUCCEEDED | BT 未重置完吃新目标 | 站间冷却 ≥2s（A/B 跑台已内置） |

固定窗代价地图陷阱：无 static_layer 时 origin 默认 (0,0)——机器人
在 (0,0) 附近时起点"越界"，必须显式 `origin_x/origin_y`。

## 七、A/B 对照方法（跑台）

同场景同目标同判定（/odom 0.3m+静止2s）：

```bash
# Side A: NAV2（ab_runner 自带站间冷却与假成功重发）
ros2 launch ros2_robot_middleware nav2_scene.launch.py scene:=rack_3c
python3 ab_runner.py nav2

# Side B: 自研栈基线
ros2 launch ros2_robot_middleware ab_custom.launch.py
python3 ab_runner.py custom
```

基线数据（rack_3c 四站，2026-09-03）：NAV2 4/4 · 48.8s · 96% 里程效率；
自研栈 0/4 · 287.9s（货架角冻结重规划 128 次）。

## 八、验证门（每次导航相关变更后）

1. 定位模式四站巡回 4/4 + AMCL 修正量波动 RMS < 100mm
2. 安全闸干预日志非空且零硬停误拦（正常路线）
3. 随机障碍场景（6 箱+2 移动）4/4
4. CI 四门禁全绿
