# 改动日志（change journal）

> 格式：症状/假设/证据/改动/验证/回滚 六元组，按 ros2-amr-debug 技能纪律记录。
> 每条注明证据可信度等级：铁证 / 推断 / 继承。

---

## [2026-08-17] 机台死锁修复落地：A* 端点有界吸附（C1）+ 验证发现 stale 区可超吸附上限

> 上游：docs/design/20260817-machine2-deadlock-review.md（评审通过后实施）

- **症状**: 同 [2026-08-16] 机台2 死锁条目（path_pts=0 死循环）。

- **改动（大脑层，按评审 C1）**:
  1. `astar_planner.hpp`：Params 增 `endpoint_snap_radius`（默认 0=关闭，零回归）；
     plan() 端点不可走时吸附到半径内**欧氏最近**可走格（圆盘窗 d²≤R²，非方形窗
     ——方形窗对角格可超半径 0.99m）。超半径 → 空路径（真被堵语义保留）
  2. `decision_node.cpp`：snap 0.75 启用（Params{200000,1.0,0.75}）；吸附 >0.1m
     打 WARN_THROTTLE（防吸附掩盖传感器劣化，评审 R1）；空路径时 dump 端点
     cost + 起点周边 16×16 cost 窗（区分起点堵/目标堵/不可达三种空因）
  3. `patrol_3c.py`：ARRIVE_DIST 0.5→1.0（评审 R3：吸附停靠距原 goal ≤0.75m）
  4. 测试 +6：吸附起点/吸附目标/超半径保留空/默认关闭回归锁/零吸附回归锁/
     decision 层"单障碍压目标→吸附派发"新契约（原 BlockedGoal 测试改为 3×3
     障碍堆埋目标——旧"单障碍不派发"契约已被靠泊语义取代）

- **验证**:
  - 单测 216/216 全绿【铁证】
  - 集成（run_sim.sh 抽签，266 回波健康局）：完整 **2 圈**巡逻
    （2 次机台2 停靠→离站，2 次回家），全程 0 次 EMPTY path，
    机台2 完成后 30ms 内规划出 352 点回家路径【铁证，/tmp/sim_run_105321_try2.log】

- **⚠️ 验证中发现的新事实（run1，同样带修复，死锁仍复现一次）**【铁证】:
  1. 停靠时 `/perception/objects` 为**空列表**——激光距机台面 0.22m，回波被
     scan_filter（<0.35m）滤除，机台从感知"消失"：既不重标也无 object 可 skip
  2. **西向逃逸走廊 6.5m 内无回波**（料架在 y∈[-2.45,2.45]，机器人在 y≈-3.9
     以南全是空地）→ 全部西向射线 r>max_range → raytrace 视为无效**不清不标**
     （scan_to_grid.hpp:40）→ stale 膨胀盘永不清除。**不依赖盲扇区**——
     max_range 语义缺口单独即可致不清障（修正评审 §2.3 第 3 点的归因）
  3. run1 死锁时 0.75m 吸附窗内无任何可走格 → stale 阻塞区宽度 **>0.75m**，
     超出吸附上限（推断：接近段多个融合 object 盘叠覆西向条带）。run2 无盘
     沉积 → 停靠格本就 FREE → 即时离站
  - 结论：C1 治"盘 ≤0.75m"场景（单测证明）；**盘 >0.75m 仍可死锁**（run1 实证），
    根治在 D2/C4（代价场清障语义）

- **遗留（C4 立项时一并设计，优先级升至 P1.5）**:
  1. raytrace 对 max_range/inf 射线应**清障到 max_range**（NAV2 ObstacleLayer
     语义：无回波=该方向自由空间证明）——一行级改动直接治 run1 场景
  2. C4 object 层每帧重建（评审原案，治动态障碍留死区）
  3. 数据质量看门狗（继承 08-16 遗留）：objects 空列表 + scan 持续无回波应告警

- **回滚**:
  ```bash
  git checkout -- include/ros2_robot_middleware/domain/planning/astar_planner.hpp \
    src/infrastructure/decision_node.cpp scripts/patrol_3c.py quality/src/test_astar.cpp \
    quality/src/test_decision.cpp
  # 重建: colcon build --packages-select ros2_robot_middleware
  ```

---

## [2026-08-16] 仿真小车在起点附近卡死（guard 误停车死锁）

- **症状**: 仿真启动后小车移动约 1m 即停在 (0.75,0.35) 原地缓转，`guard: nearest=0.28m post=0.00` 持续，`decision: gate blocked` 刷屏。**非必现**：同配置多次启动，有的能跑有的卡死。【铁证，复现：重启 simulation.launch.py 数次观察】

- **假设（终版）**: WSL2 headless-EGL 下 gz gpu_lidar 的输出质量**每次启动随机**，从全 inf（盲）到干净呈谱分布；坏签启动时右前象限出现 0.10-0.35m 近距伪影弧，进入 CollisionGuard ±45° FOV（stop_dist=0.30）触发持续钳速，同时 VFH 对幻影避让导致原地打转。【铁证：同配置 3 次启动复测 270/345/193 有效回波、近距伪影 109/0/24；另多次全 inf 启动】

- **假设（已撤回，留档防重蹈）**:
  1. "激光高度 1.5m 致传感器全瞎" —— 被启动随机性污染，归因存疑
  2. "min_range=0.35 致传感器全瞎" —— 同上
  3. "伪影=地面环" vs "伪影=扫到自身车身角"（0.206m 数值吻合）—— 区分实验因传感器随机失效无法得出干净结论，未定

- **证据链**:
  1. 世界文件 factory_3c.sdf 中卡点 1.5m 内无真实障碍（唯一 wall_west 距 0.83m）【铁证】
  2. /points 3D 探针：0.6m 内 210 点全部精确 z=0.000、等距 0.20-0.22m 排弧——真实物体不具备该几何特征【铁证】
  3. guard FOV ±45° 报 nearest=0.28m 而裸探针最近回波在 -66°（FOV 外），说明 ±45° 内亦有幻影，覆盖右前象限【铁证】
  4. `gate blocked` 为 GoalDispatchGate 同目标去重提示（fusion_ready=1），decision 层无故障【铁证，源码 goal_dispatch_gate.hpp:42-50】
  5. 大脑在进链前的最后一站（/points、/scan_raw）即可见伪影 → 大脑被冤枉，非大脑 bug【铁证，数据链路血统论证】

- **改动（全部仿真资产层，大脑零改动）**:
  1. 新增 `scripts/scan_filter.py`：仿真专用 /scan_raw→/scan 中继，滤除 <0.35m 回波（阈值=车身对角 0.32m+0.03 裕量，launch 参数可调）；CMakeLists 注册安装
  2. `config/gz_bridge.yaml`：桥的激光输出 /scan → /scan_raw
  3. `launch/simulation.launch.py`：插入 scan_filter 节点（仅仿真 launch，真机不含）
  4. `worlds/amr.sdf`：仅加排障结论注释，参数保持原版
  - 注：曾试 sdf `min_range=0.35` 方案，因传感器输出随机无法归因 + 保守起见撤回，改桥接层过滤

- **验证（行为级验收）**:
  - 重启抽签至健康启动（346/360 有效、零近距）后：小车 160s 从 (1,0) 完整跑到目标 (17,4)，全程强制停车次数 0，decision 正常派发下一站 (17,-4)【铁证】
  - scan_filter 在线确认：节点列表含 /scan_filter，启动日志"滤除 <0.35m 回波"
  - 运维规程：启动后先跑探针，`有效=0`（盲启动）→ 重启再抽

- **回滚**:
  ```bash
  cd ~/code/ros2_ws/src/ros2_amr_framework
  git checkout -- config/gz_bridge.yaml launch/simulation.launch.py worlds/amr.sdf CMakeLists.txt
  rm scripts/scan_filter.py
  # install 侧: colcon build --packages-select ros2_robot_middleware 重建
  ```

- **遗留问题（下次处理，涉及大脑层需另行评审）**:
  1. **盲启动无告警**：传感器全 inf 时 guard 视 nearest=inf 照常放行、小车凭静态网格盲跑——真机激光断线同构，建议 brain 加数据质量看门狗（N 个周期全 inf/有效回波率 < 阈值 → 降速或停车告警）。此为真机相关需求，符合改大脑的准入条件，但按纪律单独评审
  2. 伪影精确机制未定（受随机性干扰）；若换原生 Linux/真 GPU 环境可复测区分实验
  3. 传感器随机失效的根治（EGL/ogre2 层面）超出本项目范围，靠"抽签+探针"规程兜底

---

## [2026-08-16 下午] 传感器随机失效深挖：进程泄漏混杂 + 运行内劣化 + 健康门控启动器

- **症状**: 盲启动率从早晨的 0/3 恶化到 3/4（/scan_raw 全 inf）。
- **假设与证据**:
  1. 【铁证】**进程泄漏放大器**: `pkill 'gz sim'` 只杀 gz 本体, ros2 launch 全套节点栈残留堆积——发现时已有 5 套栈 (load 25/12核)。CPU 超载饿死渲染线程 → 传感器失效加剧。全量清场后盲率下降。
  2. 【铁证】**清场后仍随机**: 干净系统 3 次启动 = 健康344稳定 / 重幻影283→214 / 168→0运行中致盲。单栈运行内也会劣化, 与负载无关的渲染资源泄漏存在。
  3. 【推断】根因在 gz-rendering ogre2 + WSL2 EGL/D3D12 深度读取路径, 日志无任何错误痕迹(静默), 超出本项目可修范围。
- **改动(全部仿真资产层)**:
  1. 新增 `scripts/run_sim.sh` 健康门控启动器: 全量清场→启动→探针(/scan_raw 有效≥100)→不健康自动重试(默认3次)→连续失败提示 wsl --shutdown 重置 GPU 透传
  2. `scripts/scan_filter.py` 增加盲检测告警: 连续无有效回波每5s 打 WARN
  3. CMakeLists 注册安装
- **验证**: run_sim.sh 实测 1 次尝试即健康(342 有效回波), 仿真正常运行【铁证】
- **回滚**: 同上条目; run_sim.sh/scan_filter 增量可单独 git checkout
- **运维规程(最终版)**:
  1. 一律用 `./scripts/run_sim.sh` 启动仿真, 不再裸 ros2 launch
  2. 停仿真必须全量清场(脚本内 clean 函数模式), 禁止只 pkill gz
  3. 连续多次抽签失败 → Windows 侧 `wsl --shutdown` 重置 GPU 透传(待验证其对劣化的复位效果)
  4. scan_filter 的 WARN "传感器疑似失明" 出现即重启
- **遗留**: wsl --shutdown 后首轮启动健康率对比实验待做(需重启会话); 大脑层数据质量看门狗仍为待评审项

### 补充实验 [2026-08-16]: wsl --shutdown 复位效果

- 重启 WSL 后立即连测 3 轮(每轮全清场): 有效回波 0 / 129 / 218 —— **首轮即盲**, 2/3 健康。
- 【铁证】wsl --shutdown 对渲染劣化无决定性复位作用; GPU 透传复位不是银弹。
- 【推断】随机性为每次启动固有(EGL 初始化竞态), 与系统累计状态关系弱。
- 结论: 运维规程维持"run_sim.sh 抽签(默认3次重试)", 覆盖 2/3 健康率足够; wsl --shutdown 仅在连续多轮失败时作为最后手段。

---

## [2026-08-16] 巡逻回程在机台2死锁 (path_pts=0)

- **症状**: 完整巡逻监测: 起点→机台1(30s)→机台2(60s) 后在 (17.04,-3.91) 静止 8 分钟, decision 每 2s 刷 `plan: start=(17.0,-3.9) goal=(1.0,0.0) inscribed=0.55 path_pts=0`。【铁证】
- **根因 (代码级)**: astar_planner.hpp:73 `if (!is_traversable(start) || !is_traversable(goal)) return {}` —— machine2 为 1×1 盒 @ (18,-4), 面在 x=17.5; 机器人停在 x=17.04, 距面 0.46m < 膨胀半径 0.55m → 起点格不可通行 → A* 永远返回空, decision 按"目标被堵"语义无限重规划, 无恢复。【铁证: 代码行 + 几何计算 + 日志】
- **定性**: 大脑层真实缺陷(非仿真伪影): 巡逻目标(17,-4)本身落在 machine2 膨胀圈内, 到达后必然自锁。真机同构: 靠泊/定位漂移进膨胀圈即导航变砖。
- **同场发现**: 本局中途传感器又劣化(扇区失明 2/8, scan_filter WARN 已触发), 去程仍无碰撞属侥幸; 扇区告警机制实战有效。【铁证】
- **改动**: 本条无代码改动(大脑修改需按纪律单独评审)。
- **修复候选 (待评审, 大脑层)**:
  1. A*: 起点不可通行时以最近可通行格为虚拟起点续算(域层可单测)
  2. decision: plan 为空且起点被膨胀覆盖时触发恢复行为(微退/重试)
  3. (治标) patrol 目标点离机台面 ≥ 膨胀半径+裕量
- **验证/回滚**: 无改动, 不适用。
