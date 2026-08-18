# ISO 战略输出

> 生成日期：2026-08-01
> 模式：重构
> 下游：strategy.md（架构设计阶段）
> 主驱动力：C1（质量门禁闭环）→ C3（商业化落地）

---

## 重构前置：现状评估摘要

- **架构**：DDD 四层（hal/domain/infrastructure + observability 横切），6 进程部署（传感器独立故障隔离 + compute 单进程零拷贝），控制闭环自研完成（感知→障碍标记→A*→平滑→PurePursuit→误差监控）
- **量化痛点**：无豁免红线 3 处（health_monitor_node.cpp 379 / fusion_node.cpp 275 / motor_ctrl_node.cpp 264）；测试数文档三方矛盾（README 98 / ITERATION 14 / quality README 52 vs 实际 110）；命名 24/110 达标；test_motor_ctrl 被 CI 排除；覆盖率 78.8%<80%；clang-tidy/ASan 未接入；包名 ros2_robot_middleware vs 目录 ros2_amr_framework 漂移
- **健康资产（不动）**：domain/ 全部算法、hal/ Registry 插件注册、observability/ 全套、lidar/imu/camera/fleet 节点、DDS 选型与文档体系
- **触发原因**：代码已跑通但门禁未闭环，"宣称生产级，门禁跑不满"

---

## Insight 结构化记录

| 维度 | 结论 |
|------|------|
| 触发事件 | 代码已跑通（P0-P3 全完成、CI 绿），但质量门禁未闭环：3 处无豁免红线、测试命名 86/110 违规、test_motor_ctrl 被 CI 排除、覆盖率 78.8%<80%、clang-tidy/ASan 未接入。"宣称生产级，门禁跑不满"是现在必须做的原因；C3 是下一坡：P3e 真实硬件缺口全部挂起 |
| 用户/客户 | 真实产品/团队，且以单人开发者身份按产品标准自建。核心诉求：门禁可信、代码可被新成员接手、可落地到真实硬件。附带保留开源参考架构价值 |
| 当前做法 | 现状即痛点：红线违规 3 处、文档与代码漂移、测试命名 24/110 达标、集成测试未提交、静态分析仅 cppcheck、DDS benchmark 方法论已有但硬件落地为零 |
| 不做会怎样 | 无法宣称生产级；新成员无法信任文档数字；真实硬件接入时（P3e）质量缺口放大；作为"可落地产品"的论证不成立 |
| 长期愿景 | 生产级 AMR 参考架构 + 可落地产品：门禁可执行（CI 全绿、零红线、命名统一、覆盖率达标、ASan/clang-tidy 接入），且控制/感知/执行闭环能跑在真实硬件上（IActuator + 真实传感器） |
| 竞品/参考 | MiR/OTTO/海康自研导航栈（方向对标）；ros2_control（执行器 HAL，D2 决策不引入，IActuator 保留适配空间）；NAV2 不引入。质量工程对标 Google C++ Style + ISO 25010 + 五条铁律 |

---

## Strategy 终稿

### 一句话定位

**生产级 AMR 参考架构** —— 单人开发者以产品标准自建，质量门禁可执行、能落真实硬件的 ROS2 感知-决策-执行全链路自研方案。

### 核心目标（SMART）

| # | 目标 | 衡量标准 |
|---|------|---------|
| G1 | 质量门禁闭环（C1） | ① 零无豁免红线（check_limits CI 阻塞）② 测试命名 100% `TEST_F(ClassName, Given_When_Then)` ③ 测试全绿含 test_motor_ctrl（CI 取消排除）④ 覆盖率 ≥ 80% ⑤ clang-tidy + ASan CI 零警告 |
| G2 | 文档-代码单一事实源 | 测试数/覆盖率由 `quality.sh` 自动报告，文档不再手写数字 |
| G3 | 可落地验证（C3） | ① Docker compose 一键跑通仿真闭环 ② IActuator 接口+模拟实现单测覆盖契约，真实底盘就绪后 1 天接入 |

### 边界

| ✅ 做 | ❌ 不做 |
|------|--------|
| C1 全部：红线/命名/测试修复/覆盖率/静态分析 | NAV2 与 ros2_control（维持 D1/D2 决策） |
| test_motor_ctrl 修复 + test_control_loop 提交 | DDD 4→2 层迁移（C2 未选，维持现状） |
| 包名/目录名漂移决策（P7） | 多 AMR 集群调度完整实现（FleetManager 骨架） |
| C3：Docker 化 + IActuator 接口与模拟实现 | 功能安全 ISO 13849 / SLAM 自研 |
| 可离线验证的部分（不依赖真实硬件） | 真实硬件本体调试（依赖硬件到场，单列） |

### MVP 范围

C1 门禁五项全绿 → 集成测试（test_control_loop）通过 → Docker 镜像跑通 Gazebo 仿真闭环 → IActuator 接口+模拟实现单测通过。

### 成功标准

| 标准 | 验收方式 | 验收人 |
|------|---------|--------|
| 门禁五项 | CI 全绿（含此前排除项），check_limits/命名 lint 阻塞 | 用户自评 + 可选外部评审（开源 PR） |
| 单一事实源 | 文档数字与 quality.sh 报告一致 | 用户自评 |
| Docker 落地 | compose up 无手工步骤跑通 | 用户自评 |
| 硬件就绪度 | IActuator 模拟单测覆盖契约；硬件到位后剩余工作仅硬件绑定 | 用户自评 |

### 关键假设

| 假设 | 风险 | 验证 |
|------|:---:|------|
| 真实硬件可获得 | 中 | C3 拆"离线可验证"/"依赖硬件"两半，硬件不到不阻塞 G3 |
| WSL2 Docker 可跑通 | 中 | MVP 首日验证，失败降级为脚本+文档 |
| ASan 在 CI 无 DDS 噪音 | 中 | 扩展 lsan.supp |
| 单人自建节奏可持续 | 低 | 上一轮每日节奏已运行 4 周，无变化 |

---

## 盲点检查结果

| 维度 | 结论 | 处理 |
|------|------|------|
| 关键依赖方 | 真实硬件（底盘/传感器）何时到位不确定 | C3 拆两半：离线可验证部分不阻塞；硬件依赖部分单列 |
| 存量数据/兼容性 | 184 commits 文档复用 80%+；包名 rename 影响 README/CI/launch | rename 决策先行、执行放 P4（测试全绿后） |
| 合规与安全 | NA（无隐私/监管；硬件安全回路用户自担） | — |
| 用户真实需求 | 若砍一半，保留 G1 门禁四项 + G3 Docker，砍 ASan（可后补） | 优先级已内化到阶段划分 |
| 决策者/执行者分离 | 单人开发者，决策=执行=验收；按产品标准自建，节奏自主 | 机器验收（CI）为主，外部评审可选 |
| 失败回滚机制 | 门禁逐项加、每项独立 commit 可 revert；CI 分 job 互不阻塞 | 阶段内落地 |

---

## Operation 规划

### 阶段划分（单人，~6h/天 × 6 天/周，总 ~2-3 周）

| 阶段 | 内容 | 门禁 | 对应目标 | 预估 |
|------|------|------|:---:|:---:|
| **P1 门禁地基** | 包名漂移决策先行（不改代码）→ check_limits.sh + 命名 lint 接入 CI（阻塞）→ 红线清零：health_monitor(379)/fusion(275)/motor(264) 三处 SRP 拆分 | check_limits 零违规、命名 lint 通过 | G1①② | 2.5d |
| **P2 测试全绿** | test_motor_ctrl 修复（timer-based stepping）→ test_control_loop 提交并通过 → 补测试至覆盖率 ≥80% | 测试全绿、覆盖率达标 | G1③④ | 3d |
| **P3 静态分析收口** | clang-tidy 接入（新代码零警告 baseline）→ ASan CI job + lsan.supp 扩展 → `quality.sh` 自动生成测试数/覆盖率报告，README/ITERATION 改引用 | ASan 单测通过、报告自动生成 | G1⑤ G2 | 2d |
| **P4 Docker 落地** | 执行包名 rename → Dockerfile + compose + Gazebo 仿真闭环 | compose up 无手工步骤 | G3① | 2d |
| **P5 硬件就绪** | IActuator 接口 + 模拟实现单测（契约验证）→ 文档/README 同步收尾 | 单测覆盖契约、文档数字与报告一致 | G3② | 1.5d |

**合计 ≈ 11 天 ≈ 2 周正 + 缓冲 = 3 周**。

### 资源估算

- 人力：1 人（用户本人），~6h/天 × 6 天/周
- 外部资源：无（开源工具链：GitHub Actions / Docker / colcon / gtest / clang-tidy / ASan）

### 风险清单（Top 3）

| # | 风险 | 概率 | 影响 | 应对预案 |
|---|------|:---:|:---:|---------|
| R1 | clang-tidy/ASan 揭出大量存量问题，范围蔓延 | 高 | 阻塞主线 | baseline 模式：新代码零警告，存量问题进独立 backlog，不阻塞门禁 |
| R2 | WSL2 Docker 跑不通（NAT/GPU/权限） | 中 | 阻塞 P4 | MVP 已定义失败路径：降级为部署脚本 + 文档 |
| R3 | test_motor_ctrl 修复陷入 ROS2 timing 泥潭 | 中 | 阻塞 P2 | 限时 1 天，超时改用集成测试覆盖，不无限纠缠 |

### 验收决策者

| 角色 | 职责 |
|------|------|
| 机器验收（主） | GitHub Actions：build + check_limits + 命名 lint + 测试 + ASan + 覆盖率 全绿即通过 |
| 用户自评（辅） | 手工验证 Docker compose 与文档一致性 |
| 外部评审（可选） | 开源 PR / code review，作为产品标准的独立眼睛 |

---

## 交接说明

- 本文件为 `strategy.md` 的唯一输入约束，阶段间不得断链
- 每阶段产出经用户确认后才进入下一阶段（门禁机制）
- 实现中若发现与架构假设冲突，暂停输出"偏差分析"，回退至对应步骤
