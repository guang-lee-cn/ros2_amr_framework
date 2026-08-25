# 改动日志（change journal）

> 格式：症状/假设/证据/改动/验证/回滚 六元组，按 ros2-amr-debug 技能纪律记录。
> 每条注明证据可信度等级：铁证 / 推断 / 继承。

---

## [2026-08-25] P0 事故处置：SROS2 私钥泄露进已推送历史（轮换+重写+防再犯）

> 发现：外部 L7 工程审计（doc/ITERATION.md §2）。处置执行见本条。

- **症状**: `git cat-file -p 8c4873b:install/.../config/sros2/private/ca.key.pem`
  可完整恢复 CA 私钥明文；共 11 个私钥文件（CA 三件套 + 8 enclave key）随
  8c4873b（install/ 产物误提交）进入 origin/main 历史。HEAD 干净（0177fca
  清理 + .gitignore 正确）但历史 blob 永久可恢复。【铁证：处置前逐条复现】
- **处置（按审计要求三步 + 防再犯）**:
  1. **备份**: `git bundle create /tmp/ros2_amr_pre_filter_backup.bundle --all`
     （23MB，重写出错可回退；注意它含泄露密钥，验证后应删除）
  2. **轮换**: 旧 keystore 整体烧毁（.burned 残骸已删）；`ros2 security
     create_keystore` 新 CA 三件套 + 8 enclave（/camera…/motor_ctrl）；
     private/ 与 enclaves/ 在 gitignore 内（验证：git status 不显示）；
     跟踪的 public 证书更新为新 CA 签发（commit 3e17bbe，重写后 65428bd）
  3. **重写**: git-filter-repo（单文件脚本 ~\/.local/bin）`--invert-paths
     --path-glob '*/sros2/*key.pem' --force`——244 commits 重写 0.13s，
     新 tip 65428bd；`git log --all -- '*key.pem'` 清零、8c4873b 不复存在
  4. **强推**: `git push --force origin main`（filter-repo 移除 origin 后重挂）
  5. **防再犯**: CI 增 secrets-scan job（gitleaks-action@v2，fetch-depth:0
     全历史扫描，先于构建；公开仓库免 license）
- **验证**【铁证】:
  - 本地: `git log --all -- '*key.pem'` 空；`git cat-file -p 8c4873b` fatal
  - 远端 git: `git log origin/main -- '*key.pem'` 空；tag v0.1.0 指向泄露前
    commit（哈希未变、在新历史内）；无 refs/pull/* 保活
  - **残留（已知边界）**: GitHub 平台 GC 前，悬空 commit 8c4873b 仍可经
    API/直链访问（gh api 实测返回完整 JSON）——需 GitHub Support 工单请求
    gc；密钥已轮换烧毁，残留泄露面只影响已作废材料，风险有界
- **影响面通知**: 历史重写后所有旧 clone 失效——需 `git fetch origin &&
  git reset --hard origin/main`（或重新 clone）；本地若有未推送分支需 rebase
- **回滚**: 不适用（安全处置不可逆是目的本身）；出错恢复用备份 bundle
  `git clone /tmp/ros2_amr_pre_filter_backup.bundle`

---


## [2026-08-25 晚] supervised_sim rollout：仿真栈 11 子进程迁入 supervisor + 三 bug 现形

> B1 的落地验证轮（上一条的后续）。变更：launch/supervised_sim.launch.py
> （simulation.launch.py 的每个 RosNode → supervisor 声明式子项）+
> run_sim.sh LAUNCH_FILE 覆盖 + soak 白名单扩 compute（supervised 形态限定）。

- **症状 → 根因 → 修复**（每条都被真实运行暴露）:
  1. **oneshot 无限重跑**: spawn_amr 每 ~2s 重放一次车（gz 里堆机器人）——
     `completed_` 只在级联时 erase、从不在成功时置位。修复: feed() 里
     oneshot EXITED_OK → 置位 + "完成 ✓" 日志【铁证: 重跑前日志循环 / 修复后恰 1 次】
  2. **级联漏清 oneshot**: gz 重启后 spawn_amr 不重跑（新世界无车）——
     cascade_yield 的 `continue` 跳过 STOPPED 子项在 erase 之前。修复: 先清
     标记再判相位【铁证: 修复后 gz 击杀 → spawn_amr 二次完成 + DiffDrive
     entity 46 加载，/tmp/sup_direct.log】
  3. **清场孤儿 foxglove 占 8765**: run_sim clean 的 pkill 清单不含
     foxglove_bridge/static_transform_publisher（路径不在本包 lib 下）——被
     清场的栈留下孤儿，后续每次启动 Bind Error 崩溃循环直至 FATAL。
     08-16「清单不全清场留孤儿」同型复发。修复: clean() 补两个 pkill 模式
     【铁证: ss -tlnp 见孤儿 pid 170086 占 8765；补齐后 foxglove 稳定 RUNNING】
  4. **附送考古**: clean() 的 `rm /dev/shm/fastdds*` 是多年无效 glob——实际
     文件名 fastrtps_* 段 + sem.fastrtps_port*_mutex 端口锁；FastDDS C++ 日志
     走 stdout 污染 probe 输出致假判定。修复: 三类文件全清 + probe 只取纯数字行
     （run_sim/soak_run/sim_watchdog 三处同修）
  5. **交互式 pkill 自杀**: 清场命令 `pkill -f 'gz sim'` 匹配到自身 shell
     （CLAUDE.md 禁止清单原文明示）→ 命令静默中断、清场"没跑"。改用方括号
     模式/显式 PID。run_sim.sh 的 clean() 本身不受影响（独立进程 cmdline 安全）
- **rollout 验证**【铁证，/tmp/sup_direct.log + /tmp/sim_run_194459_try2.log】:
  - 11 子进程拓扑序错峰拉起（gz→spawn_amr/bridge→scan_filter/mock_amcl→…→patrol）
  - **kill -9 compute**: 250ms 检出 → patrol 让位 → 1.25s 退避 → 1.75s 全恢复，
    scan 全程无恙（不触发 watchdog 全栈重启）——soak compute 注入正式解锁
  - **kill -9 gz**: 逆拓扑级联（patrol→compute→mock_amcl→scan_filter→bridge）→
    1s 退避 → gz 重生 → spawn_amr 重放车 → 2.3s 全链回位 → **新抽签 254 回波健康**
    （顺带治好 kill 前已劣化到 46 回波的渲染——秒级重抽对比 watchdog 2-4min）
- **遗留**: 当晚 WSL2 渲染抽签偏冷（多轮 3/3 全盲），run #3 的健康局 + 直连裸跑
  完成全部验证矩阵；72h soak 用 supervised 形态跑前建议先 wsl --shutdown 复位
- **回滚**: `git checkout` 本条涉及文件（launch/supervised_sim.launch.py 删除 +
  scripts/run_sim.sh scripts/soak_run.sh scripts/sim_watchdog.sh +
  src/infrastructure/supervisor_node.cpp）

---

## [2026-08-25] B1 supervisor 落地：进程级监管/依赖序重启/预算 FATAL（迭代2 B1）

> 设计：docs/design/20260825-b1-supervisor-adr.md；验收：迭代2 B1
> 「声明式配置驱动；kill -9 任一节点按策略恢复」

- **症状（缺口）**: ros2 launch 无 respawn——compute_container 被 kill -9 后
  无人拉起，机器人永久停滞（soak 注入白名单因此排除 compute）；
  sim_watchdog 只看 scan、整栈重启、恢复 2-4min。
- **改动（三层）**:
  1. `domain/monitoring/supervisor_policy.hpp`：状态机纯逻辑（STOPPED/
     STARTING/RUNNING/BACKOFF/FATAL）+ 指数退避 + 窗口化重启预算 +
     Kahn 拓扑序；语义对齐既有 RecoveryPolicy（budget→FATAL、稳定清零）
  2. `infrastructure/supervisor_node.{hpp,cpp}` + `amr_supervisor` 可执行：
     posix_spawn 独立进程组拉起、waitpid(WNOHANG) 250ms tick、级联让位
     （死源逆拓扑杀依赖者）、HealthReport(latched) 状态出口、AmrNode 基类
  3. 声明式配置：`supervisor.<name>.cmd/depends_on/oneshot/退避预算` 参数；
     环/未知依赖启动即拒
- **验证**【铁证，/tmp/supervisor_val.log】（3 真实子进程
  scan_filter→compute→patrol，拓扑链）:
  1. 拉起：按拓扑序错峰 250ms 三级全 RUNNING
  2. kill -9 compute：**250ms 检出 → patrol 组杀让位 → 500ms 退避 →
     compute 重生 → patrol 跟随重生，全程 1.0s**（对比 watchdog 全栈 2-4min）
  3. 连杀 scan_filter 4 次（预算 3）：第 4 杀 → FATAL + 级联停全部依赖者，
     子进程清零；/supervisor/report 如实报 ERROR/STALE（health_monitor 兼容格式）
  4. SIGINT supervisor：逆拓扑 teardown 无泄漏
  5. 单测 22 用例（test_supervisor_policy）：拓扑/退避/预算/稳定窗/
     oneshot/迟到事件免疫/级联让位
- **边界**: supervisor 自身单点（真机 systemd 兜底）；健康门 v1=进程存活，
  v2=心跳确认（START_TIMEOUT 事件路径已就位）；sim 栈整体迁到 supervisor
  之下（supervised_sim launch）是 rollout 下一步，soak compute 注入待其解锁
- **回滚**:
  ```bash
  git checkout -- CMakeLists.txt CHANGELOG.md docs/ docs/change_journal.md \
    include/ros2_robot_middleware/domain/monitoring/supervisor_policy.hpp \
    include/ros2_robot_middleware/infrastructure/supervisor_node.hpp \
    src/infrastructure/supervisor_node.cpp src/infrastructure/supervisor_main.cpp \
    quality/src/test_supervisor_policy.cpp
  rm -f src/infrastructure/supervisor_main.cpp  # 及新文件
  colcon build --packages-select ros2_robot_middleware
  ```

---


## [2026-08-18] sim_watchdog 运行时看门狗上线：scan 劣化自动清场重启（90a1a86 承诺的"后续"）

- **症状**: guard fail-safe（min_valid_echoes=50）只负责安全停车+告警等待，
  gpu_lidar 运行中劣化（扇区失明→全盲）后的恢复仍靠"人盯日志→人跑 run_sim"，
  长时巡逻/无人值守时即永久停摆。
- **改动（仿真运维层，大脑零改动）**:
  1. 新增 `scripts/sim_watchdog.sh`：每 30s 探测 /scan_raw 有效回波，连续 4 轮
     <50（2min 判定窗，覆盖 run_sim 最坏 3×(25s+6s) 启动窗）→ 调 run_sim.sh
     清场重启，抽签逻辑复用不重写
  2. CMakeLists 注册安装（share/，与 run_sim.sh 同级运维脚本）
- **设计要点**:
  - 阈值 MIN_VALID=50 与 guard_min_valid_echoes（simulation.launch.py:111）同判据，
    watchdog 与 brain fail-safe 对"失明"的认定一致
  - 只看 scan 健康不看车动不动——车停等可能是业务停车，误判重启会杀正常任务；
    传感器坏了则重启永远是对的
  - 无 launch 进程时不计数，避免与 run_sim 自身启动抽签叠加误判
- **验证**【铁证】:
  - 17:05 run_sim 1 次尝试即健康（仅 try1 日志、无重试），本局 342 有效回波
    持续稳定（10min+ 后实测探针仍 342）
  - 17:07 watchdog 上线（PID 4069），至今静默运行 = 无劣化、无重启误触发
  - bash -n 语法通过；探测逻辑与 run_sim.sh probe 同款
- **运行规程**: `nohup ./scripts/sim_watchdog.sh >/tmp/sim_watchdog.log 2>&1 &`
  （停止 `pkill -f sim_watchdog.sh`）；日志只在劣化/恢复/重启时说话
- **回滚**: `git checkout -- CMakeLists.txt docs/change_journal.md && rm scripts/sim_watchdog.sh`；
  运行中实例 `pkill -f sim_watchdog.sh`

---

## [2026-08-17 下午] compute_container 启动失败一例：decision 功能性死亡（待查）

- **症状**: 健康抽签局（346 回波、全程 0 扇区告警），patrol 设 goal 后 2 分钟+
  机器人零速度零位移（/amcl_pose 原点、/cmd_vel 无消息），decision 无任何日志
  （plan/gate/fusion_hb/raytrace 全无）。【铁证：话题级取证】
- **证据链**【铁证，原始日志已随 run_sim 清场删除，以下为当时捕获】:
  1. `/perception/objects` 5Hz 正常、`/sensor/fusion/heartbeat` 1Hz 正常 →
     fusion 活着（同容器！）
  2. `/decision/heartbeat` 不发布 → decision 的 lifecycle 激活未完成或 executor 卡死
  3. compute_container 每 5s 刷 `async flush: thread pool doesn't exist anymore`
     （spdlog async 池被销毁后 logger 仍引用）——上上局同错误但功能正常，
     是伴随现象非直接死因
- **假设（待验证）**: compute_container 启动序列（configure→activate）存在竞态，
  observability async logger 初始化失败可能与 decision 激活失败相关。
  20+ 次启动中仅出现 1 次 → 低概率竞态。
- **改动**: 无（单次出现，先记录；复现后按六元组深挖）。
- **遗留**: 环境恢复后加 compute_container 启动自检（decision/motor heartbeat
  缺失 N 秒 → 容器自重启或告警），与"数据质量看门狗"同属健康监控项。

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
