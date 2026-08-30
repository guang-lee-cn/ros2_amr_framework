# 外部 L7 工程审计报告 — ros2_amr_framework

> 审计基准：HEAD = a2abc70（2026-08-30），270 commits，单人 10 周
> 口径：只认代码事实与正式文档（doc/ + docs/design/ + README + CHANGELOG + contracts/）；mdDoc/（gitignore）与 TODO/注释承诺不计入；每条发现带 file:line
> 方法：四路独立审计（测试质量 / 并发内存 / 可观测+安全工程 / 治理+落地距离）交叉汇总；已知仓库内嵌过一轮自审（ITERATION.md 08-25 初审 6.5 → 08-28 复审 7.5），本轮对其实施独立核验

---

## 1. 总判词

**"能证明在仿真里是对的，不能证明在车间里五年不死。"**

以单人 10 周为分母，这是一个罕见的诚实高执行力原型：domain 分层红线真实守住（36 头文件零 ROS 泄漏）、Domain 层测试教科书级（事故回归锁定、数值断言到 ±1ns）、e2e 断言式闭环、自审计文化（D1 教训写在门上、泄露事故有完整响应闭环）。

以工程标准为分母，结论严重得多：**全部系统性风险恰好聚集在安全兜底链上**——感知主链路存在每天都在触发的 stack-use-after-return；watchdog 重启机制结构性死锁（从第一天起就不可能成功）；独立进程形态下碰撞保护被旁路；OTA 验签保护的是版本号字符串而非镜像内容；真机默认参数对"传感器发疯"fail-open。同时宣称与实际的落差集中在最不该有落差的地方：README 的"看门狗重启 / Prometheus 告警 / 多 AMR 集群"，一半是纸面。

一句话：**设计与单点工程质量已过 Design Review；过不了任何一家工业客户的供应商审核，也过不了自己第一条真机产线的安全评审。**

---

## 2. 成熟度评分卡

| 维度 | 分 | 判据 |
|---|---|---|
| 架构与分层设计 | 8 | DDD 四层真实落地；domain 纯度零泄漏；时间戳/零拷贝/QoS 词汇表等 ADR 有执行力 |
| Domain 算法与单测 | 8.5 | CollisionGuard fail-safe 矩阵全断言、KF Joseph 1e-12、StampGate ±1ns、OTA 反例 fail-closed；事故驱动回归 |
| Infrastructure 并发/内存安全 | **3** | 3 个 P0（UAF/数据竞争/结构性死锁）；-Werror 无、sanitizer 全无、TSAN 一晚能抓的竞态没有任何自动防线 |
| 安全工程（OTA/DDS） | **4** | 验签内核真（EVP fail-closed + 6 单测）；信任链四周漏：不绑内容、公钥可运行时替换、私钥 0644 随安装分发 |
| 可观测性 | 5 | metrics 柱真（14/15 字段有生产写入者、跨 7 进程 SHM）；trace 100% 死代码、LOG_OBS 零调用、日志无轮转 |
| 测试工程体系 | 5.5 | 断言质量高但覆盖率分母漏零覆盖文件（"真数假全"）；CI 徽章断链 4 天无人发现 |
| 文档-代码一致性 | **4.5** | 核心数据流表不成立（默认 launch 下传感器话题无消费者）；hal 纯度宣称为假；版本三宇宙 |
| 发布与治理 | **3.5** | 契约"法律文件"零 CI 强制力且自身最陈旧；toolkit 构建已断；package.xml 漏一半运行时依赖 |
| 部署运维就绪 | **2.5** | 无 systemd/告警规则/远程通道；配置零环境分化（params.yaml 0 字节）；OTA 物理层是 /tmp 模拟 |

**综合工程成熟度：约 5.5/10（原型末期）；组织级治理：CMMI L2 + L3 孤岛。**

---

## 3. P0 清单（跨路合并，按爆炸半径排序）

### 3.1 现场会炸（并发/内存，路 2）

| # | 发现 | 证据 |
|---|---|---|
| P0-A | **感知主链路 stack-use-after-return**：tick() 内栈局部 LidarScan 的 ranges 指针被存入成员 `lidar_ranges_`，消费点全在 tick 返回后——发布的 /perception/objects 是"幸存字节"，现在能跑纯靠栈布局运气，换编译器/-O 级别即静默数据损坏 | perception_service.hpp:47,69,101,128；fusion_node.cpp:211-292 |
| P0-B | **demo_grid_ 跨线程数据竞争**：生产入口 MultiThreadedExecutor + 五订阅同挂 Reentrant 组，A* 唯一世界模型 demo_grid_ 零锁（gate_ 有锁——作者知道 Reentrant 要锁，唯独漏了最大的 grid）；goal 被堵时 A* 逼近 200ms 与 5Hz perception 重叠 | decision_node.cpp:44,184,211,243；compute_container.cpp:73 |
| P0-C | **health_monitor 重启序列结构性死锁**：单线程 spin + client 默认组与定时器同互斥组 + 回调内 future.wait_for(2s)——每个 transition 必超时，lifecycle 四步重启从第一天起不可能成功，机器人带着死节点继续跑 | health_monitor_main.cpp:8；health_monitor_node.cpp:145-270 |

**交叉裁定**（三、四路曾判"看门狗重启属实"）：supervisor 进程级 kill/respawn（独立进程、有 1.0s 实测数字）是真的；health_monitor 的 lifecycle ChangeState 重启是死的。README 宣称链路指的是后者。两个"看门狗"必须分开表述。

### 3.2 安全兜底失效（安全工程，路 3）

| # | 发现 | 证据 |
|---|---|---|
| P0-D | **OTA 验签不绑定镜像内容**：签名对象仅 `"amr-ota:v<版本>"` 字符串；fetch 是桩直接返回 true——合法签名 + 任意恶意内容可进槽 | package_signer.hpp:29-32；ota_agent_node.cpp:80-83 |
| P0-E | **信任锚可运行时替换**：公钥是 ROS 参数，默认 launch 无 DDS security——攻击者可同时 set 公钥+签名+版本完成"合法"升级 | ota_agent_node.cpp:28-29,107-112 |
| P0-F | **私钥 0644 且随 make install 分发**；identity CA 与 permissions CA 同一把 key 软链 | config/sros2/private/；CMakeLists.txt:157-158 |
| P0-G | **真机默认 fail-open**：min_valid_echoes 默认 0 关闭全盲检测——"按时发布但全 inf"（正是 08-17 撞货架模式）默认参数直行通过 | collision_guard.hpp:66-67,185-190 |
| P0-H | **无硬件安全回路接口**：全仓 e-stop/safety_scanner 零命中；软件 guard 只吃 /scan 单一来源 | motor_ctrl_node.cpp:79,196 |

### 3.3 度量与宣称失信（测试/治理，路 1、4）

| # | 发现 | 证据 |
|---|---|---|
| P0-I | **覆盖率 89.3% 分母漏零覆盖文件**：supervisor/health_monitor/fleet_manager/prometheus_server 从未进任何测试，"0%"被表达成"不存在" | coverage_full.txt 54 文件；CMakeLists.txt:49,122 |
| P0-J | **CI 徽章断链 4 天**：最后徽章提交 08-26，HEAD 08-30；a2abc70 宣称"36 模块 100%"仓库内无工件可复算——违反自家"以 CI 实测为准" | badge.json mtime；ci.yml:19 已知 push 丢失无告警 |
| P0-K | **核心数据流表不成立**：默认 launch 下 fusion 进程内实例化仿真传感器，传感器节点话题无消费者；"PID 故障隔离"叙事装饰性 | system.launch.py:55-60；fusion_node.cpp:82-99；ARCHITECTURE.md:151-160 |

### 3.4 条件性 P0（形态依赖，路 2）

**P0-L**：motor_ctrl 独立可执行（交付物之一）用单线程 spin——注释宣称的 callback group 隔离完全失效：execute() 20Hz 循环饿死一切回调 → on_scan 永不执行 → **碰撞保护旁路**（guard 判"boot race pass through"）+ odom 恒无效 → **开环积分**。compute_container 形态无此问题。原报告定 P1，因独立进程是 CMakeLists 显式交付物，本轮升级为条件性 P0。（motor_ctrl_node.cpp:66-70,254-405；collision_guard.hpp:182-190）

---

## 4. P1 精选（全量见各路原始报告）

**并发/UB**：外部 DDS 输入零校验（goal_pose NaN/Inf 直达 static_cast<int> UB；motor 无条件 ACCEPT 后 NaN twist 上 /cmd_vel，max_speed 只进日志）；OTA"原子写"实为 ofstream trunc（写 boot_target 中途掉电引导标记损坏）；MonitoringService 与 HTTP 线程无锁竞态；SHM metrics 未 placement-new 且静默降级无人察觉；decision on_cleanup 漏 reset scan_sub_/goal_sub_（清理后节点可重新带目标跑）。

**安全/可观测**：guard 硬停指令经 smoother 斜坡下降（0.5m/s 多滑 0.63s/0.16m > stop_dist 0.30m）；防降级计数器内存态重启归零（shell 演示脚本反而持久化了，C++ 生产 agent 弱于自己的 demo）；HEALTH_GATE 无超时 watcher，断电变砖不回滚；trace 体系 100% 死代码（AMR_TRACING_ENABLED 无任何构建定义）；日志无轮转无文件 sink，5/6 进程 spdlog 未初始化；:9090/:9091 绑 INADDR_ANY 无认证。

**测试**：fusion 降级阶梯节点级零覆盖（CRITICAL 门控删掉不会红）；motor 3s 反死锁 abort 消费端零测试；两处恒真断言 EXPECT_GE(size(),0u)；SetParamUnknown 断言 success==true 把 API 缺陷锁成契约；cppcheck 只门 error 级且工具崩溃静默通过。

**治理**：版本三宇宙（package.xml 0.3.0 / CHANGELOG 2.1.0 / tag 仅 v0.1.0）；package.xml 漏 8 个 launch 实际依赖（rosdep 无法重建系统）；toolkit Docker 构建必断（引用已迁移目录）；ADR-12 决策"删除"的代码原样保留且文档仍教人用；"hal 零 ROS2 依赖违反则 build 失败"宣称为假（sick_tim781_adapter.hpp:27 include rclcpp）。

---

## 5. 宣称 vs 实际（合并表，关键条目）

| 宣称 | 来源 | 判定 |
|---|---|---|
| 感知-决策-执行全链路自研 | README:9 | **基本属实**——但只在仿真闭环（含 e2e 断言），真机证据为零 |
| 看门狗重启 | README:50 | **半真**——supervisor（进程级）真；health_monitor（lifecycle）结构性死锁 |
| Prometheus 告警 | README:50 | **不实**——全仓零告警规则/Alertmanager，观测"只看不叫" |
| :9090 + :9091 + Grafana | README:108 | **夸大**——:9090 真；:9091 默认构建不存在；dashboard 数据源是占位符 |
| 多 AMR 集群 fleet_multi | README:63 | **名不副实**——launch 只起一台，第二台会话题冲突；fleet_manager 仅被动聚合 |
| 商业部署 Docker+OTA+版本锁定 | README:124 | **纸面**——Docker 构建已断；OTA 物理层是 /tmp 模拟；版本锁定自身三宇宙 |
| 覆盖率 89.3% | badge.json | **真数假全**——单源直算可复算，但分母不含零覆盖文件 |
| ed25519 fail-closed 验签 | commit a2abc70 | **属实**——6 单测含篡改/错钥；但保护对象是版本号非镜像 |
| DDS security 四段断言实证 | commit 08fb519 | **部分属实**——脚本真实可跑，但临时 keystore + demo 节点，默认 launch 零安全 |
| 加传感器零改框架 | README:137 | **属实**——19.5min→8m48s 第三方接入实测 |
| 全套件 36 模块 100% | commit a2abc70 | **不可验证**——无任何工件佐证 |

---

## 6. 落地距离评估

**阶段判定：仿真闭环完备的原型末期（demo-complete prototype），尚未跨过工程样机门槛。**

零真机验证（sick_tim781 适配器从未上硬件）、零长时可靠性数据（72h soak 有 harness 无报告）、零安全工程（ISO 3691-4/IEC 61508 无任何映射与占位）、零多机、部署链断裂。自查差距表 G1-G7 中：G1 已关闭属实、G2 代码完成但动态场景未验证、G3/G5/G6 未推进、G7 部分。

| 目标 | 工程量级（净增） | 主要构成 |
|---|---|---|
| **单机商用试点** | **10-18 人月** | 真机 bring-up 3-6（不可压缩、风险最大）+ 安全软件过程 8-15 中软件侧 + 部署运维 4-6 + 供应链 2-3 + 治理机制化 1-1.5（重叠后区间） |
| 可售量产（含多机+认证叙事） | 25-40 人月 + 认证周期 | 多机交通管制+VDA5050 另 8-15；ISO 3691-4 双通道架构决策必须**前置**，越晚返工越大 |

自估（2-3 个月全职）漏算的是安全软件过程、供应链审计、运维工具链的文档/验证侧——那不是写代码的时间。

---

## 7. 修复优先序（前 10 项）

1. **P0-A** PerceptionService 改值语义（成员 std::array 拷贝或 tick 返回快照）——一处改动消除最大 UB
2. **CI 加 ASAN+TSAN job**——没有它，下次"修好"可能只是把 UB 挪个位置（ASAN 一秒抓 P0-A，TSAN 一晚抓 P0-B）
3. **P0-C** 重启序列移出定时回调（独立 callback group + 异步状态机），否则删掉恢复功能别假装有
4. **P0-B** demo_grid_ 加专用锁或双缓冲；或论证 5Hz 无积压后降级 MutuallyExclusive
5. **P0-G** min_valid_echoes 真机默认反转（fail-safe default），空旷场景部署侧显式豁免
6. **P0-L** motor_ctrl 独立可执行要么改 MultiThreadedExecutor，要么从交付物删除
7. **P0-D/E** OTA manifest 签 {version, sha256, size} 整体；fetch 桩换真下载+哈希复验；公钥出参数域
8. **P0-F** 私钥 0600 + install 排除 sros2/private + 双 CA 分离
9. **P0-I** 覆盖率分母强制含零覆盖文件（lcov initial capture）；**P0-J** 徽章新鲜度告警
10. **P1-4** goal_pose/motor goal 入口 isfinite+量程校验（NaN 现在能直达 cmd_vel）

---

## 8. 结语

这个仓库最可贵的是"不装"的文化——D1 教训写在门上、泄露事故完整响应、verify 脚本用断言代替自欺。最危险的是**宣称与实际的落差恰好聚在安全兜底链**：出事时你会去查的那些机制（看门狗、碰撞保护、验签），一半在当前形态下不工作。作者完全具备逐项关掉缺口的能力（supervisor 线是证据：ADR→实现→单测→实测数字→CHANGELOG 全程闭环）；缺的不是能力，是**把"一个人的闭环"变成"仓库的机制"**——契约强制力、版本单一事实、sanitizer 防线、覆盖率真分母。

先修安全兜底链，再谈商用距离。

---

**审计分工**：测试质量 / 并发内存 / 可观测+安全工程 / 治理+落地距离，四路独立取证后交叉汇总
**与自审（ITERATION.md 6.5→7.5）的关系**：整改真实项经核验属实；复审自认未动项（版本对齐/soak/systemd/告警）经复核确认全部未动；本轮新增发现集中在自审未覆盖的并发 UB 与安全信任链维度
**文档版本**：1.0（2026-08-30）
