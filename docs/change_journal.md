# 改动日志（change journal）

> 格式：症状/假设/证据/改动/验证/回滚 六元组，按 ros2-amr-debug 技能纪律记录。
> 每条注明证据可信度等级：铁证 / 推断 / 继承。

---

## [2026-08-26] 泄露 #2：gitleaks 门禁首次全量扫描抓出 live AI key — 两轮补写

> 续 P0 条目：为泄露 #1 部署的 gitleaks 门禁，在第一次**真正的全量**历史
> 扫描中抓到第二批真实密钥——防线生效的直接闭环。

- **发现路径**: gitleaks-action 增量扫描模式只扫新增 commit（此前各轮皆绿）；
  一次 workflow_dispatch 无基线回落到全量 → 3 类 Finding：
  1. `log/build_2026-07-18_*/events.log`、`log/test_*/events.log`：
     colcon 日志捕获**完整构建环境**——含 `ANTHROPIC_AUTH_TOKEN`、
     `DEEPSEEK_API_KEY`（live AI 服务凭证）【真实泄露】
  2. `build/ros2_robot_middleware/colcon_command_prefix_*.sh.env`：同源
     另一份环境转储（首轮已随 build/ 清除）【真实泄露】
  3. `benchmarks/bench_zenoh/zenoh_latency.py` 的 `PING_KEY`：zenoh 主题
     路由名（key expression），generic-api-key 按变量名误报【误报】
- **处置**:
  1. 备份 bundle（/tmp/ros2_amr_pre_filter2_backup.bundle，含两次重写前状态）
  2. filter-repo 第二轮清 `build/`、第三轮清 `log/`（均 --invert-paths，
     强推验证旧对象不可达）；HEAD 无需改（两路径 e69f1af/d514198 早已清）
  3. 误报处置：HEAD 变量更名 PING_KEY→PING_TOPIC（语义本就是 topic）+
     `.gitleaks.toml`（extend useDefault + 定向放行，附复核结论）
- **验证**【铁证】: 重写后全量 secrets-scan success；fastrtps/cyclonedds
  构建腿全绿（cyclonedds 的 negative-count 亦由 -fprofile-update=atomic
  治愈，同日闭环）
- **持有人轮换（2026-08-28 完成闭环）**: 比对确认泄露两变量实为同一枚
  DeepSeek key（当时 ANTHROPIC_AUTH_TOKEN 复用了它；当前 BigModel token
  未泄露）。新 key 已替换 ~/.claude/credentials.d/deepseek.key 并实测可用；
  **旧 key 实测已失效**（平台侧删除 + API 返回 invalid）；本机明文残留
  （file-history 快照删除 + 会话转录精确串替换）清零。
  至此泄露 #2 全闭环：历史重写 + key 作废，GitHub 悬空 commit 仅余废纸
- **教训**: ① colcon 的 build/ 与 log/ 都会捕获完整环境，构建产物目录
  永不入库（.gitignore 已覆盖，泄漏源是 gitignore 之前的 7/18 提交）；
  ② 增量扫描有基线盲区——新装门禁后应先手动 dispatch 一次全量基线
  （本次教训已固化：dispatch 兜底常驻 ci.yml）

---

## [2026-09-01] P1 清扫 — 告警规则/恒真断言/trace 开关

- **Prometheus 告警规则**（config/prometheus/amr_alerts.yml）：四组规则
  ——传感器失明（08-17 模式）/节点死亡/supervisor FATAL/控制环延迟——
  观测从「只看不叫」到「会叫」。阈值从事故史复盘中来，每条带 annotations
- **恒真断言修复**（2 处 EXPECT_GE(size(), 0u)）：
  - test_fusion SimulatedSensors_DetectObjects：默认场景无障碍物（恒真
    断言掩盖了这一事实）→ 改用 lowstep 场景 + 等到 objects 非空
  - test_sensor_hal Fuse_ProducesClusters：改为多 tick + 行为级断言
- **trace 死代码**：AMR_TRACING_ENABLED 无任何 CMake 定义（100% 死）→
  加 option(AMR_TRACING) 编译期开关（默认 OFF）——C1 换 LTTng 后本开关
  降级为 fallback
- 38 模块全绿 + cppcheck 0 错

---

## [2026-09-01] 复审整改 R1-R3 — 度量诚实/P0-G 反转/P0-B 竞态根治

> 执行方案：docs/design/20260901-reaudit-fix-decision-record.md §四+§六。
> 元纪律：每项修复的验证不引用自身产物（P0-I 教训），命令级取证。

- **R1.1 initial capture 根修**：补 `--ignore-errors mismatch,gcov`（与
  runtime 对齐）+ 去 `|| true` + 非空断言——消灭静默失败通道。
  **验证【铁证】**：`grep supervisor_node coverage_full.txt` = **1 条**
  （0.0%, 0/229 行）——initial 机制首次真正生效
- **R1.2 门禁冲突裁决**：真分母 76.9% < 80% 门禁 → 路径②显式豁免
  supervisor_node（domain 22 单测覆盖逻辑，infra 侧需真实进程环境）
- **R1.4 排除注释**：fleet_manager/compute_container/supervisor_node
  三行排除全部带理由
- **附带修复**：R1.4 注释插入曾打断 bash 续行（`\\` 后 `#` = 注释掉续行）
  导致 remove 段执行不到——教训：改脚本后 `bash -n` + 端到端跑一次
- **R2.1 单一事实源**：`kDefaultMinValidEchoes = 50` 常量在 collision_guard
  定义，domain Params 默认与 motor_ctrl_node declare_parameter **都引用它**
  ——node 侧无法独立回退为 fail-open
  验证：`grep kDefaultMinValidEchoes motor_ctrl_node.cpp` 引用而非字面量
- **R2.2 注释口径**：三处 "(sim)" 框定统一改 "(fail-safe default)"
- **R3.1 grid_mutex_ + 快照**：写侧（raytrace/inflate）锁保护、读侧锁内
  160KB 快照拷贝后 A* 在快照上执行——perception 5Hz 写不被 plan 200ms 阻塞
- **R3.2 竞态回归锁**：test_grid_race 两线程（独立 ranges 缓冲 + 被堵
  goal 最大化交叠）持续 2s——TSAN 下修复前必红
- **验证**：覆盖率 86.7%→81.6%（零覆盖文件真入分母后的真数字）；
  38 模块全绿；cppcheck 0 错

---

## [2026-08-31] 三审 Wave 4 宣称对账与度量真化 — 收官批（2.2.0 发布）

- **README 四处宣称对账**：看门狗两分法（supervisor=真 / health_monitor
  修复后=真，两条链分开表述）+ 告警如实标注「未实现」；:9091 修正为仅
  AMR_PERF_INSTRUMENTATION=ON 构建存在；fleet_multi 标注单机演示形态；
  部署节降级 roadmap
- **P0-I 覆盖率真分母**：lcov --initial 先捕获全部 gcno 再合并运行时
  gcda——supervisor_node/health_monitor 等零覆盖目标以真实身份进分母。
  **89.3% → 86.7%（文件 54→59）**——下降的是水分：三个零覆盖文件此前
  从分母消失。徽章已更新为 86.7%（这是第一次「下降也自动同步」，
  真分母机制的直接验证）
- **P0-J 徽章新鲜度**：badge push 失败置红（::error::）+ rebase 重试
  （多腿 non-fast-forward 场景）——「静默 4 天」不再可能
- **版本三宇宙对齐**：package.xml 0.3.0 / CHANGELOG Unreleased / tag
  v0.1.0 → 统一 **2.2.0**（带 annotated tag）
- **toolkit Docker 修复**：独立 quality cmake 块（引用已迁移的 test/
  目录，构建必断）→ colcon 一体化构建+测试
- 三审修复序十项至此全部关闭或显式裁决

---

## [2026-08-31] 三审 Wave 3 信任链收口 — 验签绑内容/公钥钉住/install 排私钥

- **P0-D 内容绑定验签**：签名对象从裸版本号 `"amr-ota:v12"` 升级为
  `{version, sha256, size}` 整体（`image_manifest` + `sha256_hex`，OpenSSL
  EVP）。fetch 从恒真桩变为「读镜像文件 → 对实际字节算哈希 → 与签名比对」
  ——版本号不变、镜像被换的场景（旧实现放行）现在必拒。新增
  TamperedImageContent 测试锁定该攻击路径
- **P0-E 公钥出参数域**：`ota.public_key_pem`（运行时可 set 的字符串参数，
  攻击者可同时替换公钥+签名+版本完成"合法"升级）删除 → `ota.public_key_file`
  （构造期读文件一次性钉住 `pinned_public_key_`，运行时参数替换无效）
- **P0-F install 排私钥**：`install(DIRECTORY config)` 加
  `PATTERN "sros2/private" EXCLUDE` + `PATTERN "sros2/enclaves" EXCLUDE`
  ——make install 不再把 CA 三件套/enclave 私钥拷进 share 分发；本机
  keystore 全部私钥 chmod 600。附注：本 keystore 的 identity/permissions
  CA 是同一把 key 的软链（sros2 工具行为）——商用部署需独立双 CA，
  已在 ota-adr 记录为真机前置项
- 测试：test_ota_agent 2→5 例（内容绑定/篡改拒绝/未烧公钥），37 模块全绿

---

## [2026-08-31] 三审 Wave 2 防线机制 — ASAN 进 CI + -Werror 零警告 + TSAN 手册

- **ASAN job 接入 CI**（asan-gate）：cppcheck 对跨函数指针逃逸类 UAF 的
  盲区由 sanitizer 补上（Wave 1 调查结论的落地）；run_tests.sh asan 自带
  编译限幅（WSL OOM 教训），CI 容器内存不受本机约束但纪律统一
- **-Wall -Wextra -Werror 固化**：先摸面（colcon 一次性 build-base 探测）
  ——全仓仅 20 条警告且全部自有代码（外部头零噪声），逐条修毕后整体
  -Werror。四类：KF 未用变量 r3（10×）、同名别名 using Cluster=Cluster
  触发 GCC14 -Wchanges-meaning（8×）、deprecated rmw_qos_profile_t 重载
  （改 rclcpp::ServicesQoS）、测试未用变量
- **TSAN 夜跑手册**（quality/tsan-runbook.md）：demo_grid_（P0-B 嫌疑）
  的捕获规程——独立 build-base、ROS 内噪声判读规则、三个最大竞争窗口
  用例、修复优先级预案
- 验证：常规构建 + 37 模块 100% 全绿（-Werror 激活态）

---

## [2026-08-30] P0-C 重启序列异步化——「从第一天就不可能工作」的看门狗首次真工作

> 三审 P0-C 收官（Wave 1 全项完成）。旧实现三死锁要素：单线程 spin + 定时器
> 回调内 wait_for_service(1s) + 4×future.wait_for(2s)——响应永远轮不到被
> 处理，每个 transition 必超时，重启从未成功过。

- **异步状态机**：begin_restart（零阻塞入口）→ send_next_transition
  （service_is_ready 替代 wait_for_service）→ 响应回调链式推进四步
  （deactivate→cleanup→configure→activate）；客户端挂独立 callback group
  （多线程 executor 可并行）；单飞约束（一次一台，防重启风暴）
- **行为验收（test_health_restart，37 号模块）**：FakeLidar 停心跳（功能性
  死亡，lifecycle 活）→ 监控 ERROR → 四步完成 → 心跳恢复救活。**实测
  触发到完成 1ms**（旧实现 8s 超时后失败）。SingleThreadedExecutor 与生产
  main 同形态——修复不依赖多线程
- **排障三轮的教训入账**（差点误修生产代码）：
  1. 测试首版用裸 sleep 等发现——rclcpp 定时器只在 spin 时触发，睡眠期
     零心跳发布，STALE(never-seen) 永不升级 ERROR，重启自然不触发。
     隔离自检+二分+探针三轮定位后才确认毒在测试自身
  2. 排障脚手架的正则清理吞掉 return → 非 void 无返回 UB → SIGILL，
     gdb 一发定位
  3. 顺带发现（未修，记账）：HeartbeatAnalyzer 的 STALE=never-received
     永不升级 ERROR——冷启动即死的节点永远不被重启（审计 P1 族，
     与 B7 冷启动门控同族），Wave 2 后单独裁决

---

## [2026-08-30] 三审 Wave 1 安全兜底链批次 + cppcheck 失明调查 + WSL OOM 事故

> 执行方案：docs/design/20260830-audit3-execution-plan.md Wave 1（1.1/1.3/1.4/1.5）。

- **P0-A UAF 根修**：perception_service `lidar_ranges_` 裸指针（指向 tick()
  栈局部）→ `std::array` 值语义拷贝。验证门双证：旧代码 ASAN 二进制实报
  `stack-use-after-return in ClusterDetector::detect`；修复后 ASAN 全套件
  36/36 零报告。
- **cppcheck 失明调查**（为何 CI 静态扫没抓到）：
  1. 工具能力边界——「局部地址→成员指针→跨函数消费」类逃逸，enable=all +
     全 include 解析实证零发现（静态分析对这类 UB 天生短板）
  2. 门禁自身缺陷——cppcheck 调用带 `|| true` 吞工具崩溃、只门 error 级
  3. 有效防线是 ASAN——此发现直接成为 Wave 2「ASAN 进 CI」的理据；
     附带教训：GCC 无 -fsanitize-address-use-after-return 选项，
     报告名是 stack-use-after-**scope/return**，grep 模式别写窄
- **门禁硬化**：static_analysis.sh 的 cppcheck 崩溃/内部错误不再静默（红）
- **P0-G fail-open 默认反转**：min_valid_echoes 默认 0→50（真机关检测）；
  空旷/仿真显式设 0 豁免；测试 kParams 显式钉 0（几何语义与盲检测解耦），
  旧「默认直行」测试改造成「显式豁免路径」
- **P0-L 饿死修复**：motor_ctrl_main 单线程 spin → MultiThreadedExecutor
  （callback group 隔离在独立可执行形态下真正生效）
- **P1 入口校验**：decision goal_pose NaN/Inf 拒绝（防 static_cast<int> UB）
  + motor action goal 非有限值 REJECT（防 NaN twist 上 /cmd_vel）；
  NaN 测试走真实订阅路径（不 friend 破坏封装）
- **WSL OOM 事故（操作侧教训入账）**：被中断的 ASAN 全量重建子进程链未被
  杀，孤儿 cc1plus 12 路并行（单实例 2.1GB）撞穿 WSL 22GB → 全局 OOM 杀掉
  IDE 桥接进程（dsh.service, oom_score_adj=100）→ vscode/zcode 断连 → VM
  重启。journalctl -b -1 内核日志铁证。**纪律固化**：run_tests.sh asan 模式
  内置 CMAKE_BUILD_PARALLEL_LEVEL=4 + 负载命令一律 nice；重跑验证「构建前后
  可用内存持平 20GB」实证有效
- **验证**【铁证】：全套件 36/36（一次误报批次为 shell 相对路径 sourcing
  错误——DDS 测试速死签名即刻定位）；ASAN 门零报告；cppcheck errors=0

---

## [2026-08-30] OTA 真签名（§8.3-2）— 恒真桩替换为 ed25519 fail-closed 验签（§8.3-2）— 恒真桩替换为 ed25519 fail-closed 验签

> 审计 P1-b「签名校验 /*signature_valid=*/true 恒真」——三条安全不变量
> 之前的信任门从未真正闭合，本次闭合。

- **domain/ota/package_signer.hpp**（OpenSSL EVP，纯 domain）：ed25519
  detached 签名/验签/密钥生成；被签对象为规范串 amr-ota:v<version>；
  6 例单测含全部 fail-closed 反例（篡改载荷/换公钥/翻签名/坏 b64/坏 PEM/空输入）
- **agent 接线**：新参数 ota.public_key_pem（设备烧录）+ ota.target_signature
  （随版本送达）；on_param_change 先验签再 run_update——缺失/错误/未烧公钥
  三种路径全部走 REJECTED_SIGNATURE，槽位零触碰（test_ota_agent 扩至 4 例）
- **既有 2 例改造**：合法签名注入（每例独立密钥对，模拟真实信任链）
- ADR 增补交付侧 openssl 签发示例（原始 Ed25519 与 PackageSigner 互通）
- 全套件 36 模块 100%

---

## [2026-08-30] DDS security 真启用（§8.3 头名）— 四段断言实证，认证真的在挡人（§8.3 头名）— 四段断言实证，认证真的在挡人

> 审计遗留头号缺口：「DDS security 从未跑通」。本次以可复跑脚本证明启用
> （设环境变量不算——野节点被拒、DENY 连入域都不行才算）。

- **排障三发现（均为 strace/实证级）**:
  1. **enclave 必须走 ros args**：`--ros-args --enclave /<name>`——仅设
     ROS_SECURITY_* 环境变量（含 ROS_SECURITY_NODE_NAME）时 fqn 不拼入
     安全目录路径，参与者报 couldn't find all security files。这是
     system_secure.launch.py 从未工作过的真正原因
  2. `create_enclave` 自带已签 governance + 全通行 permissions（rt/*，仅
     domain 0）——认证材料开箱即得；授权收窄才需自签 permissions
  3. `create_permission` 的 schema 校验在本环境解析失败（连自家模板都过
     不了）；DENY 用 openssl CMS 手工签名等价替代（FastDDS 实测接受）
- **`scripts/verify_dds_security.sh`（四段断言，自含临时 keystore）**:
  A 认证互通（合法 enclave 加密通信）✅ B 野节点被拒（无凭证 0 条）✅
  C 真实栈（scene_simulator Enforce 下发布 /odom）✅
  D 授权拒绝（DENY permissions → Participant is not allowed，连 join 都不行）✅
  —— 首轮即 PASS=4
- **system_secure.launch.py 修通**：五个节点全部补 `--ros-args --enclave`
  （compute_container 单进程单身份用 /fusion）；keystore 的 install 侧
  拷贝语义（重生成需 colcon build 刷新）写入头注
- **遗留**: 生产 compose 的安全变量注入（真机部署时一并）；D 段的手签
  依赖本机 openssl（脚本内已自含）

---

## [2026-08-26] D1 第三方扩展验收：19.5min/8 摩擦 → 修复 → 8m48s/2 摩擦一次全过

> 迭代 2 D1——「可扩展」唯一无法自证的词，用两个干净上下文的 AI 子代理
> 扮演只凭文档的第三方用户实测。报告：docs/design/20260826-d1-extension-acceptance.md。

- **一测**（19.5min，超声波+节点）：零改框架（CRTP 模板干净可用），但 8 摩擦：
  3 份指南引用不存在路径、宏签名文档错、**AMR_REGISTER_SENSOR 宏 token 粘贴
  字符串字面量根本编不过**（审计 P1-b 同项）、**静态库自注册对象被链接器
  静默丢弃**（审计未发现——内置传感器靠显式 register_builtin_sensors 绕开，
  第三方无入口）、INDEPENDENT_NODES 无文档、sensors.yaml 无人加载、
  CLAUDE.md 指标托管过度承诺
- **修复**：宏修为 __LINE__ 拼名+used 属性（树内 ultrasonic 注册 TU 实际
  使用进回归）；registry.hpp 确立两条注册路径（幂等显式注册=静态库下唯一
  可靠）；6 处文档对账；sensors.yaml 加真相头
- **二测**（8m48s，温度+驱动节点+消费节点+e2e 测试+双进程冒烟，范围翻倍）：
  2 摩擦（ctest 前置/消费方无范本，当场修复入库）、编译测试冒烟**全部一次
  通过**、35/35 无回归——一测的链接器坑零时间浪费，修复生效直接实证
- **产物转正**：ultrasonic（发布方范本）+ temperature/temperature_monitor
  （消费方范本）全链入库，guides/11 与 CLAUDE.md 指向
- **「X 分钟接入」成为数据**：修复前 19.5min/8 摩擦 → 修复后 8m48s/2 摩擦，
  时长减半且范围翻倍

---

## [2026-08-26] A5 断言式 e2e 落地：三场景全绿 + 挖出 4 个真 bug（审计行动④）

> 验收：「车真动、到点真停、遇障真绕、断源真降级恢复」——此前全项目零断言式
> e2e。载体：scene_simulator 合成闭环（无 Gazebo，CI 容器可跑）+ compute 管线
> 同款组合；`quality/src/test_e2e_behavior.cpp`（IT-04/05/06 + IT-08 + IT-07 族）。

- **e2e 开发过程挖出的 4 个真 bug（全部修复 + 复验）**:
  1. **到点停不下（PurePursuit）**：近目标固定 0.1 m/s 爬行 + goal_tolerance
     0.05 < A* 路径终点量化误差（0.05 格 → 胞心偏移 ≤0.035）→ 死区边缘绕圈
     永不定格。实测到达后 0.275m/3s 环绕漂移。修复：容差 0.05→0.10 +
     爬行速度随剩余距离收敛（v ≤ dist，单调进死区）
  2. **快照丢戳（perception_service）**：lidar_snapshot() 不拷 stamp_ns →
     fusion StampGate 拿到默认 0 走「内部合成=now」分支——**新鲜度门对
     适配器路径完全失明**（旧帧永远冒充现在）。修复：快照透传上游戳
  3. **缓存帧判活缺失（sick 适配器）**：read() 只要 latest_msg_ 存在即
     true → 断源后降级年龄永不增长。修复：到达时间判活（steady_clock，
     1s 窗）——真驱动死 = 无新 DDS 消息到达；含复活复验单测
  4. **降级冻结（fusion）**：evaluate_degradation() 在 stamp gate 的
     return 之后 → 抑制发布的同时降级状态也冻结在 FULL，心跳继续报健康。
     修复：评估前移到 gate 拒绝之前
- **记录在案的 finding（未改，留调参决策）**：A* inscribed 0.55 < 物理不碰撞
  界 0.57（盒半宽 0.25 + 车外接 0.32）——planner 理论上可规划进碰撞区 ~2cm；
  e2e 实测绕行 0.601m 通过 0.57 断言，余量 3cm。建议后续 inscribed 提 0.60
- **配套**：SceneSimulatorNode 增 NodeOptions 构造 + pause/resume 故障注入
  API（soak 断源注入可复用）；scene_simulator_node.cpp 挪入 lib（e2e 与
  可执行共用）；e2e 显式 TIMEOUT 300（默认超时在 ~105-115s 实测时长上贴边
  抖动，曾致偶发 Timeout 假红）
- **验证**【铁证】：三场景 OK（25.7s/49.2s/29.2s）；全套件 33 模块 100%
- **回滚**：git checkout 本条涉及文件（4 个 bug 修复分属
  pure_pursuit.hpp / perception_service.hpp / sick_tim781_adapter.hpp /
  fusion_node.cpp + 测试两新文件）

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
> **复盘**：五个 bug 的模式提炼见 docs/design/20260825-b1a2-five-bugs-retrospective.md。

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
