# 复审整改方案（ISO 方法论 · 2026-09-01）

> 输入：`docs/design/20260901-external-l7-reaudit.md`（三路独立核验，
> 综合 6.0/10）。本文档按「先核验、再分波、每波带验证门」的 ISO 流程输出，
> 逐条对应复审 §8 剩余硬门槛。
> 原则：**每一句"已修复"都是一条待证伪的假设**——复审 P0-I 的五处虚假
> "已验证"叙事是本波要根治的元问题。

## 一、核验结论（先验证再审）

| 指控 | 核验方法 | 判定 |
|------|---------|------|
| P0-G 方向修反 | `grep guard_min_valid_echoes motor_ctrl_node.cpp:38` 默认 0 | **属实** |
| P0-I 机制零效果 | `grep supervisor_node coverage_full.txt` = 0 条 | **属实** |
| P0-B 未修未裁决 | `demo_grid_` 的 raytrace/plan 调用点无锁（goal_mutex_ 只保护 goal 字段） | **属实** |
| v2.2.0 tag 不可达 | `git merge-base --is-ancestor v2.2.0 HEAD` 失败；describe=v0.1.0-260 | **属实** |
| system_secure launch 必坏 | install EXCLUDE 了 launch 引用的 keystore 路径 | **属实**（代码逻辑推演） |

## 二、整改波次

### Wave R1 · 度量诚实最小闭环（复审最重发现，最先修）

**问题本质**：不是 lcov 语法问题——是「验证自己宣称的纪律忽然低于验证代码的
纪律」。`--initial` 写进了五处"已验证"叙事，却没有人跑过最便宜的
`grep supervisor_node coverage_full.txt`。

| # | 项 | 修法 | 验证门 |
|---|----|------|--------|
| R1.1 | initial capture 修通 | 排查失败根因（可能：coverage 构建先 clean 了 gcno → initial 时无文件）；修通或诚实降级 | **supervisor_node.cpp 以 0% 出现在 coverage_full.txt** |
| R1.2 | 80% 门禁 vs 真分母冲突裁决 | 若真分母 <80%（supervisor 0% 拉低），三条路：① 补 supervisor 单测到不拉闸 ② 分母豁免（显式注释+审批） ③ 降门禁（如实写理由）——不允许静默维持假分母 | 门禁逻辑与分母数学自洽 |
| R1.3 | `|| true` + `-s` 空文件跳过 | 删 `|| true`；initial 失败 = 红（与 :33 原则一致） | 故意去掉 initial 让它失败 → CI 红 |
| R1.4 | fleet_manager 排除缺注释 | 排除行补一行理由 | 无 |
| R1.5 | 五处虚假宣称更正 | journal 补 P0-I 误宣称更正记录；CHANGELOG 2.2.0 节补更正注记；quality.sh 注释改为真话 | 更正记录入库 |

### Wave R2 · P0-G 方向反转 + N-1（真机安全的最后一格）

| # | 项 | 修法 | 验证门 |
|---|----|------|--------|
| R2.1 | motor_ctrl_node 默认翻转 | `declare_parameter("guard_min_valid_echoes", 50)` + 注释改"fail-safe default (real hw)" | **默认参数场景测试**：无覆盖 → 传感器失明 → 硬停 |
| R2.2 | simulation.launch 确认 | 已显式 50 ✓ 无需改 | — |
| R2.3 | domain 注释口径 | collision_guard.hpp 注释与 motor 一致化 | — |

### Wave R3 · P0-B 实修 + TSAN 进防线

| # | 项 | 修法 | 验证门 |
|---|----|------|--------|
| R3.1 | demo_grid_ 加专用锁 | `mutable std::mutex grid_mutex_` 保护 raytrace/inflate/plan 三个写入/读取点（goal_mutex_ 模式复用） | TSAN 跑 decision 用例零自家帧报告 |
| R3.2 | 锁粒度论证 | 5Hz perception 写 + 0.5Hz plan 读——锁竞争窗口微秒级，粗粒度锁足够（不搞双缓冲） | — |
| R3.3 | TSAN nightly 进 CI（可选 job） | workflow_dispatch + schedule 触发，只跑 decision + motor（最大窗口），不进阻塞链 | 首轮跑完出报告入 journal |

### Wave R4 · N-2/N-3 + P0-K + tag + 发布叙事

| # | 项 | 修法 | 验证门 |
|---|----|------|--------|
| R4.1 | system_secure launch install 路径 | launch 读 source 侧 keystore（get_package_share_directory 改 source 路径注入），或 install 含 public/ 排 private/（最小可用） | verify_dds_security.sh 绿 |
| R4.2 | 重启序列超时 | send_next_transition 加 deadline（如 10s/步）；超时 → 放弃重启 + WARN | 慢响应模拟测试 |
| R4.3 | P0-K 显式裁决 | ARCHITECTURE.md 数据流表加一句话注：「fusion 默认进程内实例化传感器（仿真形态）；独立传感器进程 + DDS 话题消费 = system.launch 显式配置（真机形态）」 | 文档检查 |
| R4.4 | v2.2.0 tag 重打 | `git tag -fa v2.2.0 HEAD`（force 更新到可达 commit）+ push --force tag | `git merge-base --is-ancestor v2.2.0 HEAD` ✅ |
| R4.5 | CHANGELOG 补 Wave 1-3 安全修复条目 | 2.2.0 节合并双节 + 补安全修复条目 | CHANGELOG 结构检查 |
| R4.6 | 防降级计数器持久化 | security_counter 写文件（同 shell 演示脚本逻辑） | 重启后计数器不变测试 |
| R4.7 | keystore 生成脚本 0600 | scripts 或 verify 脚本里 chmod 0600 自动化 | 脚本检查 |

### Wave R5 · compose/依赖（部署链补洞）

| # | 项 | 修法 |
|---|----|------|
| R5.1 | compose 可执行名修复 | fusion_node/decision_node/motor_ctrl_node → compute_container |
| R5.2 | package.xml 补 8 依赖 | 对照 launch/CMake 实际引用逐项补 |
| R5.3 | rosdep 重建验证 | 清 install → `rosdep install --from-paths src` → build → test |

## 三、不排期（显式裁决）

- **P0-H 硬件安全回路**：维持前轮裁决（真机/认证语境前置）
- **DDS security 默认开启**：等真机验证再默认开——当前 verify 脚本可复验，
  默认开对开发迭代摩擦过大（复审 §8.8 的"或显式风险裁决"选后者：
  风险已评估，真机部署时打开）

## 四、验证纪律（本波新增的元防线）

每项修复的验证门**不得引用自身产物**（P0-I 的教训：commit message 里
"已验证" ≠ 验证）。必须满足：
- 命令级：`grep / git merge-base / ctest` 的输出是证据
- CI 级：让 CI 的 job 自身产生红/绿信号
- 反例级：能描述"如果不修会怎样红"的测试

## 五、波次执行顺序

```
R1 度量诚实（最先——它定义"修复"的可信度） → R2 P0-G（一行+测试，真机安全）
→ R3 P0-B（锁+TSAN，代码量最大） → R4 tag/N-2/N-3/P0-K（收尾杂项）
→ R5 compose/依赖（部署链）
```
