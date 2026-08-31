# 第三波审计执行方案（2026-08-30）

> 输入：`20260830-external-l7-engineering-audit.md`（四路独立审计，综合 5.5/10）。
> 本文档 = 判定结论 + 分波执行方案。前两波审计的整改文化延续：每波带验证门，
> 修完必须有机可复算的证据。

## 一、判定结论

**审计可信，指控成立。** 抽查 5 项最重指控全部核实（本仓自查复核）：

| 指控 | 核验 | 结论 |
|------|------|------|
| P0-A 感知主链路 UAF | `perception_service.hpp:69` 栈局部 `lidar_scan.ranges` 赋给成员指针 `lidar_ranges_`，消费点（:101/:128）全在 tick 返回后 | **属实，最重 UB** |
| P0-G 真机默认 fail-open | `collision_guard.hpp:66` `min_valid_echoes = 0`（注释自认 "real hw" 即关检测） | 属实 |
| P0-L motor_ctrl 独立形态饿死 | `motor_ctrl_main.cpp:8` 单线程 `rclcpp::spin` | 属实 |
| P0-J 徽章断链 | badge.json mtime = 08-27，HEAD 08-30 | 属实（badge 自动提交的 push 在某轮 rebase 后静默失败） |
| P0-C health_monitor 重启死锁 | `health_monitor_main.cpp:8` 单线程 spin + 回调内同步 wait | 属实（结构性，从未成功过） |

**与前两轮评分的关系**：不矛盾——维度不同。初审/复审看「承诺兑现度」（6.5→7.5
属实），本轮看「并发 UB 与信任链纵深」（5.5 也属实）。本轮最痛的判定接受：
**宣称与实际的落差恰好聚集在安全兜底链上**（看门狗一半死、验签不绑内容、
真机默认放行）。审计结语公允：能力已证（supervisor 线全程闭环），缺的是
把「一个人的闭环」变成「仓库的机制」。

## 二、执行方案（四波，每波带验证门）

### Wave 1 · 安全兜底链修复（立即，1-2 天）——「出事时会去查的机制必须真」

| # | 项 | 修法 | 验证门 |
|---|----|------|--------|
| 1.1 | **P0-A UAF** | PerceptionService 改值语义：`lidar_ranges_` 裸指针 → `std::array<float, kMaxRanges>` 成员拷贝 | ASAN 全套件零报告 + e2e 回归 |
| 1.2 | **P0-C 重启死锁** | 重启序列移出定时回调（异步状态机 + 独立 callback group）；README 两个看门狗分开表述（supervisor=真 / health_monitor 修复后） | 集成测试：kill 被监控节点 → 四步 transition 全部完成 |
| 1.3 | **P0-G fail-open 默认** | `min_valid_echoes` 默认 0→50；空旷/仿真场景部署侧显式豁免 | 单测反转 + 默认参数场景测试 |
| 1.4 | **P0-L 饿死** | motor_ctrl_main 改 MultiThreadedExecutor | 饿死回归测试（execute 循环期间 scan 回调可达） |
| 1.5 | P1 入口校验 | goal_pose / motor goal 的 isfinite+量程校验（NaN 现在直达 cmd_vel） | NaN 注入测试拒绝 |

### Wave 2 · 防线机制 ✅（2026-08-31 完成）——「否则修好只是把 UB 挪个位置」

| # | 项 | 修法 | 验证门 |
|---|----|------|--------|
| 2.1 | ASAN 进 CI | `run_tests.sh asan` 模式已存在，接进 ci.yml 矩阵（新 job 或第三腿） | 故意埋一个 UAF 验证 job 会红（一次性演练） |
| 2.2 | -Werror（自家代码） | CMake 目标级开启，外部依赖豁免 | 零警告基线 |
| 2.3 | TSAN 手册 | 文档化夜跑规程（CI 可选 job），先抓 P0-B 类 | demo_grid_ 竞态复现报告 |

### Wave 3 · 信任链收口 ✅（2026-08-31 完成）

| # | 项 | 修法 | 验证门 |
|---|----|------|--------|
| 3.1 | **P0-D 验签不绑内容** | manifest 改签 `{version, sha256, size}` 整体；fetch 后哈希复验 | 篡改镜像内容（版本号不变）→ 拒绝，测试锁定 |
| 3.2 | **P0-E 信任锚可替换** | 公钥出参数域：改文件路径参数（root 只读文件）+ DDS security 启用时参数域才可信的说明 | 运行时 set 公钥 → 不生效 |
| 3.3 | **P0-F 私钥分发** | CMake install 排除 `config/sros2/private/`（现在实害：make install 拷私钥）+ 本机 0600 | install 目录无私钥 + verify 脚本仍绿 |

### Wave 4 · 宣称对账与度量真化（半天）

| # | 项 | 修法 |
|---|----|------|
| 4.1 | README 五处宣称 | 看门狗两分法表述 / Prometheus 告警「不实」项改为计划或实现 / :9091 表述修正 / fleet_multi 如实标注 / 商业部署节降级为 roadmap |
| 4.2 | **P0-I 覆盖率真分母** | lcov `--initial` 捕获零覆盖文件，89.3% 将如实下降——接受下降 |
| 4.3 | **P0-J 徽章新鲜度** | badge step 加 pull --rebase 重试 + 失败置红（新鲜度 > 数字） |
| 4.4 | 版本三宇宙 | package.xml / CHANGELOG / tag 对齐为 2.2.0 |
| 4.5 | toolkit/Docker | 修引用或删除（审计：构建必断） |

## 三、明确不排期（本轮）

- **P0-H 硬件安全回路**（e-stop 双通道）：ISO 3691-4 架构决策需真机/认证语境，
  先出占位 ADR 说明边界——写代码反而伪装成熟度
- 多机 VDA5050 / 车队交通管制：25-40 人月量级，商用试点后再议
- **72h soak：排在 Wave 1-3 完成之后**——soak 一个带 UAF 的栈测的是运气

## 四、与既有清单的关系

- 迭代 2 清单的 C1/B2/B3/E1/E2 让位于本方案（Wave 1-4 优先）
- §8.3 尾款中「告警规则/systemd」并入 Wave 4 或紧随其后
- 修复完成后：第三波审计的十项修复序全部关闭或显式裁决，出新版自审对照
